/*
 * Copyright (c) 2026  Moddable Tech, Inc.
 *
 *   This file is part of the Moddable SDK Runtime.
 *
 *   The Moddable SDK Runtime is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   The Moddable SDK Runtime is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with the Moddable SDK Runtime.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "mcStreamAll.h"

typedef struct TransformStreamStruct TransformStreamRecord, *TransformStream;
typedef struct TransformStreamDefaultControllerStruct TransformStreamDefaultControllerRecord, *TransformStreamDefaultController;

struct TransformStreamStruct {
	StreamHandlePart;
	StreamDispatchPart;
	xsSlot* closures;
	TransformStreamDefaultController* controller;
	ReadableStream* readable;
	WritableStream* writable;
	xsSlot* backpressureChangeChunk;
	xsSlot* backpressureChangePromise;
	xsSlot* backpressureChangeResolved;
	xsSlot* backpressureChangeRejected;
	txBoolean backpressure;
	xsSlot* cancelReason;
};

struct TransformStreamDefaultControllerStruct {
	StreamHandlePart;
	StreamDispatchPart;
	TransformStream* stream;
	xsSlot* target;
	xsSlot* startAlgorithmCallback;
	xsSlot* transformAlgorithmCallback;;
	xsSlot* flushAlgorithmCallback;
	xsSlot* cancelAlgorithmCallback;
	xsSlot* startPromise;
	xsSlot* finishPromise;
};

static void TransformStreamError(xsMachine* the, TransformStream* stream, xsSlot* error);
static void TransformStreamErrorWritableAndUnblockWrite(xsMachine* the, TransformStream* stream, xsSlot* error);
static void TransformStreamUnblockWrite(xsMachine* the, TransformStream* stream);
static void TransformStreamSetBackpressure(xsMachine* the, TransformStream* stream, txBoolean backpressure);

static TransformStreamDefaultController* CreateTransformStreamDefaultController(xsMachine* the, TransformStream* stream);
static void TransformStreamDefaultControllerClearAlgorithms(xsMachine* the, TransformStreamDefaultController* controller);
static void TransformStreamDefaultControllerEnqueue(xsMachine* the, TransformStreamDefaultController* controller, xsSlot* chunk);
static void TransformStreamDefaultControllerError(xsMachine* the, TransformStreamDefaultController* controller, xsSlot* e);
static txBoolean TransformStreamDefaultControllerPerformCancel(xsMachine* the, TransformStreamDefaultController* controller, xsSlot* param, xsSlot* result);
static txBoolean TransformStreamDefaultControllerPerformFlush(xsMachine* the, TransformStreamDefaultController* controller, xsSlot* param, xsSlot* result);
static txBoolean TransformStreamDefaultControllerPerformStart(xsMachine* the, TransformStreamDefaultController* controller, xsSlot* param, xsSlot* result);
static txBoolean TransformStreamDefaultControllerPerformTransform(xsMachine* the, TransformStreamDefaultController* controller, xsSlot* chunk, xsSlot* result);
static void TransformStreamDefaultControllerTerminate(xsMachine* the, TransformStreamDefaultController* controller);

static txBoolean TransformStreamDefaultSinkAbortAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* reason, xsSlot* result);
static void TransformStreamDefaultSinkAbortAlgorithmResolved(xsMachine* the);
static void TransformStreamDefaultSinkAbortAlgorithmRejected(xsMachine* the);
static txBoolean TransformStreamDefaultSinkCloseAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* param, xsSlot* result);
static void TransformStreamDefaultSinkCloseAlgorithmResolved(xsMachine* the);
static void TransformStreamDefaultSinkCloseAlgorithmRejected(xsMachine* the);
static txBoolean TransformStreamDefaultSinkStartAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* param, xsSlot* result);
static txBoolean TransformStreamDefaultSinkWriteAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* chunk, xsSlot* result);
static void TransformStreamDefaultSinkWriteAlgorithmResolved(xsMachine* the);
static void TransformStreamDefaultSinkWriteAlgorithmRejected(xsMachine* the);
static txBoolean TransformStreamDefaultSourceCancelAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* reason, xsSlot* result);
static void TransformStreamDefaultSourceCancelAlgorithmResolved(xsMachine* the);
static void TransformStreamDefaultSourceCancelAlgorithmRejected(xsMachine* the);
static txBoolean TransformStreamDefaultSourcePullAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* param, xsSlot* result);
static txBoolean TransformStreamDefaultSourceStartAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* param, xsSlot* result);

// 6.2 TransformStream
const StreamDispatchRecord TransformStreamDispatchRecord = {
	"TransformStream",
};
static void TransformStream_mark(xsMachine* the, void* it, xsMarkRoot markRoot);
static const xsHostHooks TransformStreamHooks ICACHE_RODATA_ATTR = {
	TransformStream_destructor,
	TransformStream_mark,
	NULL
};
void buildTransformStream(xsMachine* the)
{
	TransformStream* stream;
	xsSlot* transformer = mxArgv(0);
	xsSlot* writableStrategy = mxArgv(1);
	xsSlot* readableStrategy = mxArgv(2);
	xsSlot* tmp;
	mxTemporary(tmp);
	
	fxSetHostChunk(the, mxThis, NULL, sizeof(TransformStreamRecord));
	fxSetHostHooks(the, mxThis, (xsHostHooks*)&TransformStreamHooks);
	stream = (TransformStream*)fxGetHostHandle(the, mxThis);
	(*stream)->reference = xsToReference(xsThis);
	(*stream)->dispatch = (StreamDispatch)&TransformStreamDispatchRecord;
	(*stream)->closures = xsToReference(xsArg(3));
	
	(*stream)->writable = CreateWritableStream(the, tmp, (*stream)->closures);
	WritableStreamDefaultController* writableController = CreateWritableStreamDefaultController(the, (*stream)->writable);
	(*writableController)->strategySizeAlgorithm = ExtractSizeAlgorithm(the, writableStrategy);
	(*writableController)->strategyHWM = ExtractHighWaterMark(the, writableStrategy, 1);
	(*writableController)->target = (*stream)->reference;
	(*writableController)->startAlgorithm.call = TransformStreamDefaultSinkStartAlgorithm;
	(*writableController)->writeAlgorithm.call = TransformStreamDefaultSinkWriteAlgorithm;
	(*writableController)->closeAlgorithm.call = TransformStreamDefaultSinkCloseAlgorithm;
	(*writableController)->abortAlgorithm.call = TransformStreamDefaultSinkAbortAlgorithm;
	
	(*stream)->readable = CreateReadableStream(the, tmp, (*stream)->closures);
	ReadableStreamDefaultController* readableController = CreateReadableStreamDefaultController(the, (*stream)->readable);
	(*readableController)->strategySizeAlgorithm = ExtractSizeAlgorithm(the, readableStrategy);
	(*readableController)->strategyHWM = ExtractHighWaterMark(the, readableStrategy, 0);
	(*readableController)->target = (*stream)->reference;
	(*readableController)->startAlgorithm.call = TransformStreamDefaultSourceStartAlgorithm;
	(*readableController)->pullAlgorithm.call = TransformStreamDefaultSourcePullAlgorithm;
	(*readableController)->cancelAlgorithm.call = TransformStreamDefaultSourceCancelAlgorithm;
	
	TransformStreamDefaultController* controller = CreateTransformStreamDefaultController(the, stream);
	
	(*controller)->target = fxToReference(the, transformer);
	(*controller)->startAlgorithmCallback  = ExtractAlgorithmReference(the, transformer, xsID_start);
	(*controller)->transformAlgorithmCallback = ExtractAlgorithmReference(the, transformer, xsID_transform);
	(*controller)->flushAlgorithmCallback = ExtractAlgorithmReference(the, transformer, xsID_flush);
	(*controller)->cancelAlgorithmCallback = ExtractAlgorithmReference(the, transformer, xsID_cancel);

	txSlot* slot = fxLastProperty(the, (*stream)->reference);
	slot = slot->next = fxNewSlot(the);
	slot->flag |= XS_INTERNAL_FLAG;
	slot->kind = XS_UNINITIALIZED_KIND;
	(*stream)->backpressureChangeChunk = slot;
	slot = slot->next = fxNewSlot(the);
	slot->flag |= XS_INTERNAL_FLAG;
	slot->kind = XS_UNINITIALIZED_KIND;
	(*stream)->cancelReason = slot;

	(*stream)->backpressureChangeResolved = fxNewHostFunctionWithHandle(the, stream, TransformStreamDefaultSinkWriteAlgorithmResolved, 1); mxPop();
	(*stream)->backpressureChangeRejected = fxNewHostFunctionWithHandle(the, stream, TransformStreamDefaultSinkWriteAlgorithmRejected, 1); mxPop();

	TransformStreamSetBackpressure(the, stream, 1);
	
	(*controller)->startPromise = fxCreatePromiseRecord(the, NULL);
	WritableStreamDefaultControllerStart(the, writableController);
	ReadableStreamDefaultControllerStart(the, readableController);
	
	xsSlot* result;
	mxTemporary(result);
	TransformStreamDefaultControllerPerformStart(the, controller, NULL, result);
	fxResolvePromiseRecord(the, (*controller)->startPromise, result);
	mxPop(); // result
}
void TransformStream_destructor(void* it)
{
}
void TransformStream_mark(xsMachine* the, void* it, xsMarkRoot markRoot)
{
	TransformStream stream = it;
	StreamMarkReference(the, stream->closures);
	StreamMarkHandle(the, stream->controller);
	StreamMarkHandle(the, stream->readable);
	StreamMarkHandle(the, stream->writable);
	StreamMarkReference(the, stream->backpressureChangePromise);
	StreamMarkReference(the, stream->backpressureChangeResolved);
	StreamMarkReference(the, stream->backpressureChangeRejected);
}
void TransformStream_get_readable(xsMachine* the)
{
	TransformStream* stream = mxStreamHandle(TransformStream, mxThis);
	ReadableStream* readableStream = (*stream)->readable;
	mxPushReference((*readableStream)->reference);
	mxPullSlot(mxResult);
}
void TransformStream_get_writable(xsMachine* the)
{
	TransformStream* stream = mxStreamHandle(TransformStream, mxThis);
	WritableStream* writableStream = (*stream)->writable;
	mxPushReference((*writableStream)->reference);
	mxPullSlot(mxResult);
}

// 6.3 TransformStreamDefaultController
static const StreamDispatchRecord TransformStreamDefaultControllerDispatchRecord = {
	"TransformStreamDefaultController",
};
static void TransformStreamDefaultController_mark(xsMachine* the, void* it, xsMarkRoot markRoot);
static const xsHostHooks TransformStreamDefaultControllerHooks ICACHE_RODATA_ATTR = {
	TransformStreamDefaultController_destructor,
	TransformStreamDefaultController_mark,
	NULL
};
void TransformStreamDefaultController_destructor(void* it)
{
}
void TransformStreamDefaultController_mark(xsMachine* the, void* it, xsMarkRoot markRoot)
{
	TransformStreamDefaultController self = it;
	StreamMarkHandle(the, self->stream);
	StreamMarkReference(the, self->target);
	StreamMarkReference(the, self->startAlgorithmCallback);
	StreamMarkReference(the, self->transformAlgorithmCallback);
	StreamMarkReference(the, self->flushAlgorithmCallback);
	StreamMarkReference(the, self->cancelAlgorithmCallback);
	StreamMarkReference(the, self->startPromise);
	StreamMarkReference(the, self->finishPromise);
}
void TransformStreamDefaultController_get_desiredSize(xsMachine* the)
{
	TransformStreamDefaultController* controller = mxStreamHandle(TransformStreamDefaultController, mxThis);
	TransformStream* stream = (*controller)->stream;
	ReadableStream* readableStream = (*stream)->readable;
	txNumber size = ReadableStreamDefaultControllerGetDesiredSize(the, (ReadableStreamDefaultController*)((*readableStream)->controller));
	if (c_isnan(size))
		xsResult = xsNull;
	else
		xsResult = xsNumber(size);
}
void TransformStreamDefaultController_enqueue(xsMachine* the)
{
	TransformStreamDefaultController* controller = mxStreamHandle(TransformStreamDefaultController, mxThis);
	if (mxArgc > 0)
		mxPushSlot(mxArgv(0));
	else
		mxPushUndefined();
	TransformStreamDefaultControllerEnqueue(the, controller, the->stack);
	mxPop();
}
void TransformStreamDefaultController_error(xsMachine* the)
{
	TransformStreamDefaultController* controller = mxStreamHandle(TransformStreamDefaultController, mxThis);
	if (mxArgc > 0)
		mxPushSlot(mxArgv(0));
	else
		mxPushUndefined();
	TransformStreamDefaultControllerError(the, controller, the->stack);
	mxPop();
}
void TransformStreamDefaultController_terminate(xsMachine* the)
{
	TransformStreamDefaultController* controller = mxStreamHandle(TransformStreamDefaultController, mxThis);
	TransformStreamDefaultControllerTerminate(the, controller);
}

// 6.4.1 Working with transform streams
void TransformStreamError(xsMachine* the, TransformStream* stream, xsSlot* error)
{
	ReadableStream* readableStream = (*stream)->readable;
	ReadableStreamDefaultControllerError(the, (ReadableStreamDefaultController*)((*readableStream)->controller), error);
	TransformStreamErrorWritableAndUnblockWrite(the, stream, error);
}
void TransformStreamErrorWritableAndUnblockWrite(xsMachine* the, TransformStream* stream, xsSlot* error)
{
	TransformStreamDefaultControllerClearAlgorithms(the, (*stream)->controller);
	WritableStream* writableStream = (*stream)->writable;
	WritableStreamDefaultControllerErrorIfNeeded(the, (*writableStream)->controller, error);
	TransformStreamUnblockWrite(the, stream);
}
void TransformStreamUnblockWrite(xsMachine* the, TransformStream* stream) 
{
	if ((*stream)->backpressure) {
		TransformStreamSetBackpressure(the, stream, 0);
	}
}
void TransformStreamSetBackpressure(xsMachine* the, TransformStream* stream, txBoolean backpressure) 
{
	mxStreamAssert((*stream)->backpressure != backpressure);
	if ((*stream)->backpressureChangePromise) {
		fxResolvePromiseRecord(the, (*stream)->backpressureChangePromise, &mxUndefined);
	}
	(*stream)->backpressureChangePromise = fxCreatePromiseRecord(the, NULL);
	(*stream)->backpressure = backpressure;
}

// 6.4.2 Default controllers
TransformStreamDefaultController* CreateTransformStreamDefaultController(xsMachine* the, TransformStream* stream)
{
	mxPushReference((*stream)->closures);
	mxGetID(xsID_transformStreamDefaultController);
	xsSlot* instance = fxNewHostInstance(the);
	xsSlot* reference = the->stack;
	fxSetHostChunk(the, reference, NULL, sizeof(TransformStreamDefaultControllerRecord));
	fxSetHostHooks(the, reference, (xsHostHooks*)&TransformStreamDefaultControllerHooks);
	TransformStreamDefaultController* controller = (TransformStreamDefaultController*)fxGetHostHandle(the, reference);
	(*controller)->reference = instance;
	(*controller)->dispatch = (StreamDispatch)&TransformStreamDefaultControllerDispatchRecord;
	(*controller)->stream = stream;
	(*stream)->controller = controller;
	mxPop(); // controller
	
	return controller;
}
void TransformStreamDefaultControllerClearAlgorithms(xsMachine* the, TransformStreamDefaultController* controller)
{
	(*controller)->startAlgorithmCallback = NULL;
	(*controller)->transformAlgorithmCallback = NULL;
	(*controller)->flushAlgorithmCallback = NULL;
	(*controller)->cancelAlgorithmCallback = NULL;
}
void TransformStreamDefaultControllerEnqueue(xsMachine* the, TransformStreamDefaultController* controller, xsSlot* chunk)
{
	TransformStream* stream = (*controller)->stream;
	ReadableStream* readableStream = (*stream)->readable;
	ReadableStreamDefaultController* readableController = (ReadableStreamDefaultController*)((*readableStream)->controller);
	if (!ReadableStreamDefaultControllerCanCloseOrEnqueue(the, readableController)) {
		mxTypeError("Readable side is not in a state that permits enqueue");
	}

	// We throttle transform invocations based on the backpressure of the ReadableStream, but we still
	// accept TransformStreamDefaultControllerEnqueue() calls.

	mxTry(the) {
		ReadableStreamDefaultControllerEnqueue(the, readableController, chunk);
	} 
	mxCatch(the) {
		// This happens when readableStrategy.size() throws.
		TransformStreamErrorWritableAndUnblockWrite(the, stream, &mxException);
		mxPushSlot((*readableStream)->storedError);
		mxPull(mxException);
		fxJump(the);
	}
	txBoolean backpressure = ReadableStreamDefaultControllerHasBackpressure(the, readableController);
	if (backpressure != (*stream)->backpressure) {
		mxStreamAssert(backpressure == 1);
		TransformStreamSetBackpressure(the, stream, 1);
	}
}
void TransformStreamDefaultControllerError(xsMachine* the, TransformStreamDefaultController* controller, xsSlot* e)
{
	TransformStream* stream = (*controller)->stream;
	TransformStreamError(the, stream, e);
}
txBoolean TransformStreamDefaultControllerPerformCancel(xsMachine* the, TransformStreamDefaultController* controller, xsSlot* param, xsSlot* result)
{
	txBoolean success = 1;
	if ((*controller)->cancelAlgorithmCallback) {
		mxTry(the) {
			if ((*controller)->target)
				mxPushReference((*controller)->target);
			else
				mxPushUndefined();
			mxPushReference((*controller)->cancelAlgorithmCallback);
			mxCall();
			mxPushSlot(param);
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
txBoolean TransformStreamDefaultControllerPerformFlush(xsMachine* the, TransformStreamDefaultController* controller, xsSlot* param, xsSlot* result)
{
	txBoolean success = 1;
	if ((*controller)->flushAlgorithmCallback) {
		mxTry(the) {
			if ((*controller)->target)
				mxPushReference((*controller)->target);
			else
				mxPushUndefined();
			mxPushReference((*controller)->flushAlgorithmCallback);
			mxCall();
			mxPushReference((*controller)->reference);
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
txBoolean TransformStreamDefaultControllerPerformStart(xsMachine* the, TransformStreamDefaultController* controller, xsSlot* param, xsSlot* result)
{
	if ((*controller)->startAlgorithmCallback) {
		if ((*controller)->target)
			mxPushReference((*controller)->target);
		else
			mxPushUndefined();
		mxPushReference((*controller)->startAlgorithmCallback);
		mxCall();
		mxPushReference((*controller)->reference);
		mxRunCount(1);
		mxPullSlot(result);
	}
	return 1;
}
txBoolean TransformStreamDefaultControllerPerformTransform(xsMachine* the, TransformStreamDefaultController* controller, xsSlot* chunk, xsSlot* result)
{
	txBoolean success = 1;
	mxTry(the) {
		if ((*controller)->transformAlgorithmCallback) {
			if ((*controller)->target)
				mxPushReference((*controller)->target);
			else
				mxPushUndefined();
			mxPushReference((*controller)->transformAlgorithmCallback);
			mxCall();
			mxPushSlot(chunk);
			mxPushReference((*controller)->reference);
			mxRunCount(2);
			mxPullSlot(result);
		}
		else {
			TransformStreamDefaultControllerEnqueue(the, controller, chunk);
			*result = mxUndefined;
		}
	}
	mxCatch(the) {
		*result = mxException;
		mxException = mxUndefined;
		success = 0;
	}
	return success;
}
void TransformStreamDefaultControllerTerminate(xsMachine* the, TransformStreamDefaultController* controller)
{
	TransformStream* stream = (*controller)->stream;
	ReadableStream* readableStream = (*stream)->readable;
	ReadableStreamDefaultController* readableController = (ReadableStreamDefaultController*)((*readableStream)->controller);
	ReadableStreamDefaultControllerClose(the, readableController);
	mxPush(mxTypeErrorConstructor);
	fxNewError(the, "TransformStream terminated");
	TransformStreamErrorWritableAndUnblockWrite(the, stream, the->stack);
	mxPop();
}

// 6.4.3 Default sinks
txBoolean TransformStreamDefaultSinkAbortAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* reason, xsSlot* result)
{
	WritableStreamDefaultController* writableController = (WritableStreamDefaultController*)stuff;
	TransformStream* stream = mxStreamHandle(TransformStream, (*writableController)->target);
	TransformStreamDefaultController* controller = (*stream)->controller;
	if ((*controller)->finishPromise == NULL) {
		(*controller)->finishPromise = fxCreatePromiseRecord(the, NULL);
		(*stream)->cancelReason->kind = reason->kind;
		(*stream)->cancelReason->value = reason->value;
		xsSlot* cancelPromise;
		mxTemporary(cancelPromise);
		txBoolean success = TransformStreamDefaultControllerPerformCancel(the, controller, reason, cancelPromise);
		TransformStreamDefaultControllerClearAlgorithms(the, controller);
		xsSlot* cancelResolved = fxNewHostFunctionWithHandle(the, stream, TransformStreamDefaultSinkAbortAlgorithmResolved, 1);
		xsSlot* cancelRejected = fxNewHostFunctionWithHandle(the, stream, TransformStreamDefaultSinkAbortAlgorithmRejected, 1);
		fxChainAlgorithm(the, cancelPromise, success, cancelResolved, cancelRejected);
		mxPop(); // cancelRejected
		mxPop(); // cancelResolved
		mxPop(); // cancelPromise
	}
	fxGetPromiseRecordPromise(the, (*controller)->finishPromise, result);
	return 1;
}
void TransformStreamDefaultSinkAbortAlgorithmResolved(xsMachine* the)
{
	TransformStream* stream = (TransformStream*)fxGetHostFunctionHandle(the);
	TransformStreamDefaultController* controller = (*stream)->controller;
	ReadableStream* readableStream = (*stream)->readable;
	if ((*readableStream)->state == mcStreamErrored) {
		fxRejectPromiseRecord(the, (*controller)->finishPromise, (*readableStream)->storedError);
	}
	else{
		ReadableStreamDefaultControllerError(the, (ReadableStreamDefaultController*)((*readableStream)->controller), (*stream)->cancelReason);
		fxResolvePromiseRecord(the, (*controller)->finishPromise, &mxUndefined);
	}
}
void TransformStreamDefaultSinkAbortAlgorithmRejected(xsMachine* the)
{
	TransformStream* stream = (TransformStream*)fxGetHostFunctionHandle(the);
	TransformStreamDefaultController* controller = (*stream)->controller;
	ReadableStream* readableStream = (*stream)->readable;
	ReadableStreamDefaultControllerError(the, (ReadableStreamDefaultController*)((*readableStream)->controller), mxArgv(0));
	fxRejectPromiseRecord(the, (*controller)->finishPromise, mxArgv(0));
}
txBoolean TransformStreamDefaultSinkCloseAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* param, xsSlot* result)
{
	WritableStreamDefaultController* writableController = (WritableStreamDefaultController*)stuff;
	TransformStream* stream = mxStreamHandle(TransformStream, (*writableController)->target);
	TransformStreamDefaultController* controller = (*stream)->controller;
	if ((*controller)->finishPromise == NULL) {
		(*controller)->finishPromise = fxCreatePromiseRecord(the, NULL);
		xsSlot* flushPromise;
		mxTemporary(flushPromise);
		txBoolean success = TransformStreamDefaultControllerPerformFlush(the, controller, param, flushPromise);
		TransformStreamDefaultControllerClearAlgorithms(the, (*stream)->controller);
		xsSlot* flushResolved = fxNewHostFunctionWithHandle(the, stream, TransformStreamDefaultSinkCloseAlgorithmResolved, 1);
		xsSlot* flushRejected = fxNewHostFunctionWithHandle(the, stream, TransformStreamDefaultSinkCloseAlgorithmRejected, 1);
		fxChainAlgorithm(the, flushPromise, success, flushResolved, flushRejected);
		mxPop(); // flushRejected
		mxPop(); // flushResolved
		mxPop(); // flushPromise
	}
	fxGetPromiseRecordPromise(the, (*controller)->finishPromise, result);
	return 1;
}
void TransformStreamDefaultSinkCloseAlgorithmResolved(xsMachine* the)
{
	TransformStream* stream = (TransformStream*)fxGetHostFunctionHandle(the);
	TransformStreamDefaultController* controller = (*stream)->controller;
	ReadableStream* readableStream = (*stream)->readable;
	if ((*readableStream)->state == mcStreamErrored) {
		fxRejectPromiseRecord(the, (*controller)->finishPromise, (*readableStream)->storedError);
	}
	else{
		ReadableStreamDefaultControllerClose(the, (ReadableStreamDefaultController*)((*readableStream)->controller));
		fxResolvePromiseRecord(the, (*controller)->finishPromise, &mxUndefined);
	}
}
void TransformStreamDefaultSinkCloseAlgorithmRejected(xsMachine* the)
{
	TransformStream* stream = (TransformStream*)fxGetHostFunctionHandle(the);
	TransformStreamDefaultController* controller = (*stream)->controller;
	ReadableStream* readableStream = (*stream)->readable;
	ReadableStreamDefaultControllerError(the, (ReadableStreamDefaultController*)((*readableStream)->controller), mxArgv(0));
	fxRejectPromiseRecord(the, (*controller)->finishPromise, mxArgv(0));
}
txBoolean TransformStreamDefaultSinkStartAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* param, xsSlot* result)
{
	WritableStreamDefaultController* writableController = (WritableStreamDefaultController*)stuff;
	TransformStream* stream = mxStreamHandle(TransformStream, (*writableController)->target);
	TransformStreamDefaultController* controller = (*stream)->controller;
	fxGetPromiseRecordPromise(the, (*controller)->startPromise, result);
	return 1;
}
txBoolean TransformStreamDefaultSinkWriteAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* chunk, xsSlot* result)
{
	WritableStreamDefaultController* writableController = (WritableStreamDefaultController*)stuff;
	TransformStream* stream = mxStreamHandle(TransformStream, (*writableController)->target);
	TransformStreamDefaultController* controller = (*stream)->controller;
	WritableStream* writableStream = (*stream)->writable;
	mxStreamAssert((*writableStream)->state == mcStreamWritable);
	txBoolean success = 1;
	if ((*stream)->backpressure) {
		mxStreamAssert((*stream)->backpressureChangePromise != NULL);
		(*stream)->backpressureChangeChunk->kind = chunk->kind;
		(*stream)->backpressureChangeChunk->value = chunk->value;
		fxGetPromiseRecordPromise(the, (*stream)->backpressureChangePromise, result);
		fxChainPromise(the, result, (*stream)->backpressureChangeResolved, NULL, result);
	}
	else {
		success = TransformStreamDefaultControllerPerformTransform(the, controller, chunk, result);
	}
	return success;
}
void TransformStreamDefaultSinkWriteAlgorithmResolved(xsMachine* the)
{
	TransformStream* stream = (TransformStream*)fxGetHostFunctionHandle(the);
	TransformStreamDefaultController* controller = (*stream)->controller;
	WritableStream* writableStream = (*stream)->writable;
	if ((*writableStream)->state == mcStreamErroring) {
		mxException = *((*writableStream)->storedError);
		fxJump(the);
	}
	if (!TransformStreamDefaultControllerPerformTransform(the, controller, (*stream)->backpressureChangeChunk, mxResult)) {
		fxCreateRejectedPromise(the, mxResult, mxResult);
		fxChainPromise(the, mxResult, NULL, (*stream)->backpressureChangeRejected, mxResult);
	}
	(*stream)->backpressureChangeChunk->kind = XS_UNINITIALIZED_KIND;
}
void TransformStreamDefaultSinkWriteAlgorithmRejected(xsMachine* the)
{
	TransformStream* stream = (TransformStream*)fxGetHostFunctionHandle(the);
	TransformStreamError(the, stream, mxArgv(0));
	mxException = *mxArgv(0);
	fxJump(the);
}

// 6.4.4 Default sources
txBoolean TransformStreamDefaultSourceCancelAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* reason, xsSlot* result)
{
	ReadableStreamController* readableController = (ReadableStreamController*)stuff;
	TransformStream* stream = mxStreamHandle(TransformStream, (*readableController)->target);
	TransformStreamDefaultController* controller = (*stream)->controller;
	if ((*controller)->finishPromise == NULL) {
		(*controller)->finishPromise = fxCreatePromiseRecord(the, NULL);
		(*stream)->cancelReason->kind = reason->kind;
		(*stream)->cancelReason->value = reason->value;
		xsSlot* cancelPromise;
		mxTemporary(cancelPromise);
		txBoolean success = TransformStreamDefaultControllerPerformCancel(the, controller, reason, cancelPromise);
		TransformStreamDefaultControllerClearAlgorithms(the, controller);
		xsSlot* cancelResolved = fxNewHostFunctionWithHandle(the, stream, TransformStreamDefaultSourceCancelAlgorithmResolved, 1);
		xsSlot* cancelRejected = fxNewHostFunctionWithHandle(the, stream, TransformStreamDefaultSourceCancelAlgorithmRejected, 1);
		fxChainAlgorithm(the, cancelPromise, success, cancelResolved, cancelRejected);
		mxPop(); // cancelRejected
		mxPop(); // cancelResolved
		mxPop(); // cancelPromise
	}
	fxGetPromiseRecordPromise(the, (*controller)->finishPromise, result);
	return 1;
}
void TransformStreamDefaultSourceCancelAlgorithmResolved(xsMachine* the)
{
	TransformStream* stream = (TransformStream*)fxGetHostFunctionHandle(the);
	TransformStreamDefaultController* controller = (*stream)->controller;
	WritableStream* writableStream = (*stream)->writable;
	if ((*writableStream)->state == mcStreamErrored) {
		fxRejectPromiseRecord(the, (*controller)->finishPromise, (*writableStream)->storedError);
	}
	else{
		WritableStreamDefaultControllerErrorIfNeeded(the, (*writableStream)->controller, (*stream)->cancelReason);
		TransformStreamUnblockWrite(the, stream);
		fxResolvePromiseRecord(the, (*controller)->finishPromise, &mxUndefined);
	}
}
void TransformStreamDefaultSourceCancelAlgorithmRejected(xsMachine* the)
{
	TransformStream* stream = (TransformStream*)fxGetHostFunctionHandle(the);
	TransformStreamDefaultController* controller = (*stream)->controller;
	WritableStream* writableStream = (*stream)->writable;
	WritableStreamDefaultControllerErrorIfNeeded(the, (*writableStream)->controller, mxArgv(0));
	TransformStreamUnblockWrite(the, stream);
	fxRejectPromiseRecord(the, (*controller)->finishPromise, mxArgv(0));
}
txBoolean TransformStreamDefaultSourcePullAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* param, xsSlot* result)
{
	ReadableStreamController* readableController = (ReadableStreamController*)stuff;
	TransformStream* stream = mxStreamHandle(TransformStream, (*readableController)->target);
	mxStreamAssert((*stream)->backpressure);
	mxStreamAssert((*stream)->backpressureChangePromise !=  NULL);
	TransformStreamSetBackpressure(the, stream, 0);
	fxGetPromiseRecordPromise(the, (*stream)->backpressureChangePromise, result);
	return 1;
}
txBoolean TransformStreamDefaultSourceStartAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* param, xsSlot* result)
{
	ReadableStreamController* readableController = (ReadableStreamController*)stuff;
	TransformStream* stream = mxStreamHandle(TransformStream, (*readableController)->target);
	TransformStreamDefaultController* controller = (*stream)->controller;
	fxGetPromiseRecordPromise(the, (*controller)->startPromise, result);
	return 1;
}

