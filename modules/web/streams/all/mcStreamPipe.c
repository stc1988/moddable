#include "mcStreamAll.h"

typedef struct StreamPipeOptionsStruct StreamPipeOptionsRecord, *StreamPipeOptions;
typedef struct StreamPipeStruct StreamPipeRecord, *StreamPipe;

struct StreamPipeOptionsStruct {
	txBoolean preventClose;
	txBoolean preventAbort;
	txBoolean preventCancel;
	xsSlot* signal;
};

struct StreamPipeStruct {
	StreamHandlePart;
	ReadableStream* readableStream;
	WritableStream* writableStream;
	ReadableStreamDefaultReader* reader;
	WritableStreamDefaultWriter* writer;
	xsSlot* signal;
	xsSlot* result;
	xsSlot* error;	
	
	xsSlot* abortAlgorithm;	
	xsSlot* ignore;
	xsSlot* loop;
	xsSlot* stepResolved;
	xsSlot* stepRejected;	
	
	txBoolean preventClose;
	txBoolean preventAbort;
	txBoolean preventCancel;
	txBoolean shuttingDown;
	uint8_t aborted;
	uint8_t isError;
	uint8_t phase;
	uint8_t shutdownAction;
};

static StreamPipe* CreateStreamPipe(xsMachine* the, txSlot* it);
static void DestroyStreamPipe(void* it);
static void MarkStreamPipe(xsMachine* the, void* it, xsMarkRoot markRoot);
static void StreamPipeAbort(xsMachine* the);
static void StreamPipeIgnore(xsMachine* the);
static void StreamPipeLoop(xsMachine* the);
static void StreamPipeStep(xsMachine* the, StreamPipe* pipe);
static void StreamPipeStepResolved(xsMachine* the);
static void StreamPipeStepRejected(xsMachine* the);
static void StreamPipeReaderChunkSteps(xsMachine* the, ReadableStreamReader* reader, xsSlot* request, xsSlot* chunk);
static void StreamPipeReaderCloseSteps(xsMachine* the, ReadableStreamReader* reader, xsSlot* request, xsSlot* chunk);
static void StreamPipeReaderErrorSteps(xsMachine* the, ReadableStreamReader* reader, xsSlot* request, xsSlot* error);
static void ReadableStreamPipeOptions(xsMachine* the, ReadableStream* readableStream, WritableStream* writableStream, StreamPipeOptions options);
static void ReadableStreamPipeTo(xsMachine* the, ReadableStream* readableStream, WritableStream* writableStream, StreamPipeOptions options, xsSlot* result);

enum {
	mxLoopPhase = 0,
	mxDrainPhase = 1,
	mxShutdownActionPhase = 2,
	mxFinalizePhase = 3,
};

enum {
	mxWritableStreamAbort = 1,
	mxReadableStreamCancel = 2,
	mxWritableStreamDefaultWriterCloseWithErrorPropagation = 4,
};

static const xsHostHooks StreamPipeHooks ICACHE_RODATA_ATTR = {
	DestroyStreamPipe,
	MarkStreamPipe,
	NULL
};

StreamPipe* CreateStreamPipe(xsMachine* the, txSlot* it)
{
	fxNewHostObject(the, NULL);
	mxPullSlot(it);
	fxSetHostChunk(the, it, NULL, sizeof(StreamPipeRecord));
	fxSetHostHooks(the, it, (xsHostHooks*)&StreamPipeHooks);
	
	return (StreamPipe*)&(it->value.reference->next->value.host.data);
}
void DestroyStreamPipe(void* it)
{
}
void MarkStreamPipe(xsMachine* the, void* it, xsMarkRoot markRoot)
{
	StreamPipe self = it;
	StreamMarkHandle(the, self->readableStream);
	StreamMarkHandle(the, self->writableStream);
	StreamMarkHandle(the, self->reader);
	StreamMarkHandle(the, self->writer);
	StreamMarkReference(the, self->signal);
	StreamMarkReference(the, self->result);
	StreamMarkReference(the, self->stepResolved);
	StreamMarkReference(the, self->stepRejected);
	StreamMarkReference(the, self->loop);
	StreamMarkReference(the, self->ignore);
	StreamMarkReference(the, self->abortAlgorithm);
}

void StreamPipeAbort(xsMachine* the)
{
	StreamPipe* pipe = (StreamPipe*)fxGetHostFunctionHandle(the);
	//fprintf(stderr, "StreamPipeAbort %d\n", (*pipe)->phase);
	if ((*pipe)->phase == 0) {
		(*pipe)->aborted = 1;
		StreamPipeStep(the, pipe);
	}
}
void StreamPipeIgnore(xsMachine* the)
{
	/* StreamPipe* pipe = (StreamPipe*) */ (void)fxGetHostFunctionHandle(the);
	//fprintf(stderr, "StreamPipeIgnore %d\n", (*pipe)->phase);
}
void StreamPipeLoop(xsMachine* the)
{
	StreamPipe* pipe = (StreamPipe*)fxGetHostFunctionHandle(the);
	//fprintf(stderr, "StreamPipeLoop %d\n", (*pipe)->phase);
	if ((*pipe)->phase == 0) {
		StreamPipeStep(the, pipe);
    }
}
void StreamPipeStep(xsMachine* the, StreamPipe* pipe)
{
	ReadableStream* readableStream = (*pipe)->readableStream;
	uint8_t readableState = (*readableStream)->state;
	ReadableStreamDefaultReader* reader = (*pipe)->reader;
	WritableStream* writableStream = (*pipe)->writableStream;
	uint8_t writableState = (*writableStream)->state;
	WritableStreamDefaultWriter* writer = (*pipe)->writer;
	xsSlot* promise;
	mxTemporary(promise);
	for(;;) {
		if ((*pipe)->phase == 0) {
			if ((*pipe)->aborted) {
				(*pipe)->isError = 1;
				mxPushReference((*pipe)->signal);
				mxGetID(xsID_reason);
				mxPullSlot((*pipe)->error);
				if ((!(*pipe)->preventAbort) && (writableState == mcStreamWritable)) {
					(*pipe)->shutdownAction |= mxWritableStreamAbort;
				}
				if ((!(*pipe)->preventCancel) && (readableState == mcStreamReadable)) {
					(*pipe)->shutdownAction |= mxReadableStreamCancel;
				}
				(*pipe)->phase = 1;
			}
			else if (readableState == mcStreamErrored) {
				(*pipe)->isError = 1;
				(*pipe)->error->kind = (*readableStream)->storedError->kind;
				(*pipe)->error->value = (*readableStream)->storedError->value;
				if (!(*pipe)->preventAbort) {
					(*pipe)->shutdownAction = mxWritableStreamAbort;
				}
				(*pipe)->phase = 1;
			}
			else if (writableState == mcStreamErrored) {
				(*pipe)->isError = 1;
				(*pipe)->error->kind = (*writableStream)->storedError->kind;
				(*pipe)->error->value = (*writableStream)->storedError->value;
				if (!(*pipe)->preventCancel) {
					(*pipe)->shutdownAction = mxReadableStreamCancel;
					(*pipe)->phase = 2;
				}
				else
					(*pipe)->phase = 3;
			}
			else if (readableState == mcStreamClosed) {
				if (!(*pipe)->preventClose) {
					(*pipe)->shutdownAction = mxWritableStreamDefaultWriterCloseWithErrorPropagation;
				}
				(*pipe)->phase = 1;
			}
			else if ((writableState == mcStreamClosed) || WritableStreamCloseQueuedOrInFlight(the, writableStream)) {
				(*pipe)->isError = 1;
				mxPush(mxTypeErrorConstructor);
				fxNewError(the, "the destination writable stream closed before all data could be piped to it");
				mxPullSlot((*pipe)->error);
				if (!(*pipe)->preventCancel) {
					(*pipe)->shutdownAction = mxReadableStreamCancel;
					(*pipe)->phase = 2;
				}
				else
					(*pipe)->phase = 3;
			}
			else if (writableState == mcStreamErroring) {
                break;
			}
			else {
				if (WritableStreamDefaultWriterGetDesiredSize(the, writer) > 0) {
					ReadableStreamDefaultReader* reader = (*pipe)->reader;
					xsSlot* record = fxCreatePromiseRecord(the, promise);
					ReadableStreamDefaultReaderRead(the, reader, promise);
					fxGetPromiseRecordPromise(the, record, promise);
				}
				else {
					fxGetPromiseRecordPromise(the, (*writer)->readyPromise, promise);
				}
				break;
			}
		}
		else if ((*pipe)->phase == 1) {
			if ((*writableStream)->inFlightWriteRequest && fxIsPromiseRecordPending(the, (*writableStream)->inFlightWriteRequest)) {
				fxGetPromiseRecordPromise(the, (*writableStream)->inFlightWriteRequest, promise);
				break;
			}
			else {
				(*pipe)->phase = ((*pipe)->shutdownAction) ? 2 : 3;
			}
		}
		else if ((*pipe)->phase == 2) {
			if ((*pipe)->shutdownAction & mxWritableStreamAbort) {
				(*pipe)->shutdownAction &= ~mxWritableStreamAbort;
				WritableStreamAbort(the, writableStream, (*pipe)->error, promise);
			}
			else if ((*pipe)->shutdownAction & mxReadableStreamCancel) {
				(*pipe)->shutdownAction &= ~mxReadableStreamCancel;
				ReadableStreamCancel(the, readableStream, (*pipe)->error, promise);
			}
			else if ((*pipe)->shutdownAction == mxWritableStreamDefaultWriterCloseWithErrorPropagation) {
				(*pipe)->shutdownAction &= ~mxWritableStreamDefaultWriterCloseWithErrorPropagation;
				WritableStreamDefaultWriterCloseWithErrorPropagation(the, writer, promise);
			}
			(*pipe)->phase = ((*pipe)->shutdownAction) ? 2 : 3;
			break;
		}
		else if ((*pipe)->phase == 3) {
			WritableStreamDefaultWriterRelease(the, writer);
			ReadableStreamReaderGenericRelease(the, (ReadableStreamReader*)reader);
			if ((*pipe)->signal) {
				mxPushReference((*pipe)->signal);
				mxDub();
				mxGetID(xsID_removeEventListener);
				mxCall();
				mxPushStringX("abort");
				mxPushReference((*pipe)->abortAlgorithm);
				mxRunCount(2);
				mxPop();
			}
			if ((*pipe)->isError)
				fxRejectPromiseRecord(the, (*pipe)->result, (*pipe)->error);
			else
				fxResolvePromiseRecord(the, (*pipe)->result, &mxUndefined);
			break;
		}
	}
	if (!mxIsUndefined(promise)) {
	//fprintf(stderr, "StreamPipeStep %d %p\n", (*pipe)->phase, promise->value.reference);
		if ((*pipe)->phase == 0)
			fxChainPromise(the, promise, (*pipe)->loop, (*pipe)->loop, NULL);
		else
			fxChainPromise(the, promise, (*pipe)->stepResolved, (*pipe)->stepRejected, NULL);
	}
	mxPop(); // promise
}
void StreamPipeStepResolved(xsMachine* the)
{
	StreamPipe* pipe = (StreamPipe*)fxGetHostFunctionHandle(the);
	//fprintf(stderr, "StreamPipeStepResolved %d\n", (*pipe)->phase);
	StreamPipeStep(the, pipe);
}
void StreamPipeStepRejected(xsMachine* the)
{
	StreamPipe* pipe = (StreamPipe*)fxGetHostFunctionHandle(the);
	if ((*pipe)->isError < 2) {
		(*pipe)->isError = 2;
		(*pipe)->error->kind = mxArgv(0)->kind;
		(*pipe)->error->value = mxArgv(0)->value;
	}
	//fprintf(stderr, "StreamPipeStepRejected %d\n", (*pipe)->phase);
	StreamPipeStep(the, pipe);
}

void StreamPipeReaderChunkSteps(xsMachine* the, ReadableStreamReader* reader, xsSlot* request, xsSlot* chunk)
{
	StreamPipe* pipe = (StreamPipe*)(*reader)->context;
	WritableStreamDefaultWriter* writer = (*pipe)->writer;
	xsSlot* promise;
	mxTemporary(promise);
	WritableStreamDefaultWriterWrite(the, writer, chunk, promise);
	fxChainPromise(the, promise, (*pipe)->ignore, (*pipe)->ignore, NULL);
	mxPop();
	//fprintf(stderr, "StreamPipeReaderChunkSteps %d\n", (*pipe)->phase);
	fxResolvePromiseRecord(the, fxToReference(the, request), &mxUndefined);
// 	mxPushUndefined();
// 	mxPushReference((*pipe)->loop);
// 	mxCall();	
// 	fxQueueJob(the, 0, C_NULL);
}
void StreamPipeReaderCloseSteps(xsMachine* the, ReadableStreamReader* reader, xsSlot* request, xsSlot* chunk)
{
}
void StreamPipeReaderErrorSteps(xsMachine* the, ReadableStreamReader* reader, xsSlot* request, xsSlot* error)
{
}

void ReadableStreamPipeOptions(xsMachine* the, ReadableStream* readableStream, WritableStream* writableStream, StreamPipeOptions options)
{
	if (mxArgc > 1) {
		mxPushSlot(mxArgv(1));
		if (fxRunTest(the)) {
			mxPushSlot(mxArgv(1));
			mxGetID(xsID_preventAbort);
			options->preventAbort = fxToBoolean(the, the->stack);
			mxPop();
			mxPushSlot(mxArgv(1));
			mxGetID(xsID_preventCancel);
			options->preventCancel = fxToBoolean(the, the->stack);
			mxPop();
			mxPushSlot(mxArgv(1));
			mxGetID(xsID_preventClose);
			options->preventClose = fxToBoolean(the, the->stack);
			mxPop();
			mxPushSlot(mxArgv(1));
			mxGetID(xsID_signal);
			if (!mxIsUndefined(the->stack)) {
				xsSlot* signal = the->stack;
				mxPushReference((*readableStream)->closures);
				mxGetID(xsID_AbortSignal);
				mxGetID(xsID_prototype);
				mxPushSlot(signal);
				if (!fxIsInstanceOf(the)) {
					mxTypeError("invalid signal");
				}
				mxPushSlot(signal);
				mxGetID(xsID_aborted);
				mxPop();
				options->signal = fxToReference(the, signal);
			}
			mxPop();
		}
	}
	if (IsReadableStreamLocked(the, readableStream)) {
		mxTypeError("source readable stream locked");
	}
	if (IsWritableStreamLocked(the, writableStream)) {
		mxTypeError("destination writable stream locked");
	}
}

void ReadableStreamPipeTo(xsMachine* the, ReadableStream* readableStream, WritableStream* writableStream, StreamPipeOptions options, xsSlot* result)
{
	txSlot* it;
	mxTemporary(it);
	StreamPipe* pipe = CreateStreamPipe(the, it);
	(*pipe)->reference = it->value.reference;
	(*pipe)->readableStream = readableStream;
	(*pipe)->writableStream = writableStream;
	ReadableStreamDefaultReader* reader = (*pipe)->reader = AcquireReadableStreamDefaultReader(the, readableStream);
	WritableStreamDefaultWriter* writer = (*pipe)->writer = AcquireWritableStreamDefaultWriter(the, writableStream);
	(*pipe)->preventClose = options->preventClose;
	(*pipe)->preventAbort = options->preventAbort;
	(*pipe)->preventCancel = options->preventCancel;
	(*pipe)->signal = options->signal;
	(*pipe)->result = fxCreatePromiseRecord(the, result);
	
	(*pipe)->abortAlgorithm = fxNewHostFunctionWithHandle(the, pipe, StreamPipeAbort, 1); mxPop();
	(*pipe)->ignore = fxNewHostFunctionWithHandle(the, pipe, StreamPipeIgnore, 1); mxPop();
	(*pipe)->loop = fxNewHostFunctionWithHandle(the, pipe, StreamPipeLoop, 1); mxPop();
	(*pipe)->stepResolved = fxNewHostFunctionWithHandle(the, pipe, StreamPipeStepResolved, 1); mxPop();
	(*pipe)->stepRejected = fxNewHostFunctionWithHandle(the, pipe, StreamPipeStepRejected, 1); mxPop();

	// error can be any value...
	txSlot* slot = fxLastProperty(the, (*pipe)->reference);
	slot = slot->next = fxNewSlot(the);
	slot->flag |= XS_INTERNAL_FLAG;
	slot->kind = XS_UNINITIALIZED_KIND;
	(*pipe)->error = slot;

	(*reader)->context = pipe;
	(*reader)->chunkSteps = StreamPipeReaderChunkSteps;
	(*reader)->closeSteps = StreamPipeReaderCloseSteps;
	(*reader)->errorSteps = StreamPipeReaderErrorSteps;
	
	mxPushUndefined();
	fxGetPromiseRecordPromise(the, (*reader)->closedPromise, the->stack);
	fxChainPromise(the, the->stack, (*pipe)->loop, (*pipe)->loop, NULL);
	mxPop();

	mxPushUndefined();
	fxGetPromiseRecordPromise(the, (*writer)->closedPromise, the->stack);
	fxChainPromise(the, the->stack, (*pipe)->loop, (*pipe)->loop, NULL);
	mxPop();
	
	if ((*pipe)->signal) {
		mxPushReference((*pipe)->signal);
		mxGetID(xsID_aborted);
		if (fxRunTest(the)) {
			(*pipe)->aborted = 1;
		}
		else {		
			mxPushReference((*pipe)->signal);
			mxDub();
			mxGetID(xsID_addEventListener);
			mxCall();
			mxPushStringX("abort");
			mxPushReference((*pipe)->abortAlgorithm);
			mxRunCount(2);
			mxPop();
		}
	}
	
	(*readableStream)->disturbed = 1;
	//fprintf(stderr, "ReadableStreamPipeTo %d\n", (*pipe)->phase);
	StreamPipeStep(the, pipe);
}

void ReadableStream_pipeThrough(xsMachine* the)
{
	ReadableStream* readableStream = NULL;
	WritableStream* writableStream = NULL;
	StreamPipeOptionsRecord options = { 0 };
	readableStream = mxStreamHandle(ReadableStream, mxThis);
	mxPushSlot(mxArgv(0));
	mxGetID(xsID_readable);
	mxStreamHandle(ReadableStream, the->stack);
	mxPop();
	mxPushSlot(mxArgv(0));
	mxGetID(xsID_writable);
	writableStream = mxStreamHandle(WritableStream, the->stack);
	mxPop();
	ReadableStreamPipeOptions(the, readableStream, writableStream, &options);	
	ReadableStreamPipeTo(the, readableStream, writableStream, &options, mxResult);
	fxHandlePromiseRecord(the, fxToReference(the, mxResult), (*readableStream)->closures);
	mxPushSlot(mxArgv(0));
	mxGetID(xsID_readable);
	mxPullSlot(mxResult);
}

void ReadableStream_pipeTo(xsMachine* the)
{
	ReadableStream* readableStream = NULL;
	WritableStream* writableStream = NULL;
	StreamPipeOptionsRecord options = { 0 };
	mxTry(the) {
		readableStream = mxStreamHandle(ReadableStream, mxThis);
		writableStream = mxStreamHandle(WritableStream, mxArgv(0));
		ReadableStreamPipeOptions(the, readableStream, writableStream, &options);	
	}
	mxCatch(the) {
		fxCreateRejectedPromise(the, &mxException, mxResult);				
		mxException = mxUndefined;
		return;
	}
	ReadableStreamPipeTo(the, readableStream, writableStream, &options, mxResult);
	fxGetPromiseRecordPromise(the, fxToReference(the, mxResult), mxResult);
}
