#include "mcStreamAll.h"

typedef struct StreamTeeStruct StreamTeeRecord, *StreamTee;
typedef struct StreamTeeStruct StreamDefaultTeeRecord, *StreamDefaultTee;
typedef struct StreamTeeStruct ByteStreamTeeRecord, *ByteStreamTee;

struct StreamTeeStruct {
	StreamHandlePart;
	ReadableStream* stream;
	ReadableStreamReader* reader;
	xsSlot* cancelPromise;
	xsSlot* chunkStepsTask;
	ReadableStreamController* controllers[2];
	xsSlot* reasons[2];
	txU1 canceled[2];
	txU1 readAgain[2];
	txU1 reading;
};

static void StreamTee_destructor(void* it);
static void StreamTee_mark(xsMachine* the, void* it, xsMarkRoot markRoot);

static txBoolean StreamTeeCancelAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* reason, xsSlot* result);
static txBoolean StreamTeeStartAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* param, xsSlot* result);
static void StreamTeeReaderChunkSteps(xsMachine* the, ReadableStreamReader* reader, xsSlot* request, xsSlot* chunk);
static void StreamTeeReaderErrorSteps(xsMachine* the, ReadableStreamReader* reader, xsSlot* request, xsSlot* error);

static void ReadableStreamDefaultTee(xsMachine* the);
static txBoolean StreamDefaultTeePullAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* param, xsSlot* result);
static void StreamDefaultTeeReaderChunkStepsTask(xsMachine* the);
static void StreamDefaultTeeReaderCloseSteps(xsMachine* the, ReadableStreamReader* reader, xsSlot* request, xsSlot* chunk);
static void StreamDefaultTeeReaderRejected(xsMachine* the);

static void ReadableByteStreamTee(xsMachine* the);
static void CloneAsUint8Array(xsMachine* the, xsSlot* view, xsSlot* clonedChunk);
static void ByteStreamTeeUseBYOBReader(xsMachine* the, ByteStreamTee* tee);
static void ByteStreamTeeUseDefaultReader(xsMachine* the, ByteStreamTee* tee);
static txBoolean ByteStreamTeePullAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* param, xsSlot* result);
static void ByteStreamTeeBYOBReaderChunkStepsTask(xsMachine* the);
static void ByteStreamTeeBYOBReaderCloseSteps(xsMachine* the, ReadableStreamReader* reader, xsSlot* request, xsSlot* chunk);
static void ByteStreamTeeDefaultReaderChunkStepsTask(xsMachine* the);
static void ByteStreamTeeDefaultReaderCloseSteps(xsMachine* the, ReadableStreamReader* reader, xsSlot* request, xsSlot* chunk);
static void ByteStreamTeeReaderRejected(xsMachine* the);

static const xsHostHooks StreamTeeHooks = {
	StreamTee_destructor,
	StreamTee_mark,
	NULL
};
void StreamTee_destructor(void* it)
{
}
void StreamTee_mark(xsMachine* the, void* it, xsMarkRoot markRoot)
{
	StreamTee self = it;
	StreamMarkHandle(the, self->stream);
	StreamMarkHandle(the, self->reader);
	StreamMarkReference(the, self->cancelPromise);
	StreamMarkReference(the, self->chunkStepsTask);
	StreamMarkHandle(the, self->controllers[0]);
	StreamMarkHandle(the, self->controllers[1]);
}

txBoolean StreamTeeCancelAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* reason, xsSlot* result)
{
	ReadableStreamController* controller = (ReadableStreamController*)stuff;
	StreamTee* tee = (StreamTee*)fxStreamHandle(the, (*controller)->target, NULL);
	uint8_t branch = (*controller)->branch;
	(*tee)->canceled[branch] = 1;
	(*tee)->reasons[branch]->kind = reason->kind;
	(*tee)->reasons[branch]->value = reason->value;
	if ((*tee)->canceled[branch ^ 1]) {
		fxNewArray(the, 2);
		txSlot* compositeReason = the->stack;
		for (branch = 0; branch < 2; branch++) {
			mxPushSlot((*tee)->reasons[branch]);
			mxPushSlot(compositeReason);
			mxSetIndex(branch);
			mxPop();
		}
		ReadableStreamCancel(the, (*tee)->stream, compositeReason, result);
		mxPop(); // compositeReason
		fxResolvePromiseRecord(the, (*tee)->cancelPromise, result);
	}
	fxGetPromiseRecordPromise(the, (*tee)->cancelPromise, result);
	return 1;
}
txBoolean StreamTeeStartAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* param, xsSlot* result)
{
	return 1;
}
void StreamTeeReaderChunkSteps(xsMachine* the, ReadableStreamReader* reader, xsSlot* request, xsSlot* chunk)
{
	StreamTee* tee = (StreamTee*)(*reader)->context;
	mxPushUndefined();
	mxPushReference((*tee)->chunkStepsTask);
	mxCall();	
	mxPushSlot(request);
	mxPushSlot(chunk);
	fxQueueJob(the, 2, C_NULL);
}
void StreamTeeReaderErrorSteps(xsMachine* the, ReadableStreamReader* reader, xsSlot* request, xsSlot* error)
{
	StreamTee* tee = (StreamTee*)(*reader)->context;
	(*tee)->reading = 0;
}

void ReadableStreamDefaultTee(xsMachine* the)
{
	xsSlot* teeInstance = fxNewHostObject(the, NULL);
	xsSlot* teeReference = the->stack;
	fxSetHostChunk(the, teeReference, NULL, sizeof(StreamTeeRecord));
	fxSetHostHooks(the, teeReference, (xsHostHooks*)&StreamTeeHooks);
	StreamDefaultTee* tee = (StreamDefaultTee*)fxGetHostHandle(the, teeReference);
	(*tee)->reference = teeInstance;

	ReadableStream* stream = (*tee)->stream = mxStreamHandle(ReadableStream, mxThis);
	(*tee)->cancelPromise = fxCreatePromiseRecord(the, NULL);

	fxNewArray(the, 2);
	mxPullSlot(mxResult);
	uint8_t branch;
	for (branch = 0; branch < 2; branch++) {
		mxPushUndefined();
		ReadableStream* branchStream = CreateReadableStream(the, the->stack, (*stream)->closures);
		mxPushSlot(mxResult);
		mxSetIndex(branch);
		mxPop();
		
		ReadableStreamController* branchController = (*tee)->controllers[branch] = (ReadableStreamController*)CreateReadableStreamDefaultController(the, branchStream);
		(*branchController)->strategyHWM = 1;
		(*branchController)->target = teeInstance;
		(*branchController)->startAlgorithm.call = StreamTeeStartAlgorithm;
		(*branchController)->pullAlgorithm.call = StreamDefaultTeePullAlgorithm;
		(*branchController)->cancelAlgorithm.call = StreamTeeCancelAlgorithm;
		(*branchController)->branch = branch;
		
		txSlot* slot = fxLastProperty(the, teeInstance);
		slot = slot->next = fxNewSlot(the);
		slot->flag |= XS_INTERNAL_FLAG;
		slot->kind = XS_UNINITIALIZED_KIND;
		(*tee)->reasons[branch] = slot;
	}
	
	(*tee)->chunkStepsTask = fxNewHostFunctionWithHandle(the, tee, StreamDefaultTeeReaderChunkStepsTask, 1); mxPop();
	ReadableStreamReader* reader = (*tee)->reader = (ReadableStreamReader*)AcquireReadableStreamDefaultReader(the, (*tee)->stream);
	(*reader)->context = tee;
	(*reader)->chunkSteps = StreamTeeReaderChunkSteps;
	(*reader)->closeSteps = StreamDefaultTeeReaderCloseSteps;
	(*reader)->errorSteps = StreamTeeReaderErrorSteps;
	xsSlot* promise;
	mxTemporary(promise);
	fxGetPromiseRecordPromise(the, (*reader)->closedPromise, promise);
	xsSlot* rejected = fxNewHostFunctionWithHandle(the, reader, StreamDefaultTeeReaderRejected, 1);
	fxChainPromise(the, promise, NULL, rejected, NULL);
	mxPop(); // rejected
	mxPop(); // promise

	ReadableStreamDefaultControllerStart(the, (ReadableStreamDefaultController*)(*tee)->controllers[0]);
	ReadableStreamDefaultControllerStart(the, (ReadableStreamDefaultController*)(*tee)->controllers[1]);
}

txBoolean StreamDefaultTeePullAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* param, xsSlot* result)
{
	ReadableStreamController* controller = (ReadableStreamController*)stuff;
	StreamDefaultTee* tee = (StreamDefaultTee*)fxStreamHandle(the, (*controller)->target, NULL);
	uint8_t branch = (*controller)->branch;
	if ((*tee)->reading) {
		(*tee)->readAgain[branch] = 1;
	}
	else {
		(*tee)->reading = 1;
		mxPushInteger(branch);
		ReadableStreamDefaultReaderRead(the, (ReadableStreamDefaultReader*)(*tee)->reader, the->stack);
		mxPop();
	}
	return 1;
}
void StreamDefaultTeeReaderChunkStepsTask(xsMachine* the)
{
	StreamDefaultTee* tee = (StreamDefaultTee*)fxGetHostFunctionHandle(the);
	xsSlot* chunk = mxArgv(1);
	(*tee)->readAgain[0] = 0;
	(*tee)->readAgain[1] = 0;
	if (!(*tee)->canceled[0])
		ReadableStreamDefaultControllerEnqueue(the, (ReadableStreamDefaultController*)(*tee)->controllers[0], chunk);
	if (!(*tee)->canceled[1])
		ReadableStreamDefaultControllerEnqueue(the, (ReadableStreamDefaultController*)(*tee)->controllers[1], chunk);
	(*tee)->reading = 0;
	if ((*tee)->readAgain[0])
		StreamDefaultTeePullAlgorithm(the, (StreamStuff*)(*tee)->controllers[0], NULL, NULL);
	else if ((*tee)->readAgain[1])
		StreamDefaultTeePullAlgorithm(the, (StreamStuff*)(*tee)->controllers[1], NULL, NULL);
}
void StreamDefaultTeeReaderCloseSteps(xsMachine* the, ReadableStreamReader* reader, xsSlot* request, xsSlot* chunk)
{
	StreamDefaultTee* tee = (StreamDefaultTee*)(*reader)->context;
	(*tee)->reading = 0;
	if (!(*tee)->canceled[0])
		ReadableStreamDefaultControllerClose(the, (ReadableStreamDefaultController*)(*tee)->controllers[0]);
	if (!(*tee)->canceled[1])
		ReadableStreamDefaultControllerClose(the, (ReadableStreamDefaultController*)(*tee)->controllers[1]);
	if (!(*tee)->canceled[0] || !(*tee)->canceled[1])
		fxResolvePromiseRecord(the, (*tee)->cancelPromise, &mxUndefined);
}

void StreamDefaultTeeReaderRejected(xsMachine* the)
{
	ReadableStreamReader* reader = (ReadableStreamReader*)fxGetHostFunctionHandle(the);
	StreamDefaultTee* tee = (StreamDefaultTee*)(*reader)->context;
	if (tee) {
		ReadableStreamDefaultControllerError(the, (ReadableStreamDefaultController*)(*tee)->controllers[0], mxArgv(0));
		ReadableStreamDefaultControllerError(the, (ReadableStreamDefaultController*)(*tee)->controllers[1], mxArgv(0));
		if (!(*tee)->canceled[0] || !(*tee)->canceled[1])
			fxResolvePromiseRecord(the, (*tee)->cancelPromise, &mxUndefined);
	}
}

void ReadableByteStreamTee(xsMachine* the)
{
	xsSlot* teeInstance = fxNewHostObject(the, NULL);
	xsSlot* teeReference = the->stack;
	fxSetHostChunk(the, teeReference, NULL, sizeof(StreamTeeRecord));
	fxSetHostHooks(the, teeReference, (xsHostHooks*)&StreamTeeHooks);
	ByteStreamTee* tee = (ByteStreamTee*)fxGetHostHandle(the, teeReference);
	(*tee)->reference = teeInstance;

	ReadableStream* stream = (*tee)->stream = mxStreamHandle(ReadableStream, mxThis);
	(*tee)->cancelPromise = fxCreatePromiseRecord(the, NULL);
	
	fxNewArray(the, 2);
	mxPullSlot(mxResult);
	uint8_t branch;
	for (branch = 0; branch < 2; branch++) {
		mxPushUndefined();
		ReadableStream* branchStream = CreateReadableStream(the, the->stack, (*stream)->closures);
		mxPushSlot(mxResult);
		mxSetIndex(branch);
		mxPop();
		
		ReadableStreamController* branchController = (*tee)->controllers[branch] = (ReadableStreamController*)CreateReadableByteStreamController(the, branchStream);
		(*branchController)->target = teeInstance;
		(*branchController)->startAlgorithm.call = StreamTeeStartAlgorithm;
		(*branchController)->pullAlgorithm.call = ByteStreamTeePullAlgorithm;
		(*branchController)->cancelAlgorithm.call = StreamTeeCancelAlgorithm;
		(*branchController)->branch = branch;
		
		txSlot* slot = fxLastProperty(the, teeInstance);
		slot = slot->next = fxNewSlot(the);
		slot->flag |= XS_INTERNAL_FLAG;
		slot->kind = XS_UNINITIALIZED_KIND;
		(*tee)->reasons[branch] = slot;
	}
	
	ByteStreamTeeUseDefaultReader(the, tee);

	ReadableByteStreamControllerStart(the, (ReadableByteStreamController*)(*tee)->controllers[0]);
	ReadableByteStreamControllerStart(the, (ReadableByteStreamController*)(*tee)->controllers[1]);
}

void CloneAsUint8Array(xsMachine* the, xsSlot* view, xsSlot* clonedChunk)
{
	txSize byteOffset, byteLength;
	xsSlot* buffer = NULL;
	if (view->kind == XS_REFERENCE_KIND) {
		txSlot* slot = view->value.reference->next;
		if (slot && (slot->flag & XS_INTERNAL_FLAG)) {
			if (slot->kind == XS_TYPED_ARRAY_KIND)
				slot = slot->next;
			if (slot->kind == XS_DATA_VIEW_KIND) {
				byteOffset = slot->value.dataView.offset;
				byteLength = slot->value.dataView.size;
				slot = slot->next;
				if (slot->kind == XS_REFERENCE_KIND) {
					slot = slot->value.reference->next;
					if (slot && (slot->flag & XS_INTERNAL_FLAG)) {
						if (slot->kind == XS_ARRAY_BUFFER_KIND) {
							buffer = slot;
						}
					}
				}
			}
		}
	}
	if (!buffer)
		mxTypeError("not a view");
	txByte* address = fxArrayBuffer(the, clonedChunk, NULL, byteLength, -1);
	c_memcpy(address, buffer->value.arrayBuffer.address + byteOffset, byteLength);
	mxPush(mxUint8ArrayConstructor);
	mxNew();
	mxPushSlot(clonedChunk);
	mxRunCount(1);
	mxPullSlot(clonedChunk);
}

void ByteStreamTeeUseBYOBReader(xsMachine* the, ByteStreamTee* tee)
{
	ReadableStreamReader* reader = (*tee)->reader;
	if (reader) {
		(*reader)->context = NULL;
		(*tee)->reader = NULL;
		ReadableStreamReaderGenericRelease(the, reader);
	}
	(*tee)->chunkStepsTask = fxNewHostFunctionWithHandle(the, tee, ByteStreamTeeBYOBReaderChunkStepsTask, 1); mxPop();
	reader = (*tee)->reader = (ReadableStreamReader*)AcquireReadableStreamBYOBReader(the, (*tee)->stream);
	(*reader)->context = tee;
	(*reader)->chunkSteps = StreamTeeReaderChunkSteps;
	(*reader)->closeSteps = ByteStreamTeeBYOBReaderCloseSteps;
	(*reader)->errorSteps = StreamTeeReaderErrorSteps;
	xsSlot* promise;
	mxTemporary(promise);
	fxGetPromiseRecordPromise(the, (*reader)->closedPromise, promise);
	xsSlot* rejected = fxNewHostFunctionWithHandle(the, reader, ByteStreamTeeReaderRejected, 1);
	fxChainPromise(the, promise, NULL, rejected, NULL);
	mxPop(); // rejected
	mxPop(); // promise
}
void ByteStreamTeeUseDefaultReader(xsMachine* the, ByteStreamTee* tee)
{
	ReadableStreamReader* reader = (*tee)->reader;
	if (reader) {
		(*reader)->context = NULL;
		(*tee)->reader = NULL;
		ReadableStreamReaderGenericRelease(the, reader);
	}
	(*tee)->chunkStepsTask = fxNewHostFunctionWithHandle(the, tee, ByteStreamTeeDefaultReaderChunkStepsTask, 1); mxPop();
	reader = (*tee)->reader = (ReadableStreamReader*)AcquireReadableStreamDefaultReader(the, (*tee)->stream);
	(*reader)->context = tee;
	(*reader)->chunkSteps = StreamTeeReaderChunkSteps;
	(*reader)->closeSteps = ByteStreamTeeDefaultReaderCloseSteps;
	(*reader)->errorSteps = StreamTeeReaderErrorSteps;
	xsSlot* promise;
	mxTemporary(promise);
	fxGetPromiseRecordPromise(the, (*reader)->closedPromise, promise);
	xsSlot* rejected = fxNewHostFunctionWithHandle(the, reader, ByteStreamTeeReaderRejected, 1);
	fxChainPromise(the, promise, NULL, rejected, NULL);
	mxPop(); // rejected
	mxPop(); // promise
}

txBoolean ByteStreamTeePullAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* param, xsSlot* result)
{
	ReadableByteStreamController* controller = (ReadableByteStreamController*)stuff;
	ByteStreamTee* tee = (ByteStreamTee*)fxStreamHandle(the, (*controller)->target, NULL);
	uint8_t branch = (*controller)->branch;
	if ((*tee)->reading) {
		(*tee)->readAgain[branch] = 1;
	}
	else {
		(*tee)->reading = 1;
		controller = (ReadableByteStreamController*)(*tee)->controllers[branch];
		ReadableByteStreamControllerGetBYOBRequest(the, controller);
		ReadableStreamBYOBRequest* byobRequest = (*controller)->byobRequest;
		if (byobRequest) {
			if (IsReadableStreamDefaultReader(the, (*tee)->reader)) {
				ByteStreamTeeUseBYOBReader(the, tee);
			}
			mxPushReference((*byobRequest)->view);
			mxPushInteger(branch);
			ReadableStreamBYOBReaderRead(the, (ReadableStreamBYOBReader*)(*tee)->reader, the->stack + 1, 1, the->stack);
			mxPop();
			mxPop();
		}
		else {
			if (IsReadableStreamBYOBReader(the, (*tee)->reader)) {
				ByteStreamTeeUseDefaultReader(the, tee);
			}
			mxPushInteger(branch);
			ReadableStreamDefaultReaderRead(the, (ReadableStreamDefaultReader*)(*tee)->reader, the->stack);
			mxPop();
		}
	}
	return 1;
}

void ByteStreamTeeBYOBReaderChunkStepsTask(xsMachine* the)
{
	ByteStreamTee* tee = (ByteStreamTee*)fxGetHostFunctionHandle(the);
	uint8_t which = (uint8_t)fxToInteger(the, mxArgv(0));
	uint8_t other = which ^ 1;
	xsSlot* chunk = mxArgv(1);
	(*tee)->readAgain[0] = 0;
	(*tee)->readAgain[1] = 0;
	txU1 whichCanceled = (*tee)->canceled[which];
	ReadableByteStreamController* whichController = (ReadableByteStreamController*)(*tee)->controllers[which];
	txU1 otherCanceled = (*tee)->canceled[other];
	ReadableByteStreamController* otherController = (ReadableByteStreamController*)(*tee)->controllers[other];
	if (!otherCanceled) {
		xsSlot* clonedChunk;
		mxTemporary(clonedChunk);
		mxTry(the) {
			CloneAsUint8Array(the, chunk, clonedChunk);
		}
		mxCatch(the) {
			mxPush(mxException);
			mxException = mxUndefined;
			ReadableByteStreamControllerError(the, whichController, the->stack);
			ReadableByteStreamControllerError(the, otherController, the->stack);
			ReadableStreamCancel(the, (*tee)->stream, the->stack, the->stack);
			fxResolvePromiseRecord(the, (*tee)->cancelPromise, the->stack);
			return;
		}
		if (!whichCanceled)
			ReadableByteStreamControllerRespondWithNewView(the, whichController, chunk);
		ReadableByteStreamControllerEnqueue(the, otherController, clonedChunk);
		mxPop(); // clonedChunk
	}
	else if (!whichCanceled)
		ReadableByteStreamControllerRespondWithNewView(the, whichController, chunk);
	(*tee)->reading = 0;
	if ((*tee)->readAgain[0])
		ByteStreamTeePullAlgorithm(the, (StreamStuff*)(*tee)->controllers[0], NULL, NULL);
	else if ((*tee)->readAgain[1])
		ByteStreamTeePullAlgorithm(the, (StreamStuff*)(*tee)->controllers[1], NULL, NULL);
}
void ByteStreamTeeBYOBReaderCloseSteps(xsMachine* the, ReadableStreamReader* reader, xsSlot* request, xsSlot* chunk)
{
	ByteStreamTee* tee = (ByteStreamTee*)(*reader)->context;
	uint8_t which = (uint8_t)fxToInteger(the, request);
	uint8_t other = which ^ 1;
	txU1 whichCanceled = (*tee)->canceled[which];
	ReadableByteStreamController* whichController = (ReadableByteStreamController*)(*tee)->controllers[which];
	txU1 otherCanceled = (*tee)->canceled[other];
	ReadableByteStreamController* otherController = (ReadableByteStreamController*)(*tee)->controllers[other];
	(*tee)->reading = 0;
	if (!whichCanceled)
		ReadableByteStreamControllerClose(the, whichController);
	if (!otherCanceled)
		ReadableByteStreamControllerClose(the, otherController);
	if (chunk) {
		if (!whichCanceled) {
			ReadableByteStreamControllerRespondWithNewView(the, whichController, chunk);
		if (!otherCanceled && (GetSlotQueueLength(the, (*otherController)->pendingPullIntos) > 0))
			ReadableByteStreamControllerRespond(the, otherController, 0);
	}
	if (!(*tee)->canceled[0] || !(*tee)->canceled[1])
		fxResolvePromiseRecord(the, (*tee)->cancelPromise, &mxUndefined);
	}
}

void ByteStreamTeeDefaultReaderChunkStepsTask(xsMachine* the)
{
	ByteStreamTee* tee = (ByteStreamTee*)fxGetHostFunctionHandle(the);
	(*tee)->readAgain[0] = 0;
	(*tee)->readAgain[1] = 0;
	xsSlot* chunk = mxArgv(1);
	xsSlot* chunk1 = chunk;	
	xsSlot* chunk2 = chunk;
	xsSlot* clonedChunk;
	mxTemporary(clonedChunk);
	if (!(*tee)->canceled[0] && !(*tee)->canceled[1]) {
		mxTry(the) {
			CloneAsUint8Array(the, chunk, clonedChunk);
			chunk2 = clonedChunk;
		}
		mxCatch(the) {
			mxPush(mxException);
			mxException = mxUndefined;
			ReadableByteStreamControllerError(the, (ReadableByteStreamController*)(*tee)->controllers[0], the->stack);
			ReadableByteStreamControllerError(the, (ReadableByteStreamController*)(*tee)->controllers[1], the->stack);
			ReadableStreamCancel(the, (*tee)->stream, the->stack, the->stack);
			fxResolvePromiseRecord(the, (*tee)->cancelPromise, the->stack);
			return;
		}
	}
	if (!(*tee)->canceled[0])
		ReadableByteStreamControllerEnqueue(the, (ReadableByteStreamController*)(*tee)->controllers[0], chunk1);
	if (!(*tee)->canceled[1])
		ReadableByteStreamControllerEnqueue(the, (ReadableByteStreamController*)(*tee)->controllers[1], chunk2);
	mxPop(); // clonedChunk
	(*tee)->reading = 0;
	if ((*tee)->readAgain[0])
		ByteStreamTeePullAlgorithm(the, (StreamStuff*)(*tee)->controllers[0], NULL, NULL);
	else if ((*tee)->readAgain[1])
		ByteStreamTeePullAlgorithm(the, (StreamStuff*)(*tee)->controllers[1], NULL, NULL);
}
void ByteStreamTeeDefaultReaderCloseSteps(xsMachine* the, ReadableStreamReader* reader, xsSlot* request, xsSlot* chunk)
{
	ByteStreamTee* tee = (ByteStreamTee*)(*reader)->context;
	(*tee)->reading = 0;
	uint8_t branch;
	for (branch = 0; branch < 2; branch++) {
		if (!(*tee)->canceled[branch])
			ReadableByteStreamControllerClose(the, (ReadableByteStreamController*)(*tee)->controllers[branch]);
	}
	for (branch = 0; branch < 2; branch++) {
		ReadableByteStreamController* controller = (ReadableByteStreamController*)(*tee)->controllers[branch];
		if (GetSlotQueueLength(the, (*controller)->pendingPullIntos) > 0)
			ReadableByteStreamControllerRespond(the, controller, 0);
	}
	if (!(*tee)->canceled[0] || !(*tee)->canceled[1])
		fxResolvePromiseRecord(the, (*tee)->cancelPromise, &mxUndefined);
}

void ByteStreamTeeReaderRejected(xsMachine* the)
{
	ReadableStreamReader* reader = (ReadableStreamReader*)fxGetHostFunctionHandle(the);
	ByteStreamTee* tee = (ByteStreamTee*)(*reader)->context;
	if (tee) {
		ReadableByteStreamControllerError(the, (ReadableByteStreamController*)(*tee)->controllers[0], mxArgv(0));
		ReadableByteStreamControllerError(the, (ReadableByteStreamController*)(*tee)->controllers[1], mxArgv(0));
		if (!(*tee)->canceled[0] || !(*tee)->canceled[1])
			fxResolvePromiseRecord(the, (*tee)->cancelPromise, &mxUndefined);
	}
}

void ReadableStream_tee(xsMachine* the)
{
	ReadableStream* stream = mxStreamHandle(ReadableStream, mxThis);
	if (IsReadableByteStreamController(the, (*stream)->controller))
		ReadableByteStreamTee(the);
	else
		ReadableStreamDefaultTee(the);
}

