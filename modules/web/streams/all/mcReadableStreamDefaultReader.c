#include "mcStreamAll.h"

static void ReadableStreamDefaultReaderCancel(xsMachine* the, ReadableStreamReader* it);
static void ReadableStreamDefaultReaderClose(xsMachine* the, ReadableStreamReader* it);
static void ReadableStreamDefaultReaderError(xsMachine* the, ReadableStreamReader* it, xsSlot* e);

static const ReadableStreamReaderDispatchRecord ReadableStreamDefaultReaderDispatchRecord = {
	"ReadableStreamDefaultReader",
	ReadableStreamDefaultReaderCancel,
	ReadableStreamDefaultReaderClose,
	ReadableStreamDefaultReaderError,
};

// 4.4 ReadableStreamDefaultReader

static void ReadableStreamDefaultReader_mark(xsMachine* the, void* it, xsMarkRoot markRoot);
static const xsHostHooks ReadableStreamDefaultReaderHooks ICACHE_RODATA_ATTR = {
	ReadableStreamDefaultReader_destructor,
	ReadableStreamDefaultReader_mark,
	NULL
};

void ReadableStreamDefaultReader_constructor(xsMachine* the)
{
	ReadableStream* stream = NULL;
	if (mxArgc > 0)
		stream = mxStreamHandle(ReadableStream, mxArgv(0));
	else
		xsTypeError("no stream");
	if (IsReadableStreamLocked(the, stream))
		xsTypeError("stream locked");
	
	xsSetHostChunk(xsThis, NULL, sizeof(ReadableStreamDefaultReaderRecord));
	xsSetHostHooks(xsThis, (xsHostHooks*)&ReadableStreamDefaultReaderHooks);
	ReadableStreamDefaultReader* reader = (ReadableStreamDefaultReader*)fxGetHostHandle(the, mxThis);
	(*reader)->reference = xsToReference(xsThis);
	(*reader)->dispatch = (ReadableStreamReaderDispatch)&ReadableStreamDefaultReaderDispatchRecord;
	
	(*reader)->stream = stream;
	(*stream)->reader = (ReadableStreamReader*)reader;
	ReadableStreamReaderGenericInitialize(the, (ReadableStreamReader*)reader);
	
	(*reader)->queue = CreateSlotQueue(the);
}

void ReadableStreamDefaultReader_destructor(void* it)
{
}

void ReadableStreamDefaultReader_mark(xsMachine* the, void* it, xsMarkRoot markRoot)
{
	ReadableStreamDefaultReader self = it;
	StreamMarkHandle(the, self->stream);
	StreamMarkReference(the, self->closedPromise);
	StreamMarkReference(the, self->queue);
	StreamMarkHandle(the, self->context);
}

void ReadableStreamDefaultReader_get_closed(xsMachine* the)
{
	ReadableStreamDefaultReader* reader = mxStreamHandle(ReadableStreamDefaultReader, mxThis);
	fxGetPromiseRecordPromise(the, (*reader)->closedPromise, mxResult);
}

void ReadableStreamDefaultReader_cancel(xsMachine* the)
{
	ReadableStreamDefaultReader* reader = mxStreamHandle(ReadableStreamDefaultReader, mxThis);
	if (!(*reader)->stream) {
		mxReturnPromiseRejectedWithTypeError("no stream");
	}
	if (mxArgc > 0)
		mxPushSlot(mxArgv(0));
	else
		mxPushUndefined();
	ReadableStreamCancel(the, (*reader)->stream, the->stack, mxResult);
	mxPop();
}

void ReadableStreamDefaultReader_read(xsMachine* the)
{
	ReadableStreamDefaultReader* reader = mxStreamHandle(ReadableStreamDefaultReader, mxThis);
	if (!(*reader)->stream) {
		mxReturnPromiseRejectedWithTypeError("no stream");
	}
	(*reader)->chunkSteps = ReadableStreamReaderChunkSteps;
	(*reader)->closeSteps = ReadableStreamReaderCloseSteps;
	(*reader)->errorSteps = ReadableStreamReaderErrorSteps;
	xsSlot* record = fxCreatePromiseRecord(the, mxResult);
	ReadableStreamDefaultReaderRead(the, reader, mxResult);
	fxGetPromiseRecordPromise(the, record, mxResult);
}

void ReadableStreamDefaultReader_releaseLock(xsMachine* the)
{
	ReadableStreamDefaultReader* reader = mxStreamHandle(ReadableStreamDefaultReader, mxThis);
	if (!(*reader)->stream)
		return;
	ReadableStreamReaderGenericRelease(the, (ReadableStreamReader*)reader);	
}

// 4.9.3 Readers
ReadableStreamDefaultReader* AcquireReadableStreamDefaultReader(xsMachine* the, ReadableStream* stream)
{
	mxPushReference((*stream)->closures);
	mxGetID(xsID_ReadableStreamDefaultReader);
	mxNew();
	mxPushReference((*stream)->reference);
	mxRunCount(1);
	mxPop();
	return (ReadableStreamDefaultReader*)((*stream)->reader);
}
txBoolean IsReadableStreamDefaultReader(xsMachine* the, ReadableStreamReader* reader)
{
	return ((*reader)->dispatch == &ReadableStreamDefaultReaderDispatchRecord) ? 1 : 0;
}
void ReadableStreamReaderGenericInitialize(xsMachine* the, ReadableStreamReader* reader)
{
	ReadableStream* stream = (*reader)->stream;
	if ((*stream)->state == mcStreamReadable) {
		(*reader)->closedPromise = fxCreatePromiseRecord(the, NULL);
	}
	else if ((*stream)->state == mcStreamClosed) {
		(*reader)->closedPromise = fxCreateResolvedPromiseRecord(the, &mxUndefined, NULL);
	}
	else {
		mxStreamAssert((*stream)->state == mcStreamErrored);
		(*reader)->closedPromise = fxCreateRejectedPromiseRecord(the, (*stream)->storedError, NULL);
		fxHandlePromiseRecord(the, (*reader)->closedPromise, (*stream)->closures);
	}
}
void ReadableStreamReaderGenericRelease(xsMachine* the, ReadableStreamReader* reader)
{
	ReadableStream* stream = (*reader)->stream;
	mxPush(mxTypeErrorConstructor);
	fxNewError(the, "release");
	xsSlot* error = the->stack;
	if ((*stream)->state == mcStreamReadable)
		fxRejectPromiseRecord(the, (*reader)->closedPromise, error);
	else
		(*reader)->closedPromise = fxCreateRejectedPromiseRecord(the, error, NULL);
	fxHandlePromiseRecord(the, (*reader)->closedPromise, (*stream)->closures);
	(*(*((*stream)->controller))->dispatch->releaseSteps)(the, (*stream)->controller);
	(*stream)->reader = NULL;
	(*reader)->stream = NULL;
	(*((*reader)->dispatch->error))(the, reader, error);
	mxPop(); // error
}
void ReadableStreamDefaultReaderCancel(xsMachine* the, ReadableStreamReader* reader)
{
}
void ReadableStreamDefaultReaderClose(xsMachine* the, ReadableStreamReader* reader)
{
	xsSlot* request;
	mxTemporary(request);
	while (GetSlotQueueLength(the, (*reader)->queue) > 0) {
		DequeueSlot(the, (*reader)->queue, request);
		(*(*reader)->closeSteps)(the, reader, request, NULL);
	}
	mxPop();
}
void ReadableStreamDefaultReaderError(xsMachine* the, ReadableStreamReader* reader, xsSlot* e)
{
	xsSlot* request;
	mxTemporary(request);
	while (GetSlotQueueLength(the, (*reader)->queue) > 0) {
		DequeueSlot(the, (*reader)->queue, request);
		(*(*reader)->errorSteps)(the, reader, request, e);
	}
	mxPop();
}
void ReadableStreamDefaultReaderRead(xsMachine* the, ReadableStreamDefaultReader* reader, xsSlot* request)
{
	ReadableStream* stream = (*reader)->stream;
	(*stream)->disturbed = 1;
	if ((*stream)->state == mcStreamClosed) {
		(*(*reader)->closeSteps)(the, (ReadableStreamReader*)reader, request, NULL);
	}
	else if ((*stream)->state == mcStreamErrored) {
		mxPushSlot((*stream)->storedError);
		(*(*reader)->errorSteps)(the, (ReadableStreamReader*)reader, request, the->stack);
		mxPop();
	}
	else {
		// assert(state == "readable");
		(*(*((*stream)->controller))->dispatch->pullSteps)(the, (*stream)->controller, request);
	}
}

