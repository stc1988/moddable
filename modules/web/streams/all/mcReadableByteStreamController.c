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

typedef struct {
	txSlot* ViewConstructor;
	txSlot* buffer;
	txSize bufferByteLength;
	txSize byteLength;
	txSize byteOffset;
	txSize bytesFilled;
	txSize elementSize;
	txSize minimumFill;
	txU1 readerType;
} PullIntoRecord, *PullInto;

enum {
	mcDefaultReaderType,
	mcNoneReaderType,
	mcBYOBReaderType,
};

static void ReadableByteStreamControllerCancelSteps(xsMachine* the, ReadableStreamController* controller, txSlot* reason, xsSlot* result);
static void ReadableByteStreamControllerPullSteps(xsMachine* the, ReadableStreamController* controller, xsSlot* request);
static void ReadableByteStreamControllerReleaseSteps(xsMachine* the, ReadableStreamController* controller);

static void ReadableByteStreamControllerStartAlgorithmResolved(xsMachine* the);
static void ReadableByteStreamControllerStartAlgorithmRejected(xsMachine* the);
static void ReadableByteStreamControllerPullAlgorithmResolved(xsMachine* the);
static void ReadableByteStreamControllerPullAlgorithmRejected(xsMachine* the);

static void ReadableByteStreamControllerCallPullIfNeeded(xsMachine* the, ReadableByteStreamController* controller);
static void ReadableByteStreamControllerClearAlgorithms(xsMachine* the, ReadableByteStreamController* controller);
static void ReadableByteStreamControllerClearPendingPullIntos(xsMachine* the, ReadableByteStreamController* controller);
static void ReadableByteStreamControllerCommitPullIntoDescriptor(xsMachine* the, ReadableStream* stream, xsSlot* pullIntoDescriptor);
static void ReadableByteStreamControllerConvertPullIntoDescriptor(xsMachine* the, xsSlot* pullIntoDescriptor, xsSlot* view);
static void ReadableByteStreamControllerEnqueueChunkToQueue(xsMachine* the, ReadableByteStreamController* controller, xsSlot* buffer, txSize byteOffset, txSize byteLength);
static void ReadableByteStreamControllerEnqueueClonedChunkToQueue(xsMachine* the, ReadableByteStreamController* controller, xsSlot* buffer, txSize byteOffset, txSize byteLength);
static void ReadableByteStreamControllerEnqueueDetachedPullIntoToQueue(xsMachine* the, ReadableByteStreamController* controller, xsSlot* pullIntoDescriptor);
static void ReadableByteStreamControllerFillHeadPullIntoDescriptor(xsMachine* the, ReadableByteStreamController* controller, txInteger size, xsSlot* pullIntoDescriptor);
txBoolean ReadableByteStreamControllerFillPullIntoDescriptorFromQueue(xsMachine* the, ReadableByteStreamController* controller, xsSlot* pullIntoDescriptor);
static void ReadableByteStreamControllerFillReadRequestFromQueue(xsMachine* the, ReadableByteStreamController* controller, xsSlot* request);
static txNumber ReadableByteStreamControllerGetDesiredSize(xsMachine* the, ReadableByteStreamController* controller);
static void ReadableByteStreamControllerHandleQueueDrain(xsMachine* the, ReadableByteStreamController* controller);
static void ReadableByteStreamControllerInvalidateBYOBRequest(xsMachine* the, ReadableByteStreamController* controller);
static void ReadableByteStreamControllerProcessPullIntoDescriptorsUsingQueue(xsMachine* the, ReadableByteStreamController* controller);
static void ReadableByteStreamControllerProcessReadRequestsUsingQueue(xsMachine* the, ReadableByteStreamController* controller);
static void ReadableByteStreamControllerRespondInClosedState(xsMachine* the, ReadableByteStreamController* controller, xsSlot* firstDescriptor);
static void ReadableByteStreamControllerRespondInReadableState(xsMachine* the, ReadableByteStreamController* controller, txInteger bytesWritten, xsSlot* pullIntoDescriptor);
static void ReadableByteStreamControllerRespondInternal(xsMachine* the, ReadableByteStreamController* controller, txInteger bytesWritten);
static void ReadableByteStreamControllerShiftPendingPullInto(xsMachine* the, ReadableByteStreamController* controller);
static txBoolean ReadableByteStreamControllerShouldCallPull(xsMachine* the, ReadableByteStreamController* controller);

static xsSlot* CreateBufferQueue(xsMachine* the);
static void DequeueBuffer(xsMachine* the, xsSlot* queue, xsSlot* buffer, txSize* byteOffset, txSize* byteLength);
static void EnqueueBuffer(xsMachine* the, xsSlot* queue, xsSlot* buffer, txSize byteOffset, txSize byteLength);
static txInteger GetBufferQueueLength(xsMachine* the, xsSlot* queue);
static txInteger GetBufferQueueTotalSize(xsMachine* the, xsSlot* queue);
static void MoveBufferQueueBytes(xsMachine* the, xsSlot* queue, xsSlot* target, txSize dstOffset, txSize dstSize);
static void ResetBufferQueue(xsMachine* the, xsSlot* queue);

static txBoolean IsArrayBufferDetached(xsMachine* the);
static void TransferArrayBuffer(xsMachine* the);

static PullInto* CreatePullInto(xsMachine* the, txSlot* it);
static void DestroyPullInto(void* it);
static void MarkPullInto(xsMachine* the, void* it, xsMarkRoot markRoot);
static PullInto* ToPullIntoHandle(xsMachine* the, txSlot* it);

static void ReadableByteStreamController_mark(xsMachine* the, void* it, xsMarkRoot markRoot);
static const xsHostHooks ReadableByteStreamControllerHooks = {
	ReadableByteStreamController_destructor,
	ReadableByteStreamController_mark,
	NULL
};
static const ReadableStreamControllerDispatchRecord ReadableByteStreamControllerDispatchRecord = {
	"ReadableByteStreamController",
	ReadableByteStreamControllerCancelSteps,
	ReadableByteStreamControllerPullSteps,
	ReadableByteStreamControllerReleaseSteps,
};

static const StreamDispatchRecord ReadableStreamBYOBRequestDispatchRecord = {
	"ReadableStreamBYOBRequest",
};
static void ReadableStreamBYOBRequest_mark(xsMachine* the, void* it, xsMarkRoot markRoot);
static const xsHostHooks ReadableStreamBYOBRequestHooks ICACHE_RODATA_ATTR = {
	ReadableStreamBYOBRequest_destructor,
	ReadableStreamBYOBRequest_mark,
	NULL
};

static const xsHostHooks PullIntoHooks ICACHE_RODATA_ATTR = {
	DestroyPullInto,
	MarkPullInto,
	NULL
};

// 4.6 ReadableByteStreamController
void ReadableByteStreamController_destructor(void* it)
{
}
void ReadableByteStreamController_mark(xsMachine* the, void* it, xsMarkRoot markRoot)
{
	ReadableByteStreamController self = it;
	StreamMarkHandle(the, self->stream);
	StreamMarkHandle(the, self->byobRequest);
	StreamMarkReference(the, self->target);
	StreamMarkReference(the, self->queue);
	StreamMarkReference(the, self->pendingPullIntos);
	StreamMarkAlgorithm(the, &self->startAlgorithm, markRoot);
	StreamMarkAlgorithm(the, &self->pullAlgorithm, markRoot);
	StreamMarkAlgorithm(the, &self->cancelAlgorithm, markRoot);
}
void ReadableByteStreamController_get_byobRequest(xsMachine* the)
{
	ReadableByteStreamController* controller = mxStreamHandle(ReadableByteStreamController, mxThis);
	ReadableByteStreamControllerGetBYOBRequest(the, controller);
	ReadableStreamBYOBRequest* byobRequest = (*controller)->byobRequest;		
	if (byobRequest)
		mxPushReference((*byobRequest)->reference);
	else
		mxPushNull();
	mxPullSlot(mxResult);
}
void ReadableByteStreamController_get_desiredSize(xsMachine* the)
{
	ReadableByteStreamController* controller = mxStreamHandle(ReadableByteStreamController, mxThis);
	txNumber size = ReadableByteStreamControllerGetDesiredSize(the, controller);
	if (c_isnan(size))
		xsResult = xsNull;
	else
		xsResult = xsNumber(size);
}
void ReadableByteStreamController_close(xsMachine* the)
{
	ReadableByteStreamController* controller = mxStreamHandle(ReadableByteStreamController, mxThis);
	if ((*controller)->closeRequested)
		xsTypeError("closing");
	if ((*(*controller)->stream)->state != mcStreamReadable)
		xsTypeError("invalid stream state");
	ReadableByteStreamControllerClose(the, controller);
}
void ReadableByteStreamController_enqueue(xsMachine* the)
{
	ReadableByteStreamController* controller = mxStreamHandle(ReadableByteStreamController, mxThis);
	if (mxArgc > 0)
		mxPushSlot(mxArgv(0));
	else
		mxPushUndefined();
	txSlot* view = the->stack;
	mxDub();
	ViewInfoRecord info;
	GetViewInfo(the, view, &info);
	if (info.byteLength == 0)
		xsTypeError("no bytes");
	if ((*controller)->closeRequested)
		xsTypeError("closing");
	if ((*(*controller)->stream)->state != mcStreamReadable)
		xsTypeError("invalid stream state");
	ReadableByteStreamControllerEnqueue(the, controller, view);
	mxPop();
}
void ReadableByteStreamController_error(xsMachine* the)
{
	ReadableByteStreamController* controller = mxStreamHandle(ReadableByteStreamController, mxThis);
	if (mxArgc > 0)
		mxPushSlot(mxArgv(0));
	else
		mxPushUndefined();
	ReadableByteStreamControllerError(the, controller, the->stack);
	mxPop();
}

// 4.7.4 Internal methods
void ReadableByteStreamControllerCancelSteps(xsMachine* the, ReadableStreamController* it, txSlot* reason, xsSlot* result)
{
	ReadableByteStreamController* controller = (ReadableByteStreamController*)it;
	ReadableByteStreamControllerClearPendingPullIntos(the, controller);
	ResetBufferQueue(the, (*controller)->queue);
	txBoolean success = (*((*controller)->cancelAlgorithm.call))(the, (StreamStuff*)it, reason, result);
	if (success)
		fxCreateResolvedPromise(the, result, result);
	else
		fxCreateRejectedPromise(the,result, result);
	ReadableByteStreamControllerClearAlgorithms(the, controller);
}
void ReadableByteStreamControllerPullSteps(xsMachine* the, ReadableStreamController* it, xsSlot* request)
{
	ReadableByteStreamController* controller = (ReadableByteStreamController*)it;
	if (GetBufferQueueTotalSize(the, (*controller)->queue) > 0) {
		ReadableByteStreamControllerFillReadRequestFromQueue(the, controller, request);
		return;
	}
	txInteger autoAllocateChunkSize = (*controller)->autoAllocateChunkSize;
	if (autoAllocateChunkSize > 0) {
		txSlot* buffer;
		mxTemporary(buffer);
		{
			mxTry(the) {
				mxPush(mxArrayBufferConstructor);
				mxNew();
				mxPushInteger(autoAllocateChunkSize);
				mxRunCount(1);
				mxPullSlot(buffer);
			}
			mxCatch(the) {
				ReadableStream* stream = (*controller)->stream;
				ReadableStreamReader* reader = (*stream)->reader;
				mxPush(mxException);
				mxException = xsUndefined;
				(*((*reader)->errorSteps))(the, reader, request, the->stack);
				mxPop();
				return;
			}
		}
		txSlot* pullIntoDescriptor;
		mxTemporary(pullIntoDescriptor);
		PullInto* pullInto = CreatePullInto(the, pullIntoDescriptor);
		(*pullInto)->ViewConstructor = fxToReference(the, &mxUint8ArrayConstructor); 
		(*pullInto)->buffer = fxToReference(the, buffer);
		(*pullInto)->bufferByteLength = autoAllocateChunkSize; 
		(*pullInto)->byteLength = autoAllocateChunkSize; 
		(*pullInto)->byteOffset = 0; 
		(*pullInto)->bytesFilled = 0; 
		(*pullInto)->elementSize = 1;
		(*pullInto)->minimumFill = 1; 
		(*pullInto)->readerType = mcDefaultReaderType;
		EnqueueSlot(the, (*controller)->pendingPullIntos, pullIntoDescriptor);
		mxPop();
		mxPop();
	}
	ReadableStreamAddReadRequest(the, (*controller)->stream, request);
	ReadableByteStreamControllerCallPullIfNeeded(the, controller);
} 
void ReadableByteStreamControllerReleaseSteps(xsMachine* the, ReadableStreamController* it)
{
	ReadableByteStreamController* controller = (ReadableByteStreamController*)it;
	if (GetSlotQueueLength(the, (*controller)->pendingPullIntos) > 0) {
		txSlot* pullIntoDescriptor;
		mxTemporary(pullIntoDescriptor);
		PeekSlotQueue(the, (*controller)->pendingPullIntos, pullIntoDescriptor);
		PullInto* pullInto = ToPullIntoHandle(the, pullIntoDescriptor);
		(*pullInto)->readerType = mcNoneReaderType;
		mxPop();
	}
}

// 4.9.4 Byte stream controllers

ReadableByteStreamController* CreateReadableByteStreamController(xsMachine* the, ReadableStream* stream)
{
	mxPushReference((*stream)->closures);
	mxGetID(xsID_readableByteStreamController);
	xsSlot* instance = fxNewHostInstance(the);
	xsSlot* reference = the->stack;
	fxSetHostChunk(the, reference, NULL, sizeof(ReadableByteStreamControllerRecord));
	fxSetHostHooks(the, reference, (xsHostHooks*)&ReadableByteStreamControllerHooks);
	ReadableByteStreamController* controller = (ReadableByteStreamController*)fxGetHostHandle(the, reference);
	(*controller)->reference = instance;
	(*controller)->dispatch = (ReadableStreamControllerDispatch)&ReadableByteStreamControllerDispatchRecord;
	
	(*controller)->queue = CreateBufferQueue(the);
	(*controller)->pendingPullIntos = CreateSlotQueue(the);
	
	(*controller)->startAlgorithm.resolved = fxNewHostFunctionWithHandle(the, controller, ReadableByteStreamControllerStartAlgorithmResolved, 1); mxPop();
	(*controller)->startAlgorithm.rejected = fxNewHostFunctionWithHandle(the, controller, ReadableByteStreamControllerStartAlgorithmRejected, 1); mxPop();
	(*controller)->pullAlgorithm.resolved = fxNewHostFunctionWithHandle(the, controller, ReadableByteStreamControllerPullAlgorithmResolved, 1); mxPop();
	(*controller)->pullAlgorithm.rejected = fxNewHostFunctionWithHandle(the, controller, ReadableByteStreamControllerPullAlgorithmRejected, 1); mxPop();

	(*controller)->stream = stream;
	(*stream)->controller = (ReadableStreamController*)controller;
	
	mxPop(); // controller
	return controller;
}
void ReadableByteStreamControllerStartAlgorithmResolved(xsMachine* the)
{
	ReadableByteStreamController* controller = (ReadableByteStreamController*)fxGetHostFunctionHandle(the);
	(*controller)->started = 1;
	ReadableByteStreamControllerCallPullIfNeeded(the, controller);
}
void ReadableByteStreamControllerStartAlgorithmRejected(xsMachine* the)
{
	ReadableByteStreamController* controller = (ReadableByteStreamController*)fxGetHostFunctionHandle(the);
	ReadableByteStreamControllerError(the, controller, mxArgv(0));
}
void ReadableByteStreamControllerPullAlgorithmResolved(xsMachine* the)
{
	ReadableByteStreamController* controller = (ReadableByteStreamController*)fxGetHostFunctionHandle(the);
	(*controller)->pulling = 0;
	if ((*controller)->pullAgain) {
		(*controller)->pullAgain = 0;
		ReadableByteStreamControllerCallPullIfNeeded(the, controller);
	}
}
void ReadableByteStreamControllerPullAlgorithmRejected(xsMachine* the)
{
	ReadableByteStreamController* controller = (ReadableByteStreamController*)fxGetHostFunctionHandle(the);
	ReadableByteStreamControllerError(the, controller, mxArgv(0));
}
void ReadableByteStreamControllerCallPullIfNeeded(xsMachine* the, ReadableByteStreamController* controller)
{
	if (!ReadableByteStreamControllerShouldCallPull(the, controller))
		return;
	if ((*controller)->pulling) {
		(*controller)->pullAgain = 1;
		return;
	}
	mxStreamAssert((*controller)->pullAgain == 0);
	(*controller)->pulling = 1;
	txSlot* result;
	mxTemporary(result);
	txBoolean success = (*((*controller)->pullAlgorithm.call))(the, (StreamStuff*)controller, NULL, result);
	fxChainAlgorithm(the, result, success, (*controller)->pullAlgorithm.resolved, (*controller)->pullAlgorithm.rejected);
	mxPop(); // result
}
void ReadableByteStreamControllerClearAlgorithms(xsMachine* the, ReadableByteStreamController* controller)
{
	(*controller)->pullAlgorithm.call = NULL;
	(*controller)->pullAlgorithm.callback = NULL;
	(*controller)->cancelAlgorithm.call = NULL;
	(*controller)->cancelAlgorithm.callback = NULL;
}
void ReadableByteStreamControllerClearPendingPullIntos(xsMachine* the, ReadableByteStreamController* controller)
{
	ReadableByteStreamControllerInvalidateBYOBRequest(the, controller);
	ResetSlotQueue(the, (*controller)->pendingPullIntos);
}
void ReadableByteStreamControllerClose(xsMachine* the, ReadableByteStreamController* controller)
{
	ReadableStream* stream = (*controller)->stream;
	if ((*controller)->closeRequested || ((*stream)->state != mcStreamReadable))
		return;
	if (GetBufferQueueTotalSize(the, (*controller)->queue) > 0) {
		(*controller)->closeRequested = 1;
		return;
	}
	if (GetSlotQueueLength(the, (*controller)->pendingPullIntos) > 0) {
		txSlot* pullIntoDescriptor;
		mxTemporary(pullIntoDescriptor);
		PeekSlotQueue(the, (*controller)->pendingPullIntos, pullIntoDescriptor);
		PullInto* pullInto = ToPullIntoHandle(the, pullIntoDescriptor);
    	if ((*pullInto)->bytesFilled % (*pullInto)->elementSize != 0) {
			mxPush(mxTypeErrorConstructor);
			fxNewError(the, "Insufficient bytes to fill elements in the given buffer");
			ReadableByteStreamControllerError(the, controller, the->stack);
			mxPull(mxException);
			fxJump(the);
		}
		mxPop(); // pullIntoDescriptor;
	}
	ReadableByteStreamControllerClearAlgorithms(the, controller);
	ReadableStreamClose(the, (*controller)->stream);
}
void ReadableByteStreamControllerCommitPullIntoDescriptor(xsMachine* the, ReadableStream* stream, xsSlot* pullIntoDescriptor)
{
	PullInto* pullInto = ToPullIntoHandle(the, pullIntoDescriptor);
	mxStreamAssert((*stream)->state != mcStreamErrored);
	mxStreamAssert((*pullInto)->readerType != mcNoneReaderType);
	txBoolean done = 0;
	if ((*stream)->state == mcStreamClosed) {
		mxStreamAssert((*pullInto)->bytesFilled % (*pullInto)->elementSize == 0);
		done = 1;
	}
	txSlot* filledView;
	mxTemporary(filledView);
	ReadableByteStreamControllerConvertPullIntoDescriptor(the, pullIntoDescriptor, filledView);
	ReadableStreamFulfillReadRequest(the, stream, filledView, done);
	mxPop();
}
void ReadableByteStreamControllerConvertPullIntoDescriptor(xsMachine* the, xsSlot* pullIntoDescriptor, xsSlot* view)
{
	PullInto* pullInto = ToPullIntoHandle(the, pullIntoDescriptor);
	mxStreamAssert((*pullInto)->bytesFilled <= (*pullInto)->byteLength);
	mxStreamAssert((*pullInto)->bytesFilled % (*pullInto)->elementSize == 0);
	mxPushReference((*pullInto)->ViewConstructor);
	mxNew();
	mxPushReference((*pullInto)->buffer);
	TransferArrayBuffer(the);
	mxPushInteger((*pullInto)->byteOffset);
	mxPushInteger((*pullInto)->bytesFilled / (*pullInto)->elementSize);
	mxRunCount(3);
	mxPullSlot(view);
}
void ReadableByteStreamControllerEnqueue(xsMachine* the, ReadableByteStreamController* controller, xsSlot* view)
{
	ReadableStream* stream = (*controller)->stream;
	if ((*controller)->closeRequested || ((*stream)->state != mcStreamReadable))
		return;
	ViewInfoRecord info;
	GetViewInfo(the, view, &info);
	txSlot* transferredBuffer;
	mxTemporary(transferredBuffer);	
	mxPushSlot(info.buffer);
	TransferArrayBuffer(the);
	mxPullSlot(transferredBuffer);
		
	if (GetSlotQueueLength(the, (*controller)->pendingPullIntos) > 0) {
		txSlot* pullIntoDescriptor;
		mxTemporary(pullIntoDescriptor);
		PeekSlotQueue(the, (*controller)->pendingPullIntos, pullIntoDescriptor);
		PullInto* pullInto = ToPullIntoHandle(the, pullIntoDescriptor);
			
		mxPushReference((*pullInto)->buffer);
		if (IsArrayBufferDetached(the))
			xsTypeError("detached buffer");
	
		ReadableByteStreamControllerInvalidateBYOBRequest(the, controller);
		mxPushReference((*pullInto)->buffer);
		TransferArrayBuffer(the);
		(*pullInto)->buffer = fxToReference(the, the->stack);
		mxPop();
		if ((*pullInto)->readerType == mcNoneReaderType)
			ReadableByteStreamControllerEnqueueDetachedPullIntoToQueue(the, controller, pullIntoDescriptor);
	}
		
	ReadableStreamReader* reader = (*stream)->reader;
	if (reader == NULL) {
		ReadableByteStreamControllerEnqueueChunkToQueue(the, controller, transferredBuffer, info.byteOffset, info.byteLength);
	}
	else if (IsReadableStreamDefaultReader(the, reader)) {
		ReadableByteStreamControllerProcessReadRequestsUsingQueue(the, controller);
		if (ReadableStreamGetNumReadRequests(the, stream) == 0) {
			mxStreamAssert(GetSlotQueueLength(the, (*controller)->pendingPullIntos) == 0);
			ReadableByteStreamControllerEnqueueChunkToQueue(the, controller, transferredBuffer, info.byteOffset, info.byteLength);
		}
		else {
			mxStreamAssert(GetBufferQueueLength(the, (*controller)->queue) == 0);
			if (GetSlotQueueLength(the, (*controller)->pendingPullIntos) > 0) {
				ReadableByteStreamControllerShiftPendingPullInto(the, controller);
			}
			mxPush(mxUint8ArrayConstructor);
			mxNew();
			mxPushSlot(transferredBuffer);
			mxPushInteger(info.byteOffset);
			mxPushInteger(info.byteLength);
			mxRunCount(3);
			ReadableStreamFulfillReadRequest(the, stream, the->stack, false);
			mxPop();
		}
	}
	else {
		ReadableByteStreamControllerEnqueueChunkToQueue(the, controller, transferredBuffer, info.byteOffset, info.byteLength);
		ReadableByteStreamControllerProcessPullIntoDescriptorsUsingQueue(the, controller);
	} 
	ReadableByteStreamControllerCallPullIfNeeded(the, controller);
	mxPop(); // transferredBuffer
}
void ReadableByteStreamControllerEnqueueChunkToQueue(xsMachine* the, ReadableByteStreamController* controller, xsSlot* buffer, txSize byteOffset, txSize byteLength)
{
	EnqueueBuffer(the, (*controller)->queue, buffer, byteOffset, byteLength);
} 
void ReadableByteStreamControllerEnqueueClonedChunkToQueue(xsMachine* the, ReadableByteStreamController* controller, xsSlot* buffer, txSize byteOffset, txSize byteLength)
{
	xsSlot* clonedBuffer;
	mxTemporary(clonedBuffer);
	txByte* clonedAddress = fxArrayBuffer(the, clonedBuffer, NULL, byteLength, -1);
	txByte* address = fxToArrayBuffer(the, buffer);
	c_memcpy(clonedAddress, address + byteOffset, byteLength);
	ReadableByteStreamControllerEnqueueChunkToQueue(the, controller, clonedBuffer, 0, byteLength);
	mxPop();
}
void ReadableByteStreamControllerEnqueueDetachedPullIntoToQueue(xsMachine* the, ReadableByteStreamController* controller, xsSlot* pullIntoDescriptor)
{
	PullInto* pullInto = ToPullIntoHandle(the, pullIntoDescriptor);
	mxStreamAssert((*pullInto)->readerType == mcNoneReaderType);
	if ((*pullInto)->bytesFilled > 0) {
		mxPushReference((*pullInto)->buffer);
		ReadableByteStreamControllerEnqueueClonedChunkToQueue(the, controller, the->stack, (*pullInto)->byteOffset, (*pullInto)->bytesFilled);
		mxPop();
	}
	ReadableByteStreamControllerShiftPendingPullInto(the, controller);
}
void ReadableByteStreamControllerError(xsMachine* the, ReadableByteStreamController* controller, xsSlot* error)
{
	ReadableStream* stream = (*controller)->stream;
	if ((*stream)->state != mcStreamReadable)
		return;
	ReadableByteStreamControllerClearPendingPullIntos(the, controller);
	ResetBufferQueue(the, (*controller)->queue);
	ReadableByteStreamControllerClearAlgorithms(the, controller);
	ReadableStreamError(the, stream, error);
}
void ReadableByteStreamControllerFillHeadPullIntoDescriptor(xsMachine* the, ReadableByteStreamController* controller, txInteger size, xsSlot* pullIntoDescriptor)
{
	PullInto* pullInto = ToPullIntoHandle(the, pullIntoDescriptor);
// 	assert((controller.pendingPullIntos.length == 0) || (controller.pendingPullIntos[0] === pullIntoDescriptor));
	mxStreamAssert((*controller)->byobRequest == NULL);
	(*pullInto)->bytesFilled += size;
}
txBoolean ReadableByteStreamControllerFillPullIntoDescriptorFromQueue(xsMachine* the, ReadableByteStreamController* controller, xsSlot* pullIntoDescriptor)
{
	txSize totalSize = GetBufferQueueTotalSize(the, (*controller)->queue);
	PullInto* pullInto = ToPullIntoHandle(the, pullIntoDescriptor);
	txSize maxBytesToCopy = (*pullInto)->byteLength - (*pullInto)->bytesFilled;
	if (maxBytesToCopy > totalSize)
		maxBytesToCopy = totalSize;
	txSize maxBytesFilled = (*pullInto)->bytesFilled + maxBytesToCopy;
	txSize totalBytesToCopyRemaining = maxBytesToCopy;
	txBoolean ready = 0;
	mxStreamAssert((*pullInto)->bytesFilled < (*pullInto)->minimumFill);
	txSize remainderBytes = maxBytesFilled % (*pullInto)->elementSize;
	txSize maxAlignedBytes = maxBytesFilled - remainderBytes;
	if (maxAlignedBytes >= (*pullInto)->minimumFill) {
		totalBytesToCopyRemaining = maxAlignedBytes - (*pullInto)->bytesFilled;
		ready = 1;
	}
	mxPushReference((*pullInto)->buffer);
	MoveBufferQueueBytes(the, (*controller)->queue, the->stack, (*pullInto)->byteOffset + (*pullInto)->bytesFilled, totalBytesToCopyRemaining);
	mxPop();
	(*pullInto)->bytesFilled += totalBytesToCopyRemaining;
	if (!ready) {
		mxStreamAssert(GetBufferQueueTotalSize(the, (*controller)->queue) == 0);
		mxStreamAssert((*pullInto)->bytesFilled > 0);
		mxStreamAssert((*pullInto)->bytesFilled < (*pullInto)->minimumFill);
	}
	return ready;
}
void ReadableByteStreamControllerFillReadRequestFromQueue(xsMachine* the, ReadableByteStreamController* controller, xsSlot* request)
{
	mxStreamAssert(GetBufferQueueTotalSize(the, (*controller)->queue) > 0);
	txSlot* buffer;
	txSize byteOffset, byteLength;
	mxTemporary(buffer);
	DequeueBuffer(the, (*controller)->queue, buffer, &byteOffset, &byteLength);
	ReadableByteStreamControllerHandleQueueDrain(the, controller);
	mxPush(mxUint8ArrayConstructor);
	mxNew();
	mxPushSlot(buffer);
	mxPushInteger(byteOffset);
	mxPushInteger(byteLength);
	mxRunCount(3);
	ReadableStream* stream = (*controller)->stream;
	ReadableStreamReader* reader = (*stream)->reader;
	(*((*reader)->chunkSteps))(the, reader, request, the->stack);
	mxPop();
}
void ReadableByteStreamControllerGetBYOBRequest(xsMachine* the, ReadableByteStreamController* controller)
{
	if (((*controller)->byobRequest == NULL) && (GetSlotQueueLength(the, (*controller)->pendingPullIntos) > 0)) {
		ReadableStream* stream = (*controller)->stream;
		mxPushReference((*stream)->closures);
		mxGetID(xsID_readableStreamBYOBRequest);
		xsSlot* instance = fxNewHostInstance(the);
		xsSlot* reference = the->stack;
		fxSetHostChunk(the, reference, NULL, sizeof(ReadableStreamBYOBRequestRecord));
		fxSetHostHooks(the, reference, (xsHostHooks*)&ReadableStreamBYOBRequestHooks);
		ReadableStreamBYOBRequest* request = (ReadableStreamBYOBRequest*)fxGetHostHandle(the, reference);
		(*request)->reference = instance;
		(*request)->dispatch = (StreamDispatch)&ReadableStreamBYOBRequestDispatchRecord;
		(*request)->controller = controller;
		
		txSlot* pullIntoDescriptor;
		mxTemporary(pullIntoDescriptor);
		PeekSlotQueue(the, (*controller)->pendingPullIntos, pullIntoDescriptor);
		PullInto* pullInto = ToPullIntoHandle(the, pullIntoDescriptor);
		mxPush(mxUint8ArrayConstructor);
		mxNew();
		mxPushReference((*pullInto)->buffer);
		mxPushInteger((*pullInto)->byteOffset + (*pullInto)->bytesFilled);
		mxPushInteger((*pullInto)->byteLength - (*pullInto)->bytesFilled);
		mxRunCount(3);
		(*request)->view = fxToReference(the, the->stack);
		mxPop();
		mxPop(); // pullIntoDescriptor
		
		(*controller)->byobRequest = request;
	}
}
txNumber ReadableByteStreamControllerGetDesiredSize(xsMachine* the, ReadableByteStreamController* controller)
{
	ReadableStream* stream = (*controller)->stream;
	if ((*stream)->state == mcStreamErrored)
		return C_NAN;
	if ((*stream)->state == mcStreamClosed)
		return 0;
	return (*controller)->strategyHWM - GetBufferQueueTotalSize(the, (*controller)->queue);
}
void ReadableByteStreamControllerHandleQueueDrain(xsMachine* the, ReadableByteStreamController* controller)
{
	ReadableStream* stream = (*controller)->stream;
	mxStreamAssert((*stream)->state == mcStreamReadable);
	if ((GetBufferQueueTotalSize(the, (*controller)->queue) == 0) && (*controller)->closeRequested) {
		ReadableByteStreamControllerClearAlgorithms(the, controller);
		ReadableStreamClose(the, stream);
	}
	else {
		ReadableByteStreamControllerCallPullIfNeeded(the, controller);
	}
}
void ReadableByteStreamControllerInvalidateBYOBRequest(xsMachine* the, ReadableByteStreamController* controller)
{
	ReadableStreamBYOBRequest* request = (*controller)->byobRequest;
	if (request == NULL)
		return;
	(*request)->controller = NULL;
	(*request)->view = NULL;
	(*controller)->byobRequest = NULL;
}
void ReadableByteStreamControllerProcessPullIntoDescriptorsUsingQueue(xsMachine* the, ReadableByteStreamController* controller)
{
	mxStreamAssert((*controller)->closeRequested == 0);
	txSlot* pullIntoDescriptor;
	mxTemporary(pullIntoDescriptor);
	while (GetSlotQueueLength(the, (*controller)->pendingPullIntos) > 0) {
		if (GetBufferQueueTotalSize(the, (*controller)->queue) == 0)
			return;
		PeekSlotQueue(the, (*controller)->pendingPullIntos, pullIntoDescriptor);
		if (ReadableByteStreamControllerFillPullIntoDescriptorFromQueue(the, controller, pullIntoDescriptor)) {
			ReadableByteStreamControllerShiftPendingPullInto(the, controller);
			ReadableByteStreamControllerCommitPullIntoDescriptor(the, (*controller)->stream, pullIntoDescriptor);
		}
	}
	mxPop(); // pullIntoDescriptor
}
void ReadableByteStreamControllerProcessReadRequestsUsingQueue(xsMachine* the, ReadableByteStreamController* controller)
{
	ReadableStream* stream = (*controller)->stream;
	ReadableStreamReader* reader = (*stream)->reader;
	mxStreamAssert((reader != NULL) && IsReadableStreamDefaultReader(the, reader));
	xsSlot* request;
	mxTemporary(request);
	while (GetSlotQueueLength(the, (*reader)->queue) > 0) {
		if (GetBufferQueueTotalSize(the, (*controller)->queue) == 0)
			return;
		DequeueSlot(the, (*reader)->queue, request);
		ReadableByteStreamControllerFillReadRequestFromQueue(the, controller, request);
	}
	mxPop();
}
void ReadableByteStreamControllerPullInto(xsMachine* the, ReadableByteStreamController* controller, txSlot* view, txSize min, txSlot* request)
{
	ReadableStream* stream = (*controller)->stream;
	txSlot* pullIntoDescriptor;
	mxTemporary(pullIntoDescriptor);
	PullInto* pullInto = CreatePullInto(the, pullIntoDescriptor);
	
	ViewInfoRecord info;
	GetViewInfo(the, view, &info);

	(*pullInto)->ViewConstructor = info.ViewConstructor->value.reference;

	(*pullInto)->bufferByteLength = info.bufferByteLength;
	(*pullInto)->byteLength = info.byteLength;
	(*pullInto)->byteOffset = info.byteOffset;
	(*pullInto)->elementSize = info.elementSize;
	(*pullInto)->bytesFilled = 0; 
	(*pullInto)->minimumFill = min * info.elementSize; 
	(*pullInto)->readerType = mcBYOBReaderType;
	
	{
		mxTry(the) {
			mxPushSlot(info.buffer);
			TransferArrayBuffer(the);
			(*pullInto)->buffer = fxToReference(the, the->stack);
			mxPop();
		}
		mxCatch(the) {
			ReadableStreamReader* reader = (*stream)->reader;
			mxPush(mxException);
			mxException = xsUndefined;
			(*((*reader)->errorSteps))(the, reader, request, the->stack);
			mxPop();
			goto bail;
		}
	}
	
	if (GetSlotQueueLength(the, (*controller)->pendingPullIntos) > 0) {
		EnqueueSlot(the, (*controller)->pendingPullIntos, pullIntoDescriptor);
		ReadableStreamAddReadRequest(the, stream, request);
		goto bail;
	}
	if ((*stream)->state == mcStreamClosed) {
		xsSlot* emptyView;
		mxTemporary(emptyView);
		PullInto* pullInto = ToPullIntoHandle(the, pullIntoDescriptor);
		mxPushReference((*pullInto)->ViewConstructor);
		mxNew();
		mxPushReference((*pullInto)->buffer);
		mxPushInteger((*pullInto)->byteOffset);
		mxPushInteger(0);
		mxRunCount(3);
		mxPullSlot(emptyView);
		ReadableStreamReader* reader = (*stream)->reader;
		(*((*reader)->closeSteps))(the, reader, request, emptyView);
		mxPop();
		goto bail;
	}
	if (GetBufferQueueTotalSize(the, (*controller)->queue) > 0) {
		if (ReadableByteStreamControllerFillPullIntoDescriptorFromQueue(the, controller, pullIntoDescriptor)) {
			xsSlot* filledView;
			mxTemporary(filledView);
			ReadableByteStreamControllerConvertPullIntoDescriptor(the, pullIntoDescriptor, filledView);
			ReadableByteStreamControllerHandleQueueDrain(the, controller);
			ReadableStreamReader* reader = (*stream)->reader;
			(*((*reader)->chunkSteps))(the, reader, request, filledView);
			mxPop();
			goto bail;
		}
		if ((*controller)->closeRequested) {
			mxPush(mxTypeErrorConstructor);
			fxNewError(the, "closed");
			ReadableByteStreamControllerError(the, controller, the->stack);
			ReadableStreamReader* reader = (*stream)->reader;
			(*((*reader)->errorSteps))(the, reader, request, the->stack);
			mxPop();
			goto bail;
		}
	}
	EnqueueSlot(the, (*controller)->pendingPullIntos, pullIntoDescriptor);
	ReadableStreamAddReadRequest(the, stream, request);
	ReadableByteStreamControllerCallPullIfNeeded(the, controller);
bail:
	mxPop(); // pullIntoDescriptor
}
void ReadableByteStreamControllerRespond(xsMachine* the, ReadableByteStreamController* controller, txInteger bytesWritten)
{
	mxStreamAssert(GetSlotQueueLength(the, (*controller)->pendingPullIntos) > 0);
	txSlot* pullIntoDescriptor;
	mxTemporary(pullIntoDescriptor);
	PeekSlotQueue(the, (*controller)->pendingPullIntos, pullIntoDescriptor);
	PullInto* pullInto = ToPullIntoHandle(the, pullIntoDescriptor);
	ReadableStream* stream = (*controller)->stream;
	if ((*stream)->state == mcStreamClosed) {
		if (bytesWritten != 0)
			xsTypeError("closed");
	}
	else {
		mxStreamAssert((*stream)->state == mcStreamReadable);
		if (bytesWritten == 0)
			xsTypeError("no bytes written");
		if ((*pullInto)->bytesFilled + bytesWritten > (*pullInto)->byteLength)
			xsRangeError("too long");
	}
	mxPushReference((*pullInto)->buffer);
	TransferArrayBuffer(the);
	(*pullInto)->buffer = fxToReference(the, the->stack);
	mxPop();
	ReadableByteStreamControllerRespondInternal(the, controller, bytesWritten);
}
void ReadableByteStreamControllerRespondInClosedState(xsMachine* the, ReadableByteStreamController* controller, xsSlot* pullIntoDescriptor)
{
	PullInto* pullInto = ToPullIntoHandle(the, pullIntoDescriptor);
	mxStreamAssert((*pullInto)->bytesFilled % (*pullInto)->elementSize == 0);
	if ((*pullInto)->readerType == mcNoneReaderType)
		ReadableByteStreamControllerShiftPendingPullInto(the, controller);
	ReadableStream* stream = (*controller)->stream;
	ReadableStreamReader* reader = (*stream)->reader;
	if (reader && IsReadableStreamBYOBReader(the, reader)) {
		mxTemporary(pullIntoDescriptor);
		while (ReadableStreamGetNumReadRequests(the, stream) > 0) {
			DequeueSlot(the, (*controller)->pendingPullIntos, pullIntoDescriptor);
			ReadableByteStreamControllerCommitPullIntoDescriptor(the, stream, pullIntoDescriptor);
		}
		mxPop();
	}
}
void ReadableByteStreamControllerRespondInReadableState(xsMachine* the, ReadableByteStreamController* controller, txInteger bytesWritten, xsSlot* pullIntoDescriptor)
{
	PullInto* pullInto = ToPullIntoHandle(the, pullIntoDescriptor);
	mxStreamAssert((*pullInto)->bytesFilled + bytesWritten <= (*pullInto)->byteLength);
	ReadableByteStreamControllerFillHeadPullIntoDescriptor(the, controller, bytesWritten, pullIntoDescriptor);
	if ((*pullInto)->readerType == mcNoneReaderType) {
		ReadableByteStreamControllerEnqueueDetachedPullIntoToQueue(the, controller, pullIntoDescriptor);
		ReadableByteStreamControllerProcessPullIntoDescriptorsUsingQueue(the, controller);
		return;
	}
	if ((*pullInto)->bytesFilled < (*pullInto)->minimumFill)
		return;
	ReadableByteStreamControllerShiftPendingPullInto(the, controller);
	txSize remainderSize = (*pullInto)->bytesFilled % (*pullInto)->elementSize;
	if (remainderSize > 0) {
		txSize end = (*pullInto)->byteOffset + (*pullInto)->bytesFilled;
		mxPushReference((*pullInto)->buffer);
		ReadableByteStreamControllerEnqueueClonedChunkToQueue(the, controller, the->stack, end - remainderSize, remainderSize);
	}
	(*pullInto)->bytesFilled -= remainderSize;
	ReadableByteStreamControllerCommitPullIntoDescriptor(the, (*controller)->stream, pullIntoDescriptor);
	ReadableByteStreamControllerProcessPullIntoDescriptorsUsingQueue(the, controller);
}
void ReadableByteStreamControllerRespondInternal(xsMachine* the, ReadableByteStreamController* controller, txInteger bytesWritten)
{
	txSlot* pullIntoDescriptor;
	mxTemporary(pullIntoDescriptor);
	PeekSlotQueue(the, (*controller)->pendingPullIntos, pullIntoDescriptor);
// 	assert(CanTransferArrayBuffer(firstDescriptor.buffer));
	ReadableByteStreamControllerInvalidateBYOBRequest(the, controller);
	ReadableStream* stream = (*controller)->stream;
	if ((*stream)->state == mcStreamClosed) {
		mxStreamAssert(bytesWritten == 0);
		ReadableByteStreamControllerRespondInClosedState(the, controller, pullIntoDescriptor);
	}
	else {
		mxStreamAssert((*stream)->state == mcStreamReadable);
		mxStreamAssert(bytesWritten > 0);
		ReadableByteStreamControllerRespondInReadableState(the, controller, bytesWritten, pullIntoDescriptor);
	}
	ReadableByteStreamControllerCallPullIfNeeded(the, controller);
}
void ReadableByteStreamControllerRespondWithNewView(xsMachine* the, ReadableByteStreamController* controller, txSlot* view)
{
	mxStreamAssert(GetSlotQueueLength(the, (*controller)->pendingPullIntos) > 0);
// 	assert(!IsDetachedBuffer(view.buffer));
	txSlot* pullIntoDescriptor;
	mxTemporary(pullIntoDescriptor);
	PeekSlotQueue(the, (*controller)->pendingPullIntos, pullIntoDescriptor);
	PullInto* pullInto = ToPullIntoHandle(the, pullIntoDescriptor);
	
	ViewInfoRecord info;
	GetViewInfo(the, view, &info);
		
	ReadableStream* stream = (*controller)->stream;
	if ((*stream)->state == mcStreamClosed) {
		if (info.byteLength != 0)
			xsTypeError("closed");
	}
	else {
		mxStreamAssert((*stream)->state == mcStreamReadable);
		if (info.byteLength == 0)
			xsTypeError("no bytes written");
	}
	if ((*pullInto)->byteOffset + (*pullInto)->bytesFilled != info.byteOffset)
		xsRangeError("byteOffset");
	if ((*pullInto)->bufferByteLength != info.bufferByteLength)
		xsRangeError("buffer.byteLength");
	if ((*pullInto)->bytesFilled + info.byteLength > (*pullInto)->byteLength)
		xsRangeError("byteLength");
	
	mxPushSlot(info.buffer);
	TransferArrayBuffer(the);
	(*pullInto)->buffer = fxToReference(the, the->stack);
	mxPop();
	
	ReadableByteStreamControllerRespondInternal(the, controller, info.byteLength);
}
void ReadableByteStreamControllerShiftPendingPullInto(xsMachine* the, ReadableByteStreamController* controller)
{
	mxStreamAssert((*controller)->byobRequest == NULL);
	ShiftSlotQueue(the, (*controller)->pendingPullIntos);
}
txBoolean ReadableByteStreamControllerShouldCallPull(xsMachine* the, ReadableByteStreamController* controller)
{
	ReadableStream* stream = (*controller)->stream;
	if ((*stream)->state != mcStreamReadable)
		return 0;
	if ((*controller)->closeRequested)
		return 0;
	if (!(*controller)->started)
		return 0;
	ReadableStreamReader* reader = (*stream)->reader;
	if ((reader != NULL) && (ReadableStreamGetNumReadRequests(the, stream) > 0))
		return 1;
	txNumber desiredSize = ReadableByteStreamControllerGetDesiredSize(the, controller);
	return (desiredSize > 0) ? 1 : 0;
}

txBoolean IsReadableByteStreamController(xsMachine* the, ReadableStreamController* controller)
{
	return ((*controller)->dispatch == &ReadableByteStreamControllerDispatchRecord) ? 1 : 0;
}
void ReadableByteStreamControllerStart(xsMachine* the, ReadableByteStreamController* controller)
{
	txSlot* result;
	mxTemporary(result);
	txBoolean success = (*((*controller)->startAlgorithm.call))(the, (StreamStuff*)controller, NULL, result);
	fxChainAlgorithm(the, result, success, (*controller)->startAlgorithm.resolved, (*controller)->startAlgorithm.rejected);
	mxPop(); // result
}

// 4.8 ReadableStreamBYOBRequest 
void ReadableStreamBYOBRequest_destructor(void* it)
{
}
void ReadableStreamBYOBRequest_mark(xsMachine* the, void* it, xsMarkRoot markRoot)
{
	ReadableStreamBYOBRequest self = it;
	StreamMarkHandle(the, self->controller);
	StreamMarkReference(the, self->view);
}
void ReadableStreamBYOBRequest_get_view(xsMachine* the)
{
	ReadableStreamBYOBRequest* request = mxStreamHandle(ReadableStreamBYOBRequest, mxThis);
	if ((*request)->view)
		mxPushReference((*request)->view);
	else
		mxPushNull();
	mxPullSlot(mxResult);
}
void ReadableStreamBYOBRequest_respond(xsMachine* the)
{
	ReadableStreamBYOBRequest* request = mxStreamHandle(ReadableStreamBYOBRequest, mxThis);
	if ((*request)->controller == NULL)
		xsTypeError("no controller");
	{
		ViewInfoRecord info;
		mxPushReference((*request)->view);
		GetViewInfo(the, the->stack, &info);
		mxPushSlot(info.buffer);
		if (IsArrayBufferDetached(the))
			xsTypeError("detached buffer");
		mxPop();
		mxPop();
	}
	if (mxArgc > 0)
		mxPushSlot(mxArgv(0));
	else
		mxPushUndefined();
	xsNumberValue bytesWritten = fxToNumber(the, the->stack);
	if (c_isnan(bytesWritten) || (bytesWritten < 0) || (bytesWritten > (xsNumberValue)0x7fffffff))
		xsTypeError("invalid bytesWritten");
	ReadableByteStreamControllerRespond(the, (*request)->controller, (txInteger)bytesWritten);
	mxPop();
}
void ReadableStreamBYOBRequest_respondWithNewView(xsMachine* the)
{
	ReadableStreamBYOBRequest* request = mxStreamHandle(ReadableStreamBYOBRequest, mxThis);
	if ((*request)->controller == NULL)
		xsTypeError("no controller");
	if (mxArgc > 0)
		mxPushSlot(mxArgv(0));
	else
		mxPushUndefined();
	{
		ViewInfoRecord info;
		GetViewInfo(the, the->stack, &info);
		mxPushSlot(info.buffer);
		if (IsArrayBufferDetached(the))
			xsTypeError("detached buffer");
		mxPop();
	}
	ReadableByteStreamControllerRespondWithNewView(the, (*request)->controller, the->stack);
	mxPop();
}

xsSlot* CreateBufferQueue(xsMachine* the)
{
	xsSlot* instance = fxNewInstance(the);
	xsSlot* length = fxNextIntegerProperty(the, instance, 0, XS_NO_ID, XS_INTERNAL_FLAG);
	xsSlot* totalSize = fxNextNumberProperty(the, length, 0, XS_NO_ID, XS_INTERNAL_FLAG);
	xsSlot* list = fxNextUndefinedProperty(the, totalSize, XS_NO_ID, XS_INTERNAL_FLAG);
	list->value.list.first = C_NULL;	
	list->value.list.last = C_NULL;	
	list->kind = XS_LIST_KIND;
	return instance;
}
void DequeueBuffer(xsMachine* the, xsSlot* queue, xsSlot* buffer, txSize* byteOffset, txSize* byteLength)
{
	xsSlot* length = queue->next;
	xsSlot* totalSize = length->next;
	xsSlot* list = totalSize->next;
	xsSlot* slot = list->value.list.first;
	buffer->kind = slot->kind;
	buffer->value = slot->value;
	slot = slot->next;
	*byteOffset = slot->value.dataView.offset;
	*byteLength = slot->value.dataView.size;
	list->value.list.first = slot->next;
	if (list->value.list.first == NULL)
		list->value.list.last = NULL;
	length->value.integer--;
	totalSize->value.number -= *byteLength;
	if (totalSize->value.number < 0)
		totalSize->value.number = 0;
}
void EnqueueBuffer(xsMachine* the, xsSlot* queue, xsSlot* buffer, txSize byteOffset, txSize byteLength)
{
	xsSlot* length = queue->next;
	xsSlot* totalSize = length->next;
	xsSlot* list = totalSize->next;
	xsSlot* slot = fxNewSlot(the);
	slot->kind = buffer->kind;
	slot->value = buffer->value;
	if (list->value.list.last == NULL)
		list->value.list.first = slot;
	else
		list->value.list.last->next = slot;
	list->value.list.last = slot;
	
	slot = fxNewSlot(the);
	slot->flag = XS_INTERNAL_FLAG;
	slot->kind = XS_DATA_VIEW_KIND;
	slot->value.dataView.offset = byteOffset;
	slot->value.dataView.size = byteLength;
	list->value.list.last->next = slot;
	list->value.list.last = slot;
	
	length->value.integer++;
	totalSize->value.number += byteLength;
}
txInteger GetBufferQueueLength(xsMachine* the, xsSlot* queue)
{
	xsSlot* length = queue->next;
	return length->value.integer;
}
txInteger GetBufferQueueTotalSize(xsMachine* the, xsSlot* queue)
{
	xsSlot* length = queue->next;
	xsSlot* totalSize = length->next;
	return totalSize->value.number;
}
void MoveBufferQueueBytes(xsMachine* the, xsSlot* queue, xsSlot* target, txSize dstOffset, txSize dstSize)
{
	xsSlot* length = queue->next;
	xsSlot* totalSize = length->next;
	xsSlot* list = totalSize->next;
	xsSlot* slot = list->value.list.first;
	
	txByte* dstAddress = fxToArrayBuffer(the, target);
	dstAddress += dstOffset;
	
	txByte* srcAddress = fxToArrayBuffer(the, slot);
	slot = slot->next;
	srcAddress += slot->value.dataView.offset;
	txSize srcSize = slot->value.dataView.size;
	
	while (dstSize > 0) {
		txSize size = (srcSize < dstSize) ? srcSize : dstSize;
		c_memcpy(dstAddress, srcAddress, size);
		srcAddress += size;
		srcSize -= size;
		dstAddress += size;
		dstSize -= size;
		totalSize->value.number -= size;
		if (srcSize == 0) {
			length->value.integer--;
			slot = slot->next;
			if (slot) {
				list->value.list.first = slot;
				srcAddress = fxToArrayBuffer(the, slot);
				slot = slot->next;
				srcAddress += slot->value.dataView.offset;
				srcSize = slot->value.dataView.size;
			}
			else {
				list->value.list.first = NULL;
				list->value.list.last = NULL;
			}
		}
		else {
			slot->value.dataView.offset += size;
			slot->value.dataView.size -= size;
		}
	}
}
void ResetBufferQueue(xsMachine* the, xsSlot* queue)
{
	xsSlot* length = queue->next;
	xsSlot* totalSize = length->next;
	xsSlot* list = totalSize->next;
	length->value.integer = 0;
	totalSize->value.number = 0;
	list->value.list.first = C_NULL;	
	list->value.list.last = C_NULL;	
}

void GetViewInfo(xsMachine* the, xsSlot* view, ViewInfo info)
{
	if (view->kind == XS_REFERENCE_KIND) {
		txSlot* slot = view->value.reference->next;
		if (slot && (slot->flag & XS_INTERNAL_FLAG)) {
			if (slot->kind == XS_TYPED_ARRAY_KIND) {
				info->elementSize = slot->value.typedArray.dispatch->size;
				info->ViewConstructor = &the->stackIntrinsics[-1 - (txInteger)slot->value.typedArray.dispatch->constructorID];
				slot = slot->next;
			}
			else {
				info->elementSize = 1;
				info->ViewConstructor = &mxDataViewConstructor;
			}
			if (slot->kind == XS_DATA_VIEW_KIND) {
				info->byteOffset = slot->value.dataView.offset;
				info->byteLength = slot->value.dataView.size;
				slot = slot->next;
				if (slot->kind == XS_REFERENCE_KIND) {
					info->buffer = slot;
					slot = slot->value.reference->next;
					if (slot && (slot->flag & XS_INTERNAL_FLAG)) {
						if (slot->kind == XS_ARRAY_BUFFER_KIND) {
							slot = slot->next;
							if (slot->kind == XS_BUFFER_INFO_KIND) {
								info->bufferByteLength = slot->value.bufferInfo.length;
								return;
							}
						}
					}
				}
			}
		}
	}
	mxTypeError("not a view");
}

txBoolean IsArrayBufferDetached(xsMachine* the)
{
	txSlot* arrayBuffer = the->stack->value.reference->next;
	return (arrayBuffer->value.arrayBuffer.address == C_NULL) ? 1 : 0;
}
void TransferArrayBuffer(xsMachine* the)
{
	txSlot* it = the->stack;
	txSlot* fromInstance = the->stack->value.reference;
	txSlot* fromArrayBuffer = fromInstance->next;
	txSlot* fromBufferInfo = fromArrayBuffer->next;

	if (fromArrayBuffer->value.arrayBuffer.address == C_NULL)
		mxTypeError("detached buffer");
	mxPush(mxArrayBufferPrototype);
	txSlot* toInstance = fxNewObjectInstance(the);
	txSlot* toArrayBuffer = toInstance->next = fxNewSlot(the);
	toArrayBuffer->flag = XS_INTERNAL_FLAG;
	toArrayBuffer->kind = XS_ARRAY_BUFFER_KIND;
	toArrayBuffer->value.arrayBuffer.address = fromArrayBuffer->value.arrayBuffer.address;
	toArrayBuffer->value.arrayBuffer.detachKey = fromArrayBuffer->value.arrayBuffer.detachKey;
	txSlot* toBufferInfo = toArrayBuffer->next = fxNewSlot(the);
	toBufferInfo->flag = XS_INTERNAL_FLAG;
	toBufferInfo->kind = XS_BUFFER_INFO_KIND;
	toBufferInfo->value.bufferInfo.length = fromBufferInfo->value.bufferInfo.length;
	toBufferInfo->value.bufferInfo.maxLength = fromBufferInfo->value.bufferInfo.maxLength;
	
	fromArrayBuffer->value.arrayBuffer.address = C_NULL;
	fromArrayBuffer->value.arrayBuffer.detachKey = C_NULL;
	fromBufferInfo->value.bufferInfo.length = 0;
	fromBufferInfo->value.bufferInfo.maxLength = 0;
	
	mxPullSlot(it);
}

static PullInto* CreatePullInto(xsMachine* the, txSlot* it)
{
	fxNewHostObject(the, NULL);
	mxPullSlot(it);
	fxSetHostChunk(the, it, NULL, sizeof(PullIntoRecord));
	fxSetHostHooks(the, it, (xsHostHooks*)&PullIntoHooks);
	return (PullInto*)&(it->value.reference->next->value.host.data);
}
void DestroyPullInto(void* it)
{
}
void MarkPullInto(xsMachine* the, void* it, xsMarkRoot markRoot)
{
	PullInto self = it;
	StreamMarkReference(the, self->ViewConstructor);
	StreamMarkReference(the, self->buffer);
}
static PullInto* ToPullIntoHandle(xsMachine* the, txSlot* it)
{
	txSlot* host = NULL;
	if (it->kind == XS_REFERENCE_KIND) {
		it = it->value.reference;
		if (it->next) {
			it = it->next;
			if ((it->flag & XS_INTERNAL_FLAG) && (it->kind == XS_HOST_KIND))
				host = it;
		}
	}
	if (host) {
		return (PullInto*)&host->value.host.data;
	}
	mxTypeError("not a PullInto");
}
