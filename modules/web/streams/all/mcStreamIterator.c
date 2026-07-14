#include "mcStreamAll.h"

typedef struct ReadableStreamAsyncIteratorStruct ReadableStreamAsyncIteratorRecord, *ReadableStreamAsyncIterator;
struct ReadableStreamAsyncIteratorStruct {
	StreamHandlePart;
	StreamDispatchPart;
	ReadableStreamDefaultReader* reader;
	xsSlot* ongoingPromise;
	xsSlot* nextSteps;
	xsSlot* nextStepsResolved;
	xsSlot* nextStepsRejected;
	xsSlot* returnQueue;
	xsSlot* returnSteps;
	xsSlot* returnStepsResolved;
	xsSlot* returnStepsRejected;
	txBoolean preventCancel;
	txBoolean isFinished;
};

static void StreamIteratorChainSteps(xsMachine* the, ReadableStreamAsyncIterator* iterator, xsSlot* steps);
static void StreamIteratorReaderCloseSteps(xsMachine* the, ReadableStreamReader* reader, xsSlot* request, xsSlot* chunk);
static void StreamIteratorReaderErrorSteps(xsMachine* the, ReadableStreamReader* reader, xsSlot* request, xsSlot* error);
static void StreamIteratorNextSteps(xsMachine* the);
static void StreamIteratorNextStepsRejected(xsMachine* the);
static void StreamIteratorNextStepsResolved(xsMachine* the);
static void StreamIteratorReturnSteps(xsMachine* the);
static void StreamIteratorReturnStepsRejected(xsMachine* the);
static void StreamIteratorReturnStepsResolved(xsMachine* the);

static void ReadableStreamAsyncIterator_destructor(void* it);
static void ReadableStreamAsyncIterator_mark(xsMachine* the, void* it, xsMarkRoot markRoot);
static const xsHostHooks ReadableStreamAsyncIteratorHooks = {
	ReadableStreamAsyncIterator_destructor,
	ReadableStreamAsyncIterator_mark,
	NULL
};
static const StreamDispatchRecord ReadableStreamAsyncIteratorDispatchRecord = {
	"ReadableStreamAsyncIterator",
};

void buildReadableStreamAsyncIterator(xsMachine* the)
{
	ReadableStream* stream = mxStreamHandle(ReadableStream, mxThis);
	txBoolean preventCancel = fxToBoolean(the, mxArgv(0));
	ReadableStreamDefaultReader* reader;
	
	xsSlot* instance = fxNewHostObject(the, NULL);
	mxPullSlot(mxResult);
	
	mxPushReference((*stream)->closures);
	mxGetID(xsID_readableStreamAsyncIteratorPrototype);
	instance->value.instance.prototype = the->stack->value.reference;
	mxPop();
	
	fxSetHostChunk(the, mxResult, NULL, sizeof(ReadableStreamAsyncIteratorRecord));
	fxSetHostHooks(the, mxResult, (xsHostHooks*)&ReadableStreamAsyncIteratorHooks);
	ReadableStreamAsyncIterator* iterator = (ReadableStreamAsyncIterator*)fxGetHostHandle(the, mxResult);
	(*iterator)->reference = instance;
	(*iterator)->dispatch = (StreamDispatch)&ReadableStreamAsyncIteratorDispatchRecord;
	reader = (*iterator)->reader = AcquireReadableStreamDefaultReader(the, stream);;
	(*iterator)->preventCancel = preventCancel;
	
	(*reader)->context = iterator;
	(*reader)->chunkSteps = ReadableStreamReaderChunkSteps;
	(*reader)->closeSteps = StreamIteratorReaderCloseSteps;
	(*reader)->errorSteps = StreamIteratorReaderErrorSteps;
	
	(*iterator)->nextSteps = fxNewHostFunctionWithHandle(the, iterator, StreamIteratorNextSteps, 1); mxPop();
	(*iterator)->nextStepsResolved = fxNewHostFunctionWithHandle(the, iterator, StreamIteratorNextStepsResolved, 1); mxPop();
	(*iterator)->nextStepsRejected = fxNewHostFunctionWithHandle(the, iterator, StreamIteratorNextStepsRejected, 1); mxPop();

	(*iterator)->returnQueue = CreateSlotQueue(the);
	(*iterator)->returnSteps = fxNewHostFunctionWithHandle(the, iterator, StreamIteratorReturnSteps, 1); mxPop();
	(*iterator)->returnStepsResolved = fxNewHostFunctionWithHandle(the, iterator, StreamIteratorReturnStepsResolved, 1); mxPop();
	(*iterator)->returnStepsRejected = fxNewHostFunctionWithHandle(the, iterator, StreamIteratorReturnStepsRejected, 1); mxPop();
	
}
void ReadableStreamAsyncIterator_destructor(void* it)
{
}
void ReadableStreamAsyncIterator_mark(xsMachine* the, void* it, xsMarkRoot markRoot)
{
	ReadableStreamAsyncIterator self = it;
	StreamMarkHandle(the, self->reader);
    StreamMarkReference(the, self->ongoingPromise);
    StreamMarkReference(the, self->nextSteps);
    StreamMarkReference(the, self->nextStepsResolved);
    StreamMarkReference(the, self->nextStepsRejected);
    StreamMarkReference(the, self->returnQueue);
    StreamMarkReference(the, self->returnSteps);
    StreamMarkReference(the, self->returnStepsResolved);
    StreamMarkReference(the, self->returnStepsRejected);
}
void ReadableStreamAsyncIterator_next(xsMachine* the)
{
	ReadableStreamAsyncIterator* iterator = mxStreamHandle(ReadableStreamAsyncIterator, mxThis);
	StreamIteratorChainSteps(the, iterator, (*iterator)->nextSteps);
}
void ReadableStreamAsyncIterator_return(xsMachine* the)
{
	ReadableStreamAsyncIterator* iterator = mxStreamHandle(ReadableStreamAsyncIterator, mxThis);
	if (mxArgc > 0)
		mxPushSlot(mxArgv(0));
	else
		mxPushUndefined();
	EnqueueSlot(the, (*iterator)->returnQueue, the->stack);
	mxPop();
	StreamIteratorChainSteps(the, iterator, (*iterator)->returnSteps);
}

void StreamIteratorChainSteps(xsMachine* the, ReadableStreamAsyncIterator* iterator, xsSlot* steps)
{
	xsSlot* ongoingPromise = (*iterator)->ongoingPromise;
	if (ongoingPromise) {
		mxPushReference(ongoingPromise);
		fxChainPromise(the, the->stack, steps, steps, mxResult);
		mxPop();
	}
	else {
		mxPushUndefined();
		mxPushReference(steps);
		mxCall();
		mxRunCount(0);
		mxPullSlot(mxResult);
	}
	(*iterator)->ongoingPromise = fxToReference(the, mxResult);
}
void StreamIteratorReaderCloseSteps(xsMachine* the, ReadableStreamReader* reader, xsSlot* request, xsSlot* chunk)
{
	ReadableStreamAsyncIterator* iterator =  (ReadableStreamAsyncIterator*)(*reader)->context;
	if (!(*iterator)->isFinished) {
		(*iterator)->isFinished = 1;
		ReadableStreamReaderGenericRelease(the, (ReadableStreamReader*)(*iterator)->reader);
	}
	ReadableStreamReaderCloseSteps(the, reader, request, chunk);
}
void StreamIteratorReaderErrorSteps(xsMachine* the, ReadableStreamReader* reader, xsSlot* request, xsSlot* error)
{
	ReadableStreamAsyncIterator* iterator = (ReadableStreamAsyncIterator*)(*reader)->context;
	if (!(*iterator)->isFinished) {
		(*iterator)->isFinished = 1;
		ReadableStreamReaderGenericRelease(the, (ReadableStreamReader*)(*iterator)->reader);
	}
	ReadableStreamReaderErrorSteps(the, reader, request, error);
}
void StreamIteratorNextSteps(xsMachine* the)
{
	ReadableStreamAsyncIterator* iterator = (ReadableStreamAsyncIterator*)fxGetHostFunctionHandle(the);
	if ((*iterator)->isFinished) {
		mxPushUndefined();
		fxNewStreamResult(the, 1);
		fxCreateResolvedPromise(the, the->stack, mxResult);
		mxPop();
		return;
	}
	xsSlot* record = fxCreatePromiseRecord(the, mxResult);
	ReadableStreamDefaultReaderRead(the, (*iterator)->reader, mxResult);
	fxGetPromiseRecordPromise(the, record, mxResult);
	fxChainPromise(the, mxResult, (*iterator)->nextStepsResolved, (*iterator)->nextStepsRejected, mxResult);
}
void StreamIteratorNextStepsRejected(xsMachine* the)
{
	ReadableStreamAsyncIterator* iterator = (ReadableStreamAsyncIterator*)fxGetHostFunctionHandle(the);
	(*iterator)->ongoingPromise = NULL;
	mxPushSlot(mxArgv(0));
	mxPull(mxException);
	fxJump(the);
}
void StreamIteratorNextStepsResolved(xsMachine* the)
{
	ReadableStreamAsyncIterator* iterator = (ReadableStreamAsyncIterator*)fxGetHostFunctionHandle(the);
	(*iterator)->ongoingPromise = NULL;
	mxPushSlot(mxArgv(0));
	mxPullSlot(mxResult);
	
}
void StreamIteratorReturnSteps(xsMachine* the)
{
	ReadableStreamAsyncIterator* iterator = (ReadableStreamAsyncIterator*)fxGetHostFunctionHandle(the);
	xsSlot* reason;
	mxTemporary(reason);
	if ((*iterator)->isFinished) {
		DequeueSlot(the, (*iterator)->returnQueue, reason);
		fxNewStreamResult(the, 1);
		fxCreateResolvedPromise(the, the->stack, mxResult);
		mxPop();
	}
	else {
		ReadableStreamDefaultReader* reader = (*iterator)->reader;
		(*iterator)->isFinished = 1;
		if (!(*iterator)->preventCancel) {
			PeekSlotQueue(the, (*iterator)->returnQueue, reason);
			ReadableStreamCancel(the, (*reader)->stream, reason, mxResult);
			ReadableStreamReaderGenericRelease(the, (ReadableStreamReader*)reader);
			fxChainPromise(the, mxResult, (*iterator)->returnStepsResolved, (*iterator)->returnStepsRejected, mxResult);
		}
		else {
			ReadableStreamReaderGenericRelease(the, (ReadableStreamReader*)reader);
			DequeueSlot(the, (*iterator)->returnQueue, reason);
			fxNewStreamResult(the, 1);	
			fxCreateResolvedPromise(the, the->stack, mxResult);
			mxPop();
		}
	}
	mxPop(); // reason
}
void StreamIteratorReturnStepsRejected(xsMachine* the)
{
	ReadableStreamAsyncIterator* iterator = (ReadableStreamAsyncIterator*)fxGetHostFunctionHandle(the);
	(*iterator)->ongoingPromise = NULL;
	mxPushSlot(mxArgv(0));
	mxPull(mxException);
	fxJump(the);
}
void StreamIteratorReturnStepsResolved(xsMachine* the)
{
	ReadableStreamAsyncIterator* iterator = (ReadableStreamAsyncIterator*)fxGetHostFunctionHandle(the);
	(*iterator)->ongoingPromise = NULL;
	mxPushUndefined();
	DequeueSlot(the, (*iterator)->returnQueue, the->stack);
	fxNewStreamResult(the, 1);	
	mxPullSlot(mxResult);
}

typedef struct ReadableStreamFromStruct ReadableStreamFromRecord, *ReadableStreamFrom;
struct ReadableStreamFromStruct {
	StreamHandlePart;
	StreamDispatchPart;
	ReadableStream* stream;
	xsSlot* iterator;
	xsSlot* nextMethod;
	xsSlot* nextResolved;
	xsSlot* returnResolved;
};

static txBoolean StreamFromCancelAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* reason, xsSlot* result);
static txBoolean StreamFromPullAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* param, xsSlot* result);
static txBoolean StreamFromStartAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* param, xsSlot* result);
static void StreamFromNextResolved(xsMachine* the);
static void StreamFromReturnResolved(xsMachine* the);

static void ReadableStreamFrom_destructor(void* it);
static void ReadableStreamFrom_mark(xsMachine* the, void* it, xsMarkRoot markRoot);
static const xsHostHooks ReadableStreamFromHooks = {
	ReadableStreamFrom_destructor,
	ReadableStreamFrom_mark,
	NULL
};
static const StreamDispatchRecord ReadableStreamFromDispatchRecord = {
	"ReadableStreamFrom",
};
void ReadableStreamFrom_destructor(void* it)
{
}
void ReadableStreamFrom_mark(xsMachine* the, void* it, xsMarkRoot markRoot)
{
	ReadableStreamFrom self = it;
	StreamMarkHandle(the, self->stream);
    StreamMarkReference(the, self->iterator);
    StreamMarkReference(the, self->nextMethod);
    StreamMarkReference(the, self->nextResolved);
    StreamMarkReference(the, self->returnResolved);
}

void buildReadableStreamFrom(xsMachine* the)
{
	txSlot* iterable = mxArgv(0);
	txSlot* closures = fxToReference(the, mxArgv(1));
	txSlot* iterator = NULL;
	txSlot* nextMethod = NULL;
	
	mxPushSlot(iterable);
	mxDub();
	mxGetID(mxID(_Symbol_asyncIterator));
	if (mxIsUndefined(the->stack) || mxIsNull(the->stack)) {
		mxPop();
		mxDub();
		mxGetID(mxID(_Symbol_iterator));
		mxCall();
		mxRunCount(0);
		fxNewAsyncFromSyncIteratorInstance(the);
	}
	else {
		mxCall();
		mxRunCount(0);
	}
	if (!mxIsReference(the->stack))
		mxTypeError("iterator is no object");
	iterator = the->stack;
	mxDub();
	mxGetID(xsID_next);
	if (!fxIsCallable(the, the->stack))
		mxTypeError("iterator.next is no function");
	fxToInstance(the, the->stack); // host function
	nextMethod = the->stack;
			
	xsSlot* fromInstance = fxNewHostObject(the, NULL);
	xsSlot* fromReference = the->stack;
	fxSetHostChunk(the, fromReference, NULL, sizeof(ReadableStreamFromRecord));
	fxSetHostHooks(the, fromReference, (xsHostHooks*)&ReadableStreamFromHooks);
	ReadableStreamFrom* from = (ReadableStreamFrom*)fxGetHostHandle(the, fromReference);
	(*from)->reference = fromInstance;
	(*from)->dispatch = (StreamDispatch)&ReadableStreamFromDispatchRecord;
	(*from)->iterator = fxToReference(the, iterator);
	(*from)->nextMethod = fxToReference(the, nextMethod);
	(*from)->nextResolved = fxNewHostFunctionWithHandle(the, from, StreamFromNextResolved, 1); mxPop();
	(*from)->returnResolved = fxNewHostFunctionWithHandle(the, from, StreamFromReturnResolved, 1); mxPop();
		
	ReadableStream* stream = (*from)->stream = CreateReadableStream(the, mxResult, closures);
	ReadableStreamDefaultController* controller = CreateReadableStreamDefaultController(the, stream);
	(*controller)->target = fromInstance;
	(*controller)->startAlgorithm.call = StreamFromStartAlgorithm;
	(*controller)->pullAlgorithm.call = StreamFromPullAlgorithm;
	(*controller)->cancelAlgorithm.call = StreamFromCancelAlgorithm;
	
	mxPop(); // fromReference
	mxPop(); // nextMethod
	mxPop(); // iterator
	
	ReadableStreamDefaultControllerStart(the, controller);
}

txBoolean StreamFromCancelAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* reason, xsSlot* result)
{
	ReadableStreamController* controller = (ReadableStreamController*)stuff;
	ReadableStreamFrom* from = mxStreamHandle(ReadableStreamFrom, (*controller)->target);
	txBoolean success = 1;
	mxTry(the) {
		mxPushReference((*from)->iterator);
		mxDub();
		mxGetID(xsID_return);
		if (!mxIsUndefined(the->stack) && !mxIsNull(the->stack)) {
			if (!fxIsCallable(the, the->stack))
				mxTypeError("iterator.return is no function");
			mxCall();
			mxPushSlot(reason);
			mxRunCount(1);
			fxCreateResolvedPromise(the, the->stack, the->stack);
			fxChainPromise(the, the->stack, (*from)->returnResolved, NULL, result);
			mxPop();
		}
		else {
			mxPop();
			mxPop();
		}
	}
	mxCatch(the) {
		*result = mxException;
		mxException = mxUndefined;
		success = 0;
	}
	return success;
}
txBoolean StreamFromPullAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* param, xsSlot* result)
{
	ReadableStreamController* controller = (ReadableStreamController*)stuff;
	ReadableStreamFrom* from = mxStreamHandle(ReadableStreamFrom, (*controller)->target);
	txBoolean success = 1;
	mxTry(the) {
		mxPushReference((*from)->iterator);
		mxPushReference((*from)->nextMethod);
		mxCall();
		mxRunCount(0);
		fxCreateResolvedPromise(the, the->stack, the->stack);
		fxChainPromise(the, the->stack, (*from)->nextResolved, NULL, result);
		mxPop();
	}
	mxCatch(the) {
		*result = mxException;
		mxException = mxUndefined;
		success = 0;
	}
	return success;
}
txBoolean StreamFromStartAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* param, xsSlot* result)
{
	return 1;
}
void StreamFromNextResolved(xsMachine* the)
{
	ReadableStreamFrom* from = (ReadableStreamFrom*)fxGetHostFunctionHandle(the);
	ReadableStream* stream = (*from)->stream;
	xsSlot* result = mxArgv(0);
	if (!mxIsReference(result))
		mxTypeError("The promise returned by the iterator.next() method must fulfill with an object");
	mxPushSlot(result);
	mxGetID(xsID_done);
	if (fxToBoolean(the, the->stack)) {
		ReadableStreamDefaultControllerClose(the, (ReadableStreamDefaultController*)(*stream)->controller);
	}
	else {
		mxPushSlot(result);
		mxGetID(xsID_value);
		ReadableStreamDefaultControllerEnqueue(the, (ReadableStreamDefaultController*)(*stream)->controller, the->stack);
		mxPop();
	}
	mxPop();
}
void StreamFromReturnResolved(xsMachine* the)
{
	xsSlot* result = mxArgv(0);
	if (!mxIsReference(result))
		mxTypeError("The promise returned by the iterator.next() method must fulfill with an object");
}





