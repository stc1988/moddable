#include "mcStreamAll.h"

static void WritableStreamDefaultControllerAdvanceQueueIfNeeded(xsMachine* the, WritableStreamDefaultController* controller);
static void WritableStreamDefaultControllerClearAlgorithms(xsMachine* the, WritableStreamDefaultController* controller);
static void WritableStreamDefaultControllerError(xsMachine* the, WritableStreamDefaultController* controller, xsSlot* error);
static txBoolean WritableStreamDefaultControllerGetBackpressure(xsMachine* the, WritableStreamDefaultController* controller);
static void WritableStreamDefaultControllerProcessClose(xsMachine* the, WritableStreamDefaultController* controller);
static void WritableStreamDefaultControllerProcessWrite(xsMachine* the, WritableStreamDefaultController* controller, xsSlot* chunk);

static void WritableStreamDefaultControllerStartAlgorithmResolved(xsMachine* the);
static void WritableStreamDefaultControllerStartAlgorithmRejected(xsMachine* the);
static void WritableStreamDefaultControllerWriteAlgorithmResolved(xsMachine* the);
static void WritableStreamDefaultControllerWriteAlgorithmRejected(xsMachine* the);
static void WritableStreamDefaultControllerCloseAlgorithmResolved(xsMachine* the);
static void WritableStreamDefaultControllerCloseAlgorithmRejected(xsMachine* the);
static void WritableStreamDefaultControllerAbortAlgorithmResolved(xsMachine* the);
static void WritableStreamDefaultControllerAbortAlgorithmRejected(xsMachine* the);

static void WritableStreamDefaultController_mark(xsMachine* the, void* it, xsMarkRoot markRoot);
static const xsHostHooks WritableStreamDefaultControllerHooks ICACHE_RODATA_ATTR = {
	WritableStreamDefaultController_destructor,
	WritableStreamDefaultController_mark,
	NULL
};
static const StreamDispatchRecord WritableStreamDefaultControllerDispatchRecord = {
	"WritableStreamDefaultController",
};

// 5.4 WritableStreamDefaultController
void WritableStreamDefaultController_destructor(void* it)
{
}
void WritableStreamDefaultController_mark(xsMachine* the, void* it, xsMarkRoot markRoot)
{
	WritableStreamDefaultController self = it;
	StreamMarkHandle(the, self->stream);
	StreamMarkReference(the, self->queue);
	StreamMarkReference(the, self->abortController);
	StreamMarkReference(the, self->strategySizeAlgorithm);
	StreamMarkReference(the, self->target);
	StreamMarkAlgorithm(the, &self->startAlgorithm, markRoot);
	StreamMarkAlgorithm(the, &self->writeAlgorithm, markRoot);
	StreamMarkAlgorithm(the, &self->closeAlgorithm, markRoot);
	StreamMarkAlgorithm(the, &self->abortAlgorithm, markRoot);
}
void WritableStreamDefaultController_get_signal(xsMachine* the)
{
	WritableStreamDefaultController* controller = mxStreamHandle(WritableStreamDefaultController, mxThis);
	mxPushReference((*controller)->abortController);
	mxGetID(xsID_signal);
	mxPullSlot(mxResult);
}
void WritableStreamDefaultController_error(xsMachine* the)
{
	WritableStreamDefaultController* controller = mxStreamHandle(WritableStreamDefaultController, mxThis);
	WritableStream* stream = (*controller)->stream;
	if ((*stream)->state != mcStreamWritable)
		return;
	if (mxArgc > 0)
		mxPushSlot(mxArgv(0));
	else
		mxPushUndefined();
	WritableStreamDefaultControllerError(the, controller, the->stack);
	mxPop();
}

// 5.5.4 Default controllers
WritableStreamDefaultController* CreateWritableStreamDefaultController(xsMachine* the, WritableStream* stream)
{
	mxPushReference((*stream)->closures);
	mxGetID(xsID_writableStreamDefaultController);
	xsSlot* instance = fxNewHostInstance(the);
	xsSlot* reference = the->stack;
	fxSetHostChunk(the, reference, NULL, sizeof(WritableStreamDefaultControllerRecord));
	fxSetHostHooks(the, reference, (xsHostHooks*)&WritableStreamDefaultControllerHooks);
	WritableStreamDefaultController* controller = (WritableStreamDefaultController*)fxGetHostHandle(the, reference);
	(*controller)->reference = instance;
	(*controller)->dispatch = (StreamDispatch)&WritableStreamDefaultControllerDispatchRecord;
	
	(*controller)->queue = CreateValueSizeQueue(the);
	
	mxPushReference((*stream)->closures);
	mxGetID(xsID_AbortController);
	mxNew();
	mxRunCount(0);
	(*controller)->abortController = fxToReference(the, the->stack);
	mxPop();

	(*controller)->startAlgorithm.resolved = fxNewHostFunctionWithHandle(the, controller, WritableStreamDefaultControllerStartAlgorithmResolved, 1); mxPop();
	(*controller)->startAlgorithm.rejected = fxNewHostFunctionWithHandle(the, controller, WritableStreamDefaultControllerStartAlgorithmRejected, 1); mxPop();
	(*controller)->writeAlgorithm.resolved = fxNewHostFunctionWithHandle(the, controller, WritableStreamDefaultControllerWriteAlgorithmResolved, 1); mxPop();
	(*controller)->writeAlgorithm.rejected = fxNewHostFunctionWithHandle(the, controller, WritableStreamDefaultControllerWriteAlgorithmRejected, 1); mxPop();
	(*controller)->closeAlgorithm.resolved = fxNewHostFunctionWithHandle(the, controller, WritableStreamDefaultControllerCloseAlgorithmResolved, 1); mxPop();
	(*controller)->closeAlgorithm.rejected = fxNewHostFunctionWithHandle(the, controller, WritableStreamDefaultControllerCloseAlgorithmRejected, 1); mxPop();
	(*controller)->abortAlgorithm.resolved = fxNewHostFunctionWithHandle(the, controller, WritableStreamDefaultControllerAbortAlgorithmResolved, 1); mxPop();
	(*controller)->abortAlgorithm.rejected = fxNewHostFunctionWithHandle(the, controller, WritableStreamDefaultControllerAbortAlgorithmRejected, 1); mxPop();
	
	(*controller)->stream = stream;
	(*stream)->controller = controller;
	mxPop(); // controller
	
	return controller;
}
void WritableStreamDefaultControllerAdvanceQueueIfNeeded(xsMachine* the, WritableStreamDefaultController* controller)
{
	WritableStream* stream = (*controller)->stream;
	if (!(*controller)->started)
		return;
	if ((*stream)->inFlightWriteRequest)
		return;
	uint8_t state = (*stream)->state;
	mxStreamAssert((state != mcStreamClosed) && (state != mcStreamErrored));
	if (state == mcStreamErroring) {
		WritableStreamFinishErroring(the, stream);
		return;
	}
	if (GetQueueLength(the, (*controller)->queue) == 0)
		return;
	txSlot* value;
	txNumber size;
	mxTemporary(value);
	PeekQueueValue(the, (*controller)->queue, value, &size);
	if (mxIsUndefined(value) && (size == 0))
		WritableStreamDefaultControllerProcessClose(the, controller);
	else
		WritableStreamDefaultControllerProcessWrite(the, controller, value);
	mxPop();
}
void WritableStreamDefaultControllerClearAlgorithms(xsMachine* the, WritableStreamDefaultController* controller)
{
	(*controller)->strategySizeAlgorithm = NULL;
	(*controller)->writeAlgorithm.call = NULL;
	(*controller)->writeAlgorithm.callback = NULL;
	(*controller)->closeAlgorithm.call = NULL;
	(*controller)->closeAlgorithm.callback = NULL;
	(*controller)->abortAlgorithm.call = NULL;
	(*controller)->abortAlgorithm.callback = NULL;
}
void WritableStreamDefaultControllerClose(xsMachine* the, WritableStreamDefaultController* controller)
{
	EnqueueValueSize(the, (*controller)->queue, &mxUndefined, 0);
	WritableStreamDefaultControllerAdvanceQueueIfNeeded(the, controller);
}
void WritableStreamDefaultControllerError(xsMachine* the, WritableStreamDefaultController* controller, xsSlot* error)
{
	WritableStream* stream = (*controller)->stream;
	mxStreamAssert((*stream)->state == mcStreamWritable);
	WritableStreamDefaultControllerClearAlgorithms(the, controller);
	WritableStreamStartErroring(the, stream, error);
}
void WritableStreamDefaultControllerErrorIfNeeded(xsMachine* the, WritableStreamDefaultController* controller, xsSlot* error)
{
	WritableStream* stream = (*controller)->stream;
	if ((*stream)->state == mcStreamWritable)
		WritableStreamDefaultControllerError(the, controller, error);
}
txBoolean WritableStreamDefaultControllerGetBackpressure(xsMachine* the, WritableStreamDefaultController* controller)
{
	txNumber desiredSize = WritableStreamDefaultControllerGetDesiredSize(the, controller);
	return (desiredSize <= 0) ? 1 : 0;
}
txNumber WritableStreamDefaultControllerGetChunkSize(xsMachine* the, WritableStreamDefaultController* controller, xsSlot* chunk)
{
	txNumber chunkSize = 1;
	mxTry(the) {
		if ((*controller)->strategySizeAlgorithm) {
			mxPushUndefined();
			mxPushReference((*controller)->strategySizeAlgorithm);
			mxCall();
			mxPushSlot(chunk);
			mxRunCount(1);
			chunkSize = fxToNumber(the, the->stack);
			mxPop();
		}
	}
	mxCatch(the) {
		mxPush(mxException);
		mxException = xsUndefined;
		WritableStreamDefaultControllerErrorIfNeeded(the, controller, the->stack);
		mxPop();
	}
	return chunkSize;
}
txNumber WritableStreamDefaultControllerGetDesiredSize(xsMachine* the, WritableStreamDefaultController* controller)
{
	return (*controller)->strategyHWM - GetQueueTotalSize(the, (*controller)->queue);
}
void WritableStreamDefaultControllerProcessAbort(xsMachine* the, WritableStreamDefaultController* controller, xsSlot* reason)
{
	txSlot* result;
	mxTemporary(result);
	txBoolean success = (*((*controller)->abortAlgorithm.call))(the, (StreamStuff*)controller, reason, result);
	WritableStreamDefaultControllerClearAlgorithms(the, controller);
	fxChainAlgorithm(the, result, success, (*controller)->abortAlgorithm.resolved, (*controller)->abortAlgorithm.rejected);
	mxPop(); // result
}
void WritableStreamDefaultControllerProcessClose(xsMachine* the, WritableStreamDefaultController* controller)
{
	WritableStream* stream = (*controller)->stream;
	WritableStreamMarkCloseRequestInFlight(the, stream);
	txSlot* result;
	mxTemporary(result);
	txBoolean success = (*((*controller)->closeAlgorithm.call))(the, (StreamStuff*)controller, NULL, result);
	WritableStreamDefaultControllerClearAlgorithms(the, controller);
	fxChainAlgorithm(the, result, success, (*controller)->closeAlgorithm.resolved, (*controller)->closeAlgorithm.rejected);
	mxPop(); // result
}
void WritableStreamDefaultControllerProcessWrite(xsMachine* the, WritableStreamDefaultController* controller, xsSlot* chunk)
{
	WritableStream* stream = (*controller)->stream;
	WritableStreamMarkFirstWriteRequestInFlight(the, stream);
	txSlot* result;
	mxTemporary(result);
	txBoolean success = (*((*controller)->writeAlgorithm.call))(the, (StreamStuff*)controller, chunk, result);
	fxChainAlgorithm(the, result, success, (*controller)->writeAlgorithm.resolved, (*controller)->writeAlgorithm.rejected);
	mxPop(); // result
}
void WritableStreamDefaultControllerStart(xsMachine* the, WritableStreamDefaultController* controller)
{
	txBoolean backpressure = WritableStreamDefaultControllerGetBackpressure(the, controller);
	WritableStreamUpdateBackpressure(the, (*controller)->stream, backpressure);
	txSlot* result;
	mxTemporary(result);
	txBoolean success = (*(*controller)->startAlgorithm.call)(the, (StreamStuff*)controller, NULL, result);
	fxChainAlgorithm(the, result, success, (*controller)->startAlgorithm.resolved, (*controller)->startAlgorithm.rejected);
	mxPop(); // result
}
void WritableStreamDefaultControllerWrite(xsMachine* the, WritableStreamDefaultController* controller, xsSlot* chunk, txNumber chunkSize)
{
	mxTry(the) {
		EnqueueValueSize(the, (*controller)->queue, chunk, chunkSize);
	}
	mxCatch(the) {
		WritableStreamDefaultControllerErrorIfNeeded(the, controller, &mxException);
		return;
	}
	WritableStream* stream = (*controller)->stream;
	if (!WritableStreamCloseQueuedOrInFlight(the, stream) && ((*stream)->state == mcStreamWritable)) {
		txBoolean backpressure = WritableStreamDefaultControllerGetBackpressure(the, controller);
		WritableStreamUpdateBackpressure(the, stream, backpressure);
	}
	WritableStreamDefaultControllerAdvanceQueueIfNeeded(the, controller);
}

void WritableStreamDefaultControllerStartAlgorithmResolved(xsMachine* the)
{
	WritableStreamDefaultController* controller = (WritableStreamDefaultController*)fxGetHostFunctionHandle(the);
	(*controller)->started = 1;
	WritableStreamDefaultControllerAdvanceQueueIfNeeded(the, controller);
}
void WritableStreamDefaultControllerStartAlgorithmRejected(xsMachine* the)
{
	WritableStreamDefaultController* controller = (WritableStreamDefaultController*)fxGetHostFunctionHandle(the);
	(*controller)->started = 1;
	WritableStreamDealWithRejection(the, (*controller)->stream, mxArgv(0));
}
void WritableStreamDefaultControllerWriteAlgorithmResolved(xsMachine* the)
{
	WritableStreamDefaultController* controller = (WritableStreamDefaultController*)fxGetHostFunctionHandle(the);
	WritableStream* stream = (*controller)->stream;
	uint8_t state = (*stream)->state;
	mxStreamAssert((state == mcStreamWritable) || (state == mcStreamErroring));
	WritableStreamFinishInFlightWrite(the, stream);
	txSlot* value;
	mxTemporary(value);
	DequeueValueSize(the, (*controller)->queue, value);
	mxPop();
	if (!WritableStreamCloseQueuedOrInFlight(the, stream) && (state == mcStreamWritable)) {
		txBoolean backpressure = WritableStreamDefaultControllerGetBackpressure(the, controller);
		WritableStreamUpdateBackpressure(the, stream, backpressure);
	}
	WritableStreamDefaultControllerAdvanceQueueIfNeeded(the, controller);
}
void WritableStreamDefaultControllerWriteAlgorithmRejected(xsMachine* the)
{
	WritableStreamDefaultController* controller = (WritableStreamDefaultController*)fxGetHostFunctionHandle(the);
	WritableStream* stream = (*controller)->stream;
	if ((*stream)->state == mcStreamWritable)
		WritableStreamDefaultControllerClearAlgorithms(the, controller);
	WritableStreamFinishInFlightWriteWithError(the, stream, mxArgv(0));
}
void WritableStreamDefaultControllerCloseAlgorithmResolved(xsMachine* the)
{
	WritableStreamDefaultController* controller = (WritableStreamDefaultController*)fxGetHostFunctionHandle(the);
	WritableStream* stream = (*controller)->stream;
	WritableStreamFinishInFlightClose(the, stream);
}
void WritableStreamDefaultControllerCloseAlgorithmRejected(xsMachine* the)
{
	WritableStreamDefaultController* controller = (WritableStreamDefaultController*)fxGetHostFunctionHandle(the);
	WritableStream* stream = (*controller)->stream;
	WritableStreamFinishInFlightCloseWithError(the, stream, mxArgv(0));
}
void WritableStreamDefaultControllerAbortAlgorithmResolved(xsMachine* the)
{
	WritableStreamDefaultController* controller = (WritableStreamDefaultController*)fxGetHostFunctionHandle(the);
	WritableStream* stream = (*controller)->stream;
	fxResolvePromiseRecord(the, (*stream)->pendingAbortRequest, &mxUndefined);
	(*stream)->pendingAbortRequest = NULL;
	WritableStreamRejectCloseAndClosedPromiseIfNeeded(the, stream);
}
void WritableStreamDefaultControllerAbortAlgorithmRejected(xsMachine* the)
{
	WritableStreamDefaultController* controller = (WritableStreamDefaultController*)fxGetHostFunctionHandle(the);
	WritableStream* stream = (*controller)->stream;
	fxRejectPromiseRecord(the, (*stream)->pendingAbortRequest, mxArgv(0));
	(*stream)->pendingAbortRequest = NULL;
	WritableStreamRejectCloseAndClosedPromiseIfNeeded(the, stream);
}

