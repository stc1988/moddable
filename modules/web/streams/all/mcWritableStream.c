#include "mcStreamAll.h"

static txBoolean WritableStreamHasOperationMarkedInFlight(xsMachine* the, WritableStream* stream);
static txBoolean WritableStreamDefaultSinkAbortAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* reason, xsSlot* result);
static txBoolean WritableStreamDefaultSinkCloseAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* param, xsSlot* result);
static txBoolean WritableStreamDefaultSinkStartAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* param, xsSlot* result);
static txBoolean WritableStreamDefaultSinkWriteAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* chunk, xsSlot* result);

const StreamDispatchRecord WritableStreamDispatchRecord = {
	"WritableStream",
};

static void WritableStream_mark(xsMachine* the, void* it, xsMarkRoot markRoot);
static const xsHostHooks WritableStreamHooks ICACHE_RODATA_ATTR = {
	WritableStream_destructor,
	WritableStream_mark,
	NULL
};

// 5.2 WritableStream
void buildWritableStream(xsMachine* the)
{
	WritableStream* stream = InitializeWritableStream(the, mxThis, fxToReference(the, mxArgv(2)));
	WritableStreamDefaultController* controller = CreateWritableStreamDefaultController(the, stream);
	xsSlot* underlyingSink = mxArgv(0);
	xsSlot* strategy = mxArgv(1);
	
	(*controller)->strategySizeAlgorithm = ExtractSizeAlgorithm(the, strategy);
	(*controller)->strategyHWM = ExtractHighWaterMark(the, strategy, 1);

	(*controller)->target = fxToReference(the, underlyingSink);
	(*controller)->startAlgorithm.call = WritableStreamDefaultSinkStartAlgorithm;
	(*controller)->startAlgorithm.callback  = ExtractAlgorithmReference(the, underlyingSink, xsID_start);
	(*controller)->writeAlgorithm.call = WritableStreamDefaultSinkWriteAlgorithm;
	(*controller)->writeAlgorithm.callback = ExtractAlgorithmReference(the, underlyingSink, xsID_write);
	(*controller)->closeAlgorithm.call = WritableStreamDefaultSinkCloseAlgorithm;
	(*controller)->closeAlgorithm.callback = ExtractAlgorithmReference(the, underlyingSink, xsID_close);
	(*controller)->abortAlgorithm.call = WritableStreamDefaultSinkAbortAlgorithm;
	(*controller)->abortAlgorithm.callback = ExtractAlgorithmReference(the, underlyingSink, xsID_abort);
	
	WritableStreamDefaultControllerStart(the, controller);
}
void WritableStream_destructor(void* it)
{
}
void WritableStream_mark(xsMachine* the, void* it, xsMarkRoot markRoot)
{
	WritableStream stream = it;
	StreamMarkReference(the, stream->closures);
	StreamMarkHandle(the, stream->controller);
	StreamMarkHandle(the, stream->writer);
	StreamMarkReference(the, stream->closeRequest);
	StreamMarkReference(the, stream->inFlightCloseRequest);
	StreamMarkReference(the, stream->inFlightWriteRequest);
	StreamMarkReference(the, stream->pendingAbortRequest);
	StreamMarkReference(the, stream->pendingAbortRequestReason);
	StreamMarkReference(the, stream->writeRequests);
}
void WritableStream_get_locked(xsMachine* the)
{
	WritableStream* stream = mxStreamHandle(WritableStream, mxThis);
	xsResult = (IsWritableStreamLocked(the, stream)) ? xsTrue : xsFalse;
}
void WritableStream_abort(xsMachine* the)
{
	WritableStream* stream = mxStreamHandle(WritableStream, mxThis);
	if (IsWritableStreamLocked(the, stream)) {
		mxReturnPromiseRejectedWithTypeError("stream locked");
	}
	if (mxArgc > 0)
		mxPushSlot(mxArgv(0));
	else
		mxPushUndefined();
	WritableStreamAbort(the, stream, the->stack, mxResult);
	mxPop();
}
void WritableStream_close(xsMachine* the)
{
	WritableStream* stream = mxStreamHandle(WritableStream, mxThis);
	if (IsWritableStreamLocked(the, stream)) {
		mxReturnPromiseRejectedWithTypeError("stream locked");
	}
	if (WritableStreamCloseQueuedOrInFlight(the, stream)) {
		mxReturnPromiseRejectedWithTypeError("stream closed");
	}
	WritableStreamClose(the, stream, mxResult);
}

// 5.5.1 Working with writable streams
WritableStream* CreateWritableStream(xsMachine* the, xsSlot* it, xsSlot* closures)
{
	mxPushReference(closures);
	mxGetID(xsID_writableStream);
	fxNewHostInstance(the);
	mxPullSlot(it);
	return InitializeWritableStream(the, it, closures);
}
WritableStream* InitializeWritableStream(xsMachine* the, xsSlot* it, xsSlot* closures)
{
	fxSetHostChunk(the, it, NULL, sizeof(WritableStreamRecord));
	fxSetHostHooks(the, it, (xsHostHooks*)&WritableStreamHooks);
	WritableStream* stream = (WritableStream*)fxGetHostHandle(the, it);
	(*stream)->reference = fxToReference(the, it);
	(*stream)->dispatch = (StreamDispatch)&WritableStreamDispatchRecord;
	(*stream)->closures = closures;
	// storedError can be any value...
	txSlot* slot = fxLastProperty(the, (*stream)->reference);
	slot = slot->next = fxNewSlot(the);
	slot->flag |= XS_INTERNAL_FLAG;
	slot->kind = XS_UNINITIALIZED_KIND;
	(*stream)->storedError = slot;
	(*stream)->writeRequests = CreateSlotQueue(the);
	return stream;
}

txBoolean IsWritableStreamLocked(xsMachine* the, WritableStream* stream)
{
	return ((*stream)->writer) ? 1 : 0;
}
void WritableStreamAbort(xsMachine* the, WritableStream* stream, txSlot* reason, txSlot* promise)
{
	uint8_t state = (*stream)->state;
	if ((state == mcStreamClosed) || (state == mcStreamErrored)) {
		fxCreateResolvedPromise(the, &mxUndefined, promise);
		return;
	}
	
	WritableStreamDefaultController* controller = (*stream)->controller;
	mxPushReference((*controller)->abortController);
	mxDub();
	mxGetID(xsID_abort);
	mxCall();
	mxPushSlot(reason);
	mxRunCount(1);
	mxPop();
	
	if ((state == mcStreamClosed) || (state == mcStreamErrored)) {
		fxCreateResolvedPromise(the, &mxUndefined, promise);
		return;
	}
	if ((*stream)->pendingAbortRequest) {
		fxGetPromiseRecordPromise(the, (*stream)->pendingAbortRequest, promise);
		return;
	}
	mxStreamAssert((state == mcStreamWritable) || (state == mcStreamErroring));
	(*stream)->pendingAbortRequest = fxCreatePromiseRecord(the, NULL);
	if (state == mcStreamErroring) {
		(*stream)->pendingAbortRequestReason = NULL;
		(*stream)->pendingAbortRequestWasAlreadyErroring = 1;
	}
	else {
// 		(*stream)->pendingAbortRequestReason = fxToReference(the, reason);
		(*stream)->pendingAbortRequestWasAlreadyErroring = 0;
		WritableStreamStartErroring(the, stream, reason);
	}
	fxGetPromiseRecordPromise(the, (*stream)->pendingAbortRequest, promise);
}
void WritableStreamClose(xsMachine* the, WritableStream* stream, txSlot* promise)
{
	uint8_t state = (*stream)->state;
	if ((state == mcStreamClosed) || (state == mcStreamErrored)) {
		mxPush(mxTypeErrorConstructor);
		fxNewError(the, "stream already closed or errored");
		fxCreateRejectedPromise(the, the->stack, promise);
		return;
	}
	mxStreamAssert((state == mcStreamWritable) || (state == mcStreamErroring));
	(*stream)->closeRequest = fxCreatePromiseRecord(the, NULL);
	fxGetPromiseRecordPromise(the, (*stream)->closeRequest, promise);
	WritableStreamDefaultWriter* writer = (*stream)->writer;
	if (writer && (*stream)->backpressure && (state == mcStreamWritable))
		fxResolvePromiseRecord(the, (*writer)->readyPromise, &mxUndefined);
	WritableStreamDefaultControllerClose(the, (*stream)->controller);
}

// 5.5.2 Interfacing with controllers
txSlot* WritableStreamAddWriteRequest(xsMachine* the, WritableStream* stream)
{
	mxStreamAssert(IsWritableStreamLocked(the, stream));
	uint8_t state = (*stream)->state;
	mxStreamAssert(state == mcStreamWritable);
	txSlot* record;
	mxTemporary(record);
	txSlot* writeRequest = fxCreatePromiseRecord(the, record);
	EnqueueSlot(the, (*stream)->writeRequests, record);
	mxPop();
	return writeRequest;
}
txBoolean WritableStreamCloseQueuedOrInFlight(xsMachine* the, WritableStream* stream)
{
	return (((*stream)->closeRequest) || ((*stream)->inFlightCloseRequest)) ? 1 : 0;
}
void WritableStreamDealWithRejection(xsMachine* the, WritableStream* stream, txSlot* error)
{
	uint8_t state = (*stream)->state;
	if (state == mcStreamWritable) {
		WritableStreamStartErroring(the, stream, error);
		return;
	}
	mxStreamAssert(state == mcStreamErroring);
	WritableStreamFinishErroring(the, stream);
}
void WritableStreamFinishErroring(xsMachine* the, WritableStream* stream)
{
	uint8_t state = (*stream)->state;
	mxStreamAssert(state == mcStreamErroring);
	(*stream)->state = mcStreamErrored;
	WritableStreamDefaultController* controller = (*stream)->controller;
	ResetQueue(the, (*controller)->queue);
	xsSlot* writeRequest;
	mxTemporary(writeRequest);
	while (GetSlotQueueLength(the, (*stream)->writeRequests) > 0) {
		DequeueSlot(the, (*stream)->writeRequests, writeRequest);
		fxRejectPromiseRecord(the, writeRequest->value.reference, (*stream)->storedError);
	}
	mxPop();
	
	txSlot* pendingAbortRequest = (*stream)->pendingAbortRequest;
	if (!pendingAbortRequest) {
		WritableStreamRejectCloseAndClosedPromiseIfNeeded(the, stream);
		return;
	}
	if ((*stream)->pendingAbortRequestWasAlreadyErroring) {
		(*stream)->pendingAbortRequest = NULL;
		(*stream)->pendingAbortRequestWasAlreadyErroring = 0;
		fxRejectPromiseRecord(the, pendingAbortRequest, (*stream)->storedError);
		WritableStreamRejectCloseAndClosedPromiseIfNeeded(the, stream);
		return;
	}
	WritableStreamDefaultControllerProcessAbort(the, controller, (*stream)->storedError);
}
void WritableStreamFinishInFlightClose(xsMachine* the, WritableStream* stream)
{
	mxStreamAssert((*stream)->inFlightCloseRequest);
	fxResolvePromiseRecord(the, (*stream)->inFlightCloseRequest, &mxUndefined);
	(*stream)->inFlightCloseRequest = NULL;
	uint8_t state = (*stream)->state;
	mxStreamAssert((state == mcStreamWritable) || (state == mcStreamErroring));
	if (state == mcStreamErroring) {
		(*stream)->storedError->kind = XS_UNINITIALIZED_KIND;
		if ((*stream)->pendingAbortRequest) {
			fxResolvePromiseRecord(the, (*stream)->pendingAbortRequest, &mxUndefined);
			(*stream)->pendingAbortRequest = NULL;
		}
	}
	(*stream)->state = mcStreamClosed;
	WritableStreamDefaultWriter* writer = (*stream)->writer;
	if (writer)
		fxResolvePromiseRecord(the, (*writer)->closedPromise, &mxUndefined);
	mxStreamAssert((*stream)->pendingAbortRequest == NULL);
	mxStreamAssert((*stream)->storedError->kind == XS_UNINITIALIZED_KIND);
}
void WritableStreamFinishInFlightCloseWithError(xsMachine* the, WritableStream* stream, txSlot* error)
{
	mxStreamAssert((*stream)->inFlightCloseRequest);
	fxRejectPromiseRecord(the, (*stream)->inFlightCloseRequest, error);
	(*stream)->inFlightCloseRequest = NULL;
	uint8_t state = (*stream)->state;
	mxStreamAssert((state == mcStreamWritable) || (state == mcStreamErroring));
	if ((*stream)->pendingAbortRequest) {
		fxRejectPromiseRecord(the, (*stream)->pendingAbortRequest, error);
		(*stream)->pendingAbortRequest = NULL;
	}
	WritableStreamDealWithRejection(the, stream, error);
}
void WritableStreamFinishInFlightWrite(xsMachine* the, WritableStream* stream)
{
	xsSlot* inFlightWriteRequest = (*stream)->inFlightWriteRequest;
	mxStreamAssert(inFlightWriteRequest);
	(*stream)->inFlightWriteRequest = NULL;
	fxResolvePromiseRecord(the, inFlightWriteRequest, &mxUndefined);
}
void WritableStreamFinishInFlightWriteWithError(xsMachine* the, WritableStream* stream, txSlot* error)
{
	mxStreamAssert((*stream)->inFlightWriteRequest);
	fxRejectPromiseRecord(the, (*stream)->inFlightWriteRequest, error);
	(*stream)->inFlightWriteRequest = NULL;
	uint8_t state = (*stream)->state;
	mxStreamAssert((state == mcStreamWritable) || (state == mcStreamErroring));
	WritableStreamDealWithRejection(the, stream, error);
}
txBoolean WritableStreamHasOperationMarkedInFlight(xsMachine* the, WritableStream* stream)
{
	return (((*stream)->inFlightWriteRequest) || ((*stream)->inFlightCloseRequest)) ? 1 : 0;
}
void WritableStreamMarkCloseRequestInFlight(xsMachine* the, WritableStream* stream)
{
	mxStreamAssert((*stream)->inFlightCloseRequest == NULL);
	mxStreamAssert((*stream)->closeRequest != NULL);
	(*stream)->inFlightCloseRequest = (*stream)->closeRequest;
	(*stream)->closeRequest = NULL;
}
void WritableStreamMarkFirstWriteRequestInFlight(xsMachine* the, WritableStream* stream)
{
	mxStreamAssert((*stream)->inFlightWriteRequest == NULL);
	mxStreamAssert(GetSlotQueueLength(the, (*stream)->writeRequests) > 0);
	xsSlot* writeRequest;
	mxTemporary(writeRequest);
	DequeueSlot(the, (*stream)->writeRequests, writeRequest);
	(*stream)->inFlightWriteRequest = fxToReference(the, writeRequest);
	mxPop();
}
void WritableStreamRejectCloseAndClosedPromiseIfNeeded(xsMachine* the, WritableStream* stream)
{
	uint8_t state = (*stream)->state;
	mxStreamAssert(state == mcStreamErrored);
	if ((*stream)->closeRequest) {
		mxStreamAssert((*stream)->inFlightCloseRequest == NULL);
		fxRejectPromiseRecord(the, (*stream)->closeRequest, (*stream)->storedError);
		(*stream)->closeRequest = NULL;
	}
	WritableStreamDefaultWriter* writer = (*stream)->writer;
	if (writer) {
		fxRejectPromiseRecord(the, (*writer)->closedPromise, (*stream)->storedError);
		fxHandlePromiseRecord(the, (*writer)->closedPromise, (*stream)->closures);
	}
}
void WritableStreamStartErroring(xsMachine* the, WritableStream* stream, txSlot* reason)
{
	mxStreamAssert((*stream)->storedError->kind == XS_UNINITIALIZED_KIND);
	uint8_t state = (*stream)->state;
	mxStreamAssert(state == mcStreamWritable);
	WritableStreamDefaultController* controller = (*stream)->controller;
	mxStreamAssert(controller != NULL);
	(*stream)->state = mcStreamErroring;
	(*stream)->storedError->kind = reason->kind;
	(*stream)->storedError->value = reason->value;
	WritableStreamDefaultWriter* writer = (*stream)->writer;
	if (writer)
		WritableStreamDefaultWriterEnsureReadyPromiseRejected(the, writer, reason);
	if (!WritableStreamHasOperationMarkedInFlight(the, stream) && (*controller)->started)
		WritableStreamFinishErroring(the, stream);
}
void WritableStreamUpdateBackpressure(xsMachine* the, WritableStream* stream, txBoolean backpressure)
{
	uint8_t state = (*stream)->state;
	mxStreamAssert(state == mcStreamWritable);
	mxStreamAssert(!WritableStreamCloseQueuedOrInFlight(the, stream));
	WritableStreamDefaultWriter* writer = (*stream)->writer;
	if (writer && ((*stream)->backpressure != backpressure)) {
		if (backpressure)
			(*writer)->readyPromise = fxCreatePromiseRecord(the, NULL);
		else 
			fxResolvePromiseRecord(the, (*writer)->readyPromise, &mxUndefined);
	}
	(*stream)->backpressure = backpressure;
}

txBoolean WritableStreamDefaultSinkAbortAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* reason, xsSlot* result)
{
	WritableStreamDefaultController* controller = (WritableStreamDefaultController*)stuff;
	txBoolean success = 1;
	if ((*controller)->abortAlgorithm.callback) {
		mxTry(the) {
			if ((*controller)->target)
				mxPushReference((*controller)->target);
			else
				mxPushUndefined();
			mxPushReference((*controller)->abortAlgorithm.callback);
			mxCall();
			mxPushSlot(reason);
			mxRunCount(1);
			mxPullSlot(result);
		}
		mxCatch(the) {
			*result = mxException;
			mxException = mxUndefined;
			success = 0;
		}
	}
	return success;
}
txBoolean WritableStreamDefaultSinkCloseAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* param, xsSlot* result)
{
	WritableStreamDefaultController* controller = (WritableStreamDefaultController*)stuff;
	txBoolean success = 1;
	if ((*controller)->closeAlgorithm.callback) {
		mxTry(the) {
			if ((*controller)->target)
				mxPushReference((*controller)->target);
			else
				mxPushUndefined();
			mxPushReference((*controller)->closeAlgorithm.callback);
			mxCall();
			mxRunCount(0);
			mxPullSlot(result);
		}
		mxCatch(the) {
			*result = mxException;
			mxException = mxUndefined;
			success = 0;
		}
	}
	return success;
}
txBoolean WritableStreamDefaultSinkStartAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* param, xsSlot* result)
{
	WritableStreamDefaultController* controller = (WritableStreamDefaultController*)stuff;
	txBoolean success = 1;
	if ((*controller)->startAlgorithm.callback) {
		if ((*controller)->target)
			mxPushReference((*controller)->target);
		else
			mxPushUndefined();
		mxPushReference((*controller)->startAlgorithm.callback);
		mxCall();
		mxPushReference((*controller)->reference);
		mxRunCount(1);
		mxPullSlot(result);
	}
	return success;
}
txBoolean WritableStreamDefaultSinkWriteAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* chunk, xsSlot* result)
{
	WritableStreamDefaultController* controller = (WritableStreamDefaultController*)stuff;
	txBoolean success = 1;
	if ((*controller)->writeAlgorithm.callback) {
		mxTry(the) {
			if ((*controller)->target)
				mxPushReference((*controller)->target);
			else
				mxPushUndefined();
			mxPushReference((*controller)->writeAlgorithm.callback);
			mxCall();
			mxPushSlot(chunk);
			mxPushReference((*controller)->reference);
			mxRunCount(2);
			mxPullSlot(result);
		}
		mxCatch(the) {
			*result = mxException;
			mxException = mxUndefined;
			success = 0;
		}
	}
	return success;
}





