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

static txBoolean ReadableStreamDefaultSourceCancelAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* reason, xsSlot* result);
static txBoolean ReadableStreamDefaultSourcePullAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* param, xsSlot* result);
static txBoolean ReadableStreamDefaultSourceStartAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* param, xsSlot* result);

const StreamDispatchRecord ReadableStreamDispatchRecord = {
	"ReadableStream",
};

static void ReadableStream_mark(xsMachine* the, void* it, xsMarkRoot markRoot);
static const xsHostHooks ReadableStreamHooks ICACHE_RODATA_ATTR = {
	ReadableStream_destructor,
	ReadableStream_mark,
	NULL
};

// 4.2 ReadableStream

void buildReadableStream(xsMachine* the)
{
	ReadableStream* stream = InitializeReadableStream(the, mxThis, fxToReference(the, mxArgv(2)));
	xsSlot* underlyingSource = mxArgv(0);
	xsSlot* strategy = mxArgv(1);
	if (fxToBoolean(the, mxArgv(3))) {
		ReadableStreamDefaultController* controller = CreateReadableStreamDefaultController(the, stream);
		
		(*controller)->strategySizeAlgorithm = ExtractSizeAlgorithm(the, strategy);
		(*controller)->strategyHWM = ExtractHighWaterMark(the, strategy, 1);
		(*controller)->target = fxToReference(the, underlyingSource);
		(*controller)->startAlgorithm.call = ReadableStreamDefaultSourceStartAlgorithm;
		(*controller)->startAlgorithm.callback = ExtractAlgorithmReference(the, underlyingSource, xsID_start);
		(*controller)->pullAlgorithm.call = ReadableStreamDefaultSourcePullAlgorithm;
		(*controller)->pullAlgorithm.callback = ExtractAlgorithmReference(the, underlyingSource, xsID_pull);
		(*controller)->cancelAlgorithm.call = ReadableStreamDefaultSourceCancelAlgorithm;
		(*controller)->cancelAlgorithm.callback = ExtractAlgorithmReference(the, underlyingSource, xsID_cancel);
	
		ReadableStreamDefaultControllerStart(the, controller);
	}
	else {
		ReadableByteStreamController* controller = CreateReadableByteStreamController(the, stream);
		
		(*controller)->strategyHWM = ExtractHighWaterMark(the, strategy, 0);
		(*controller)->target = fxToReference(the, underlyingSource);
		(*controller)->startAlgorithm.call = ReadableStreamDefaultSourceStartAlgorithm;
		(*controller)->startAlgorithm.callback = ExtractAlgorithmReference(the, underlyingSource, xsID_start);
		(*controller)->pullAlgorithm.call = ReadableStreamDefaultSourcePullAlgorithm;
		(*controller)->pullAlgorithm.callback = ExtractAlgorithmReference(the, underlyingSource, xsID_pull);
		(*controller)->cancelAlgorithm.call = ReadableStreamDefaultSourceCancelAlgorithm;
		(*controller)->cancelAlgorithm.callback = ExtractAlgorithmReference(the, underlyingSource, xsID_cancel);
	
		(*controller)->autoAllocateChunkSize = 0;
		if ((*controller)->target) {
			mxPushReference((*controller)->target);
			mxGetID(xsID_autoAllocateChunkSize);
			if (!mxIsUndefined(the->stack)) {
				(*controller)->autoAllocateChunkSize = fxToInteger(the, the->stack);
				if ((*controller)->autoAllocateChunkSize <= 0)
					xsTypeError("invalid autoAllocateChunkSize");
			}
			mxPop();
		}
		
		ReadableByteStreamControllerStart(the, controller);
	}
}

void ReadableStream_destructor(void* it)
{
}
void ReadableStream_mark(xsMachine* the, void* it, xsMarkRoot markRoot)
{
	ReadableStream stream = it;
	StreamMarkReference(the, stream->closures);
	StreamMarkHandle(the, stream->controller);
	StreamMarkHandle(the, stream->reader);
}

void ReadableStream_get_locked(xsMachine* the)
{
	ReadableStream* stream = mxStreamHandle(ReadableStream, mxThis);
	xsResult = (IsReadableStreamLocked(the, stream)) ? xsTrue : xsFalse;
}

void ReadableStream_cancel(xsMachine* the)
{
	ReadableStream* stream = mxStreamHandle(ReadableStream, mxThis);
	if (IsReadableStreamLocked(the, stream)) {
		mxPush(mxTypeErrorConstructor);
		fxNewError(the, "stream locked");
		fxCreateRejectedPromise(the, the->stack, mxResult);
		mxPop();
	}
	else {
		if (mxArgc > 0)
			mxPushSlot(mxArgv(0));
		else
			mxPushUndefined();
		ReadableStreamCancel(the, stream, the->stack, mxResult);
		mxPop();
	}
}


// 4.9.2 Interfacing with controllers
ReadableStream* CreateReadableStream(xsMachine* the, xsSlot* it, xsSlot* closures)
{
	mxPushReference(closures);
	mxGetID(xsID_readableStream);
	fxNewHostInstance(the);
	mxPullSlot(it);
	return InitializeReadableStream(the, it, closures);
}
ReadableStream* InitializeReadableStream(xsMachine* the, xsSlot* it, xsSlot* closures)
{
	fxSetHostChunk(the, it, NULL, sizeof(ReadableStreamRecord));
	fxSetHostHooks(the, it, (xsHostHooks*)&ReadableStreamHooks);
	ReadableStream* stream = (ReadableStream*)fxGetHostHandle(the, it);
	(*stream)->reference = fxToReference(the, it);
	(*stream)->dispatch = (StreamDispatch)&ReadableStreamDispatchRecord;
	(*stream)->closures = closures;
	// storedError can be any value...
	txSlot* slot = fxLastProperty(the, (*stream)->reference);
	slot = slot->next = fxNewSlot(the);
	slot->flag |= XS_INTERNAL_FLAG;
	slot->kind = XS_UNINITIALIZED_KIND;
	(*stream)->storedError = slot;
	return stream;
}
void ReadableStreamCancel(xsMachine* the, ReadableStream* stream, xsSlot* reason, xsSlot* result)
{
	(*stream)->disturbed = 1;
	if ((*stream)->state == mcStreamClosed) {
		fxCreateResolvedPromise(the, &mxUndefined, result);
	}
	else if ((*stream)->state == mcStreamErrored) {
		fxCreateRejectedPromise(the, (*stream)->storedError, result);
	}
	else {
		ReadableStreamClose(the, stream);
		ReadableStreamReader* reader = (*stream)->reader;
		if (reader != NULL)
			(*((*reader)->dispatch->cancel))(the, reader);
		(*(*((*stream)->controller))->dispatch->cancelSteps)(the, (*stream)->controller, reason, result);
		mxPushReference((*stream)->closures);
		mxGetID(xsID_ignore);
		xsSlot* function = fxToReference(the, the->stack);
		fxChainPromise(the, result, function, NULL, result);
		mxPop();
	}
}
void ReadableStreamClose(xsMachine* the, ReadableStream* stream)
{
	mxStreamAssert((*stream)->state == mcStreamReadable);
	(*stream)->state = mcStreamClosed;
	ReadableStreamReader* reader = (*stream)->reader;
	if (reader == NULL)
		return;
	fxResolvePromiseRecord(the, (*reader)->closedPromise, &mxUndefined);
	(*((*reader)->dispatch->close))(the, reader);
}
void ReadableStreamError(xsMachine* the, ReadableStream* stream, xsSlot* error)
{
	mxStreamAssert((*stream)->state == mcStreamReadable);
	(*stream)->state = mcStreamErrored;
	(*stream)->storedError->kind = error->kind;
	(*stream)->storedError->value = error->value;
	ReadableStreamReader* reader = (*stream)->reader;
	if (reader == NULL)
		return;
	fxRejectPromiseRecord(the, (*reader)->closedPromise, error);
	fxHandlePromiseRecord(the, (*reader)->closedPromise, (*stream)->closures);
	(*((*reader)->dispatch->error))(the, reader, error);
}
txBoolean IsReadableStreamLocked(xsMachine* the, ReadableStream* stream)
{
	return ((*stream)->reader) ? 1 : 0;
}

void ReadableStreamAddReadRequest(xsMachine* the, ReadableStream* self, xsSlot* request)
{
	ReadableStreamDefaultReader* reader = (ReadableStreamDefaultReader*)((*self)->reader);
	EnqueueSlot(the, (*reader)->queue, request);
}
void ReadableStreamFulfillReadRequest(xsMachine* the, ReadableStream* self, xsSlot* chunk, txBoolean done)
{
	ReadableStreamReader* reader = (*self)->reader;
	mxStreamAssert(GetSlotQueueLength(the, (*reader)->queue) > 0);
	xsSlot* request;
	mxTemporary(request);
	DequeueSlot(the, (*reader)->queue, request);
	if (done)
		(*(*reader)->closeSteps)(the, reader, request, chunk);
	else
		(*(*reader)->chunkSteps)(the, reader, request, chunk);
	mxPop();
}
txInteger ReadableStreamGetNumReadRequests(xsMachine* the, ReadableStream* self)
{
	ReadableStreamDefaultReader* reader = (ReadableStreamDefaultReader*)((*self)->reader);
	return GetSlotQueueLength(the, (*reader)->queue);
}

void ReadableStreamReaderChunkSteps(xsMachine* the, ReadableStreamReader* reader, xsSlot* request, xsSlot* chunk)
{
	mxPushSlot(chunk);
	fxNewStreamResult(the, 0);	
	fxResolvePromiseRecord(the, fxToReference(the, request), the->stack);
	mxPop();
}

void ReadableStreamReaderCloseSteps(xsMachine* the, ReadableStreamReader* reader, xsSlot* request, xsSlot* chunk)
{
	if (chunk)
		mxPushSlot(chunk);
	else
		mxPushUndefined();
	fxNewStreamResult(the, 1);	
	fxResolvePromiseRecord(the, fxToReference(the, request), the->stack);
}

void ReadableStreamReaderErrorSteps(xsMachine* the, ReadableStreamReader* reader, xsSlot* request, xsSlot* error)
{
	fxRejectPromiseRecord(the, fxToReference(the, request), error);
}

txBoolean ReadableStreamDefaultSourceCancelAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* reason, xsSlot* result)
{
	ReadableStreamController* controller = (ReadableStreamController*)stuff;
	txBoolean success = 1;
	if ((*controller)->cancelAlgorithm.callback) {
		mxTry(the) {
			if ((*controller)->target)
				mxPushReference((*controller)->target);
			else
				mxPushUndefined();
			mxPushReference((*controller)->cancelAlgorithm.callback);
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
txBoolean ReadableStreamDefaultSourcePullAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* param, xsSlot* result)
{
	ReadableStreamController* controller = (ReadableStreamController*)stuff;
	txBoolean success = 1;
	if ((*controller)->pullAlgorithm.callback) {
		mxTry(the) {
			if ((*controller)->target)
				mxPushReference((*controller)->target);
			else
				mxPushUndefined();
			mxPushReference((*controller)->pullAlgorithm.callback);
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
txBoolean ReadableStreamDefaultSourceStartAlgorithm(xsMachine* the, StreamStuff* stuff, xsSlot* param, xsSlot* result)
{
	ReadableStreamController* controller = (ReadableStreamController*)stuff;
	txBoolean success = 1;
	if ((*controller)->startAlgorithm.callback) {
// 		mxTry(the) {
			if ((*controller)->target)
				mxPushReference((*controller)->target);
			else
				mxPushUndefined();
			mxPushReference((*controller)->startAlgorithm.callback);
			mxCall();
			mxPushReference((*controller)->reference);
			mxRunCount(1);
			mxPullSlot(result);
// 		}
// 		mxCatch(the) {
// 			*result = mxException;
// 			mxException = mxUndefined;
// 			success = 0;
// 		}
	}
	return success;
}
