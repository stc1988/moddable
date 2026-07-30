#include "xsAll.h"
#include "mc.xs.h"

extern void fxNewError(xsMachine* the, xsStringValue format, ...);

// Promises

extern void fxChainPromise(xsMachine* the, xsSlot* promise, xsSlot* resolved, xsSlot* rejected, xsSlot* result);
extern xsSlot* fxCreatePromiseRecord(xsMachine* the, xsSlot* record);
extern void fxCreateRejectedPromise(xsMachine* the, xsSlot* reason, xsSlot* promise);
extern xsSlot* fxCreateRejectedPromiseRecord(xsMachine* the, xsSlot* reason, xsSlot* record);
extern void fxCreateResolvedPromise(xsMachine* the, xsSlot* value, xsSlot* promise);
extern xsSlot* fxCreateResolvedPromiseRecord(xsMachine* the, xsSlot* value, xsSlot* record);
extern void fxGetPromiseRecordPromise(xsMachine* the, xsSlot* record, xsSlot* promise);
extern void fxHandlePromiseRecord(xsMachine* the, xsSlot* record, xsSlot* closures);
extern txBoolean fxIsThenableObject(xsMachine* the, xsSlot* slot);
extern txBoolean fxIsPromiseRecordPending(xsMachine* the, xsSlot* record);
extern void fxRejectPromiseRecord(xsMachine* the, xsSlot* record, xsSlot* reason);
extern void fxResolvePromiseRecord(xsMachine* the, xsSlot* record, xsSlot* value);

#define mxReturnPromiseRejectedWithRangeError(...) \
	mxPush(mxRangeErrorConstructor); \
	fxNewError(the, __VA_ARGS__); \
	fxCreateRejectedPromise(the, the->stack, mxResult); \
	mxPop(); \
	return
#define mxReturnPromiseRejectedWithTypeError(...) \
	mxPush(mxTypeErrorConstructor); \
	fxNewError(the, __VA_ARGS__); \
	fxCreateRejectedPromise(the, the->stack, mxResult); \
	mxPop(); \
	return

// Assertions

extern void fxStreamAssert(xsMachine* the, char* file, int line);
#ifdef mxDebug
#define mxStreamAssert(THE_ASSERTION) \
	if (!(THE_ASSERTION)) \
		fxStreamAssert(the, __FILE__,__LINE__)
#else
#define mxStreamAssert(THE_ASSERTION)
#endif

// Handles

#define StreamHandlePart \
	xsSlot* reference

#define StreamDispatchPart \
	StreamDispatch dispatch

typedef struct {
	StreamHandlePart;
} StreamHandleRecord, *StreamHandle;

typedef struct {
	char* type;
} StreamDispatchRecord, *StreamDispatch;

typedef struct {
	StreamHandlePart;
	StreamDispatchPart;
} StreamStuffRecord, *StreamStuff;

typedef struct {
	txBoolean (*call)(xsMachine* the, StreamStuff* controller, xsSlot* param, xsSlot* result);
	xsSlot* callback;
	xsSlot* resolved;
	xsSlot* rejected;
} StreamAlgorithmRecord, *StreamAlgorithm;

extern void fxChainAlgorithm(xsMachine* the, xsSlot* result, txBoolean success, xsSlot* resolved, xsSlot* rejected);
extern void StreamMarkAlgorithm(xsMachine* the, StreamAlgorithm algorithm, xsMarkRoot markRoot);

extern void** fxStreamHandle(xsMachine* the, xsSlot* it, StreamDispatch dispatch);
#define mxStreamHandle(CAST, SLOT) ((CAST*)fxStreamHandle(the, (SLOT), (StreamDispatch)(&CAST##DispatchRecord)))
#define StreamMarkHandle(THE, HANDLE) if (HANDLE) (*markRoot)(THE, (*((StreamHandle*)(HANDLE)))->reference)
#define StreamMarkReference(THE, REFERENCE) if (REFERENCE) (*markRoot)(THE, REFERENCE)
extern xsSlot* fxNewHostFunctionWithHandle(txMachine* the, void* handle, txCallback callback, txInteger length);
extern void* fxGetHostFunctionHandle(txMachine* the);

enum {
	mcStreamReadable = 0,
	mcStreamWritable = 0,
	mcStreamClosed = 1,
	mcStreamErrored = 2,
	mcStreamErroring = 3,
};

typedef struct ReadableStreamStruct ReadableStreamRecord, *ReadableStream;

typedef struct ReadableStreamReaderDispatchStruct ReadableStreamReaderDispatchRecord, *ReadableStreamReaderDispatch;
typedef struct ReadableStreamReaderStruct ReadableStreamReaderRecord, *ReadableStreamReader;
typedef struct ReadableStreamDefaultReaderStruct ReadableStreamDefaultReaderRecord, *ReadableStreamDefaultReader;
typedef struct ReadableStreamBYOBReaderStruct ReadableStreamBYOBReaderRecord, *ReadableStreamBYOBReader;
typedef struct ReadableStreamBYOBRequestStruct ReadableStreamBYOBRequestRecord, *ReadableStreamBYOBRequest;

typedef struct ReadableStreamControllerStruct ReadableStreamControllerRecord, *ReadableStreamController;
typedef struct ReadableStreamDefaultControllerStruct ReadableStreamDefaultControllerRecord, *ReadableStreamDefaultController;
typedef struct ReadableByteStreamControllerStruct ReadableByteStreamControllerRecord, *ReadableByteStreamController;
typedef struct ReadableStreamControllerDispatchStruct ReadableStreamControllerDispatchRecord, *ReadableStreamControllerDispatch;

typedef struct WritableStreamStruct WritableStreamRecord, *WritableStream;
typedef struct WritableStreamDefaultWriterStruct WritableStreamDefaultWriterRecord, *WritableStreamDefaultWriter;
typedef struct WritableStreamDefaultControllerStruct WritableStreamDefaultControllerRecord, *WritableStreamDefaultController;


extern const StreamDispatchRecord ReadableStreamDispatchRecord;
struct ReadableStreamStruct {
	StreamHandlePart;
	StreamDispatchPart;
	xsSlot* closures;
	ReadableStreamController* controller;
	ReadableStreamReader* reader;
	
	xsSlot* storedError;
	uint8_t disturbed;
	uint8_t state;
};

typedef void (*ReadRequestSteps)(xsMachine*, ReadableStreamReader* reader, xsSlot*, xsSlot*);
struct ReadableStreamReaderDispatchStruct {
	char* type;
	void (*cancel)(xsMachine* the, ReadableStreamReader* reader);
	void (*close)(xsMachine* the, ReadableStreamReader* reader);
	void (*error)(xsMachine* the, ReadableStreamReader* reader, xsSlot* e);
};
#define ReadableStreamReaderPart \
	ReadableStreamReaderDispatch dispatch; \
	ReadableStream* stream; \
	xsSlot* closedPromise; \
	xsSlot* queue; \
	ReadRequestSteps chunkSteps; \
	ReadRequestSteps closeSteps; \
	ReadRequestSteps errorSteps; \
	void* context
	
struct ReadableStreamReaderStruct {
	StreamHandlePart;
	ReadableStreamReaderPart;
};
struct ReadableStreamDefaultReaderStruct {
	StreamHandlePart;
	ReadableStreamReaderPart;
};
struct ReadableStreamBYOBReaderStruct {
	StreamHandlePart;
	ReadableStreamReaderPart;
};
struct ReadableStreamBYOBRequestStruct {
	StreamHandlePart;
	StreamDispatchPart;
	ReadableByteStreamController* controller;
	xsSlot* view;
};

#define ReadableStreamControllerPart \
	ReadableStreamControllerDispatch dispatch; \
	ReadableStream* stream; \
	xsNumberValue strategyHWM; \
	xsSlot* target; \
	StreamAlgorithmRecord startAlgorithm; \
	StreamAlgorithmRecord pullAlgorithm; \
	StreamAlgorithmRecord cancelAlgorithm; \
	uint8_t started; \
	uint8_t closeRequested; \
	uint8_t pullAgain; \
	uint8_t pulling; \
	uint8_t branch
struct ReadableStreamControllerStruct {
	StreamHandlePart;
	ReadableStreamControllerPart;
};
struct ReadableStreamDefaultControllerStruct {
	StreamHandlePart;
	ReadableStreamControllerPart;
	xsSlot* strategySizeAlgorithm;
	xsSlot* queue;
};
struct ReadableByteStreamControllerStruct {
	StreamHandlePart;
	ReadableStreamControllerPart;
	xsSlot* queue;
	xsSlot* pendingPullIntos;
	ReadableStreamBYOBRequest* byobRequest;
	txSize autoAllocateChunkSize;
};
struct ReadableStreamControllerDispatchStruct {
	char* type;
	void (*cancelSteps)(xsMachine* the, ReadableStreamController* controller, txSlot* reason, xsSlot* resuls);
	void (*pullSteps)(xsMachine* the, ReadableStreamController* controller, txSlot* request);
	void (*releaseSteps)(xsMachine* the, ReadableStreamController* controller);
};

extern const StreamDispatchRecord WritableStreamDispatchRecord;
struct WritableStreamStruct {
	StreamHandlePart;
	StreamDispatchPart;
	xsSlot* closures;
	WritableStreamDefaultController* controller;
	WritableStreamDefaultWriter* writer;
	xsSlot* storedError;
	
	xsSlot* writeRequests;
	xsSlot* closeRequest;
	xsSlot* inFlightWriteRequest;
	xsSlot* inFlightCloseRequest;
	xsSlot* pendingAbortRequest;
	xsSlot* pendingAbortRequestReason;
	
	uint8_t backpressure;
	uint8_t pendingAbortRequestWasAlreadyErroring;
	uint8_t state;
};
struct WritableStreamDefaultWriterStruct {
	StreamHandlePart;
	StreamDispatchPart;
	WritableStream* stream;
	xsSlot* readyPromise;
	xsSlot* closedPromise;
};
struct WritableStreamDefaultControllerStruct {
	StreamHandlePart;
	StreamDispatchPart;
	WritableStream* stream;
	xsSlot* abortController;
	xsSlot* strategySizeAlgorithm;
	xsSlot* target;
	StreamAlgorithmRecord startAlgorithm;
	StreamAlgorithmRecord writeAlgorithm;
	StreamAlgorithmRecord closeAlgorithm;
	StreamAlgorithmRecord abortAlgorithm;
	xsSlot* queue;
	uint8_t started;
	xsNumberValue strategyHWM;
};


extern xsSlot* ExtractAlgorithmReference(xsMachine* the, xsSlot* target, txID id);
extern xsNumberValue ExtractHighWaterMark(xsMachine* the, xsSlot* strategy, xsNumberValue defaultValue);
extern xsSlot* ExtractSizeAlgorithm(xsMachine* the, xsSlot* strategy);

extern xsSlot* CreateValueSizeQueue(xsMachine* the);
extern void DequeueValueSize(xsMachine* the, xsSlot* queue, xsSlot* value);
extern void EnqueueValueSize(xsMachine* the, xsSlot* queue, xsSlot* value, txNumber size);
extern txInteger GetQueueLength(xsMachine* the, xsSlot* queue);
extern txNumber GetQueueTotalSize(xsMachine* the, xsSlot* queue);
extern void PeekQueueValue(xsMachine* the, xsSlot* queue, xsSlot* result, txNumber* size);
extern void ResetQueue(xsMachine* the, xsSlot* queue);

extern xsSlot* CreateSlotQueue(xsMachine* the);
extern void DequeueSlot(xsMachine* the, xsSlot* queue, xsSlot* it);
extern void EnqueueSlot(xsMachine* the, xsSlot* queue, xsSlot* it);
extern txInteger GetSlotQueueLength(xsMachine* the, xsSlot* queue);
extern void PeekSlotQueue(xsMachine* the, xsSlot* queue, xsSlot* it);
extern void ResetSlotQueue(xsMachine* the, xsSlot* queue);
extern void ShiftSlotQueue(xsMachine* the, xsSlot* queue);

extern void fxNewStreamResult(txMachine* the, txBoolean done);

extern void ReadableStreamReaderGenericInitialize(xsMachine* the, ReadableStreamReader* reader);
extern void ReadableStreamReaderGenericRelease(xsMachine* the, ReadableStreamReader* reader);

extern ReadableStream* CreateReadableStream(xsMachine* the, xsSlot* it, xsSlot* closures);
extern ReadableStream* InitializeReadableStream(xsMachine* the, xsSlot* it, xsSlot* closures);
extern void ReadableStreamCancel(xsMachine* the, ReadableStream* stream, xsSlot* reason, xsSlot* result);
extern void ReadableStreamClose(xsMachine* the, ReadableStream* self);
extern void ReadableStreamError(xsMachine* the, ReadableStream* self, xsSlot* error);

extern void ReadableStreamAddReadRequest(xsMachine* the, ReadableStream* self, xsSlot* request);
extern void ReadableStreamFulfillReadRequest(xsMachine* the, ReadableStream* self, xsSlot* chunk, txBoolean done);
extern txInteger ReadableStreamGetNumReadRequests(xsMachine* the, ReadableStream* self);

extern void ReadableStreamReaderChunkSteps(xsMachine* the, ReadableStreamReader* reader, xsSlot* request, xsSlot* chunk);
extern void ReadableStreamReaderCloseSteps(xsMachine* the, ReadableStreamReader* reader, xsSlot* request, xsSlot* chunk);
extern void ReadableStreamReaderErrorSteps(xsMachine* the, ReadableStreamReader* reader, xsSlot* request, xsSlot* error);

extern txBoolean IsReadableStreamLocked(xsMachine* the, ReadableStream* self);
extern txBoolean IsReadableByteStreamController(xsMachine* the, ReadableStreamController* controller);

extern ReadableStreamDefaultController* CreateReadableStreamDefaultController(xsMachine* the, ReadableStream* stream);
extern txBoolean ReadableStreamDefaultControllerCanCloseOrEnqueue(xsMachine* the, ReadableStreamDefaultController* controller);
extern void ReadableStreamDefaultControllerClose(xsMachine* the, ReadableStreamDefaultController* controller);
extern void ReadableStreamDefaultControllerEnqueue(xsMachine* the, ReadableStreamDefaultController* controller, xsSlot* chunk);
extern void ReadableStreamDefaultControllerError(xsMachine* the, ReadableStreamDefaultController* controller, xsSlot* error);
extern txNumber ReadableStreamDefaultControllerGetDesiredSize(xsMachine* the, ReadableStreamDefaultController* controller);
extern txBoolean ReadableStreamDefaultControllerHasBackpressure(xsMachine* the, ReadableStreamDefaultController* controller);
extern void ReadableStreamDefaultControllerStart(xsMachine* the, ReadableStreamDefaultController* controller);

extern ReadableStreamDefaultReader* AcquireReadableStreamDefaultReader(xsMachine* the, ReadableStream* stream);
extern txBoolean IsReadableStreamDefaultReader(xsMachine* the, ReadableStreamReader* reader);
extern void ReadableStreamDefaultReaderRead(xsMachine* the, ReadableStreamDefaultReader* reader, xsSlot* request);

extern ReadableByteStreamController* CreateReadableByteStreamController(xsMachine* the, ReadableStream* stream);
extern void ReadableByteStreamControllerClose(xsMachine* the, ReadableByteStreamController* controller);
extern void ReadableByteStreamControllerEnqueue(xsMachine* the, ReadableByteStreamController* controller, xsSlot* chunk);
extern void ReadableByteStreamControllerError(xsMachine* the, ReadableByteStreamController* controller, xsSlot* error);
extern void ReadableByteStreamControllerGetBYOBRequest(xsMachine* the, ReadableByteStreamController* controller);
extern void ReadableByteStreamControllerPullInto(xsMachine* the, ReadableByteStreamController* controller, txSlot* view, txSize min, txSlot* readIntoRequest);
extern void ReadableByteStreamControllerRespondWithNewView(xsMachine* the, ReadableByteStreamController* controller, txSlot* view);
extern void ReadableByteStreamControllerRespond(xsMachine* the, ReadableByteStreamController* controller, txInteger bytesWritten);
extern void ReadableByteStreamControllerStart(xsMachine* the, ReadableByteStreamController* controller);

extern ReadableStreamReader* AcquireReadableStreamBYOBReader(xsMachine* the, ReadableStream* stream);
extern txBoolean IsReadableStreamBYOBReader(xsMachine* the, ReadableStreamReader* reader);
extern void ReadableStreamBYOBReaderRead(xsMachine* the, ReadableStreamBYOBReader* reader, xsSlot* view, txInteger min, xsSlot* request);

extern WritableStream* CreateWritableStream(xsMachine* the, xsSlot* it, xsSlot* closures);
extern WritableStream* InitializeWritableStream(xsMachine* the, xsSlot* it, xsSlot* closures);
extern txBoolean IsWritableStreamLocked(xsMachine* the, WritableStream* stream);
extern void WritableStreamAbort(xsMachine* the, WritableStream* stream, txSlot* reason, txSlot* promise);
extern txSlot* WritableStreamAddWriteRequest(xsMachine* the, WritableStream* stream);
extern void WritableStreamClose(xsMachine* the, WritableStream* stream, txSlot* promise);
extern txBoolean WritableStreamCloseQueuedOrInFlight(xsMachine* the, WritableStream* stream);
extern void WritableStreamDealWithRejection(xsMachine* the, WritableStream* stream, txSlot* error);
extern void WritableStreamFinishInFlightClose(xsMachine* the, WritableStream* stream);
extern void WritableStreamFinishInFlightCloseWithError(xsMachine* the, WritableStream* stream, txSlot* error);
extern void WritableStreamFinishInFlightWrite(xsMachine* the, WritableStream* stream);
extern void WritableStreamFinishInFlightWriteWithError(xsMachine* the, WritableStream* stream, txSlot* error);
extern void WritableStreamFinishErroring(xsMachine* the, WritableStream* stream);
extern void WritableStreamMarkCloseRequestInFlight(xsMachine* the, WritableStream* stream);
extern void WritableStreamMarkFirstWriteRequestInFlight(xsMachine* the, WritableStream* stream);
extern void WritableStreamStartErroring(xsMachine* the, WritableStream* stream, txSlot* reason);
extern void WritableStreamUpdateBackpressure(xsMachine* the, WritableStream* stream, txBoolean backpressure);
extern void WritableStreamRejectCloseAndClosedPromiseIfNeeded(xsMachine* the, WritableStream* stream);

extern WritableStreamDefaultController* CreateWritableStreamDefaultController(xsMachine* the, WritableStream* stream);
extern void WritableStreamDefaultControllerClose(xsMachine* the, WritableStreamDefaultController* controller);
extern void WritableStreamDefaultControllerErrorIfNeeded(xsMachine* the, WritableStreamDefaultController* controller, xsSlot* error);
extern txNumber WritableStreamDefaultControllerGetChunkSize(xsMachine* the, WritableStreamDefaultController* controller, xsSlot* chunk);
extern txNumber WritableStreamDefaultControllerGetDesiredSize(xsMachine* the, WritableStreamDefaultController* controller);
extern void WritableStreamDefaultControllerProcessAbort(xsMachine* the, WritableStreamDefaultController* controller, xsSlot* reason);
extern void WritableStreamDefaultControllerStart(xsMachine* the, WritableStreamDefaultController* controller);
extern void WritableStreamDefaultControllerWrite(xsMachine* the, WritableStreamDefaultController* controller, xsSlot* chunk, txNumber chunkSize);

extern WritableStreamDefaultWriter* AcquireWritableStreamDefaultWriter(xsMachine* the, WritableStream* stream);
extern void WritableStreamDefaultWriterCloseWithErrorPropagation(xsMachine* the, WritableStreamDefaultWriter* writer, xsSlot* promise);
extern void WritableStreamDefaultWriterEnsureReadyPromiseRejected(xsMachine* the, WritableStreamDefaultWriter* writer, xsSlot* error);
extern txNumber WritableStreamDefaultWriterGetDesiredSize(xsMachine* the, WritableStreamDefaultWriter* writer);
extern void WritableStreamDefaultWriterRelease(xsMachine* the, WritableStreamDefaultWriter* writer);
extern void WritableStreamDefaultWriterWrite(xsMachine* the, WritableStreamDefaultWriter* writer, xsSlot* chunk, xsSlot* promise);


typedef struct {
	txSlot* ViewConstructor;
	txSlot* buffer;
	txSize bufferByteLength;
	txSize byteLength;
	txSize byteOffset;
	txSize elementSize;
} ViewInfoRecord, *ViewInfo;

extern void GetViewInfo(xsMachine* the, xsSlot* view, ViewInfo info);
