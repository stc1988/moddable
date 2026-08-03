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

static void ReadableStreamDefaultControllerCancelSteps(xsMachine* the, ReadableStreamController* controller, txSlot* reason, xsSlot* result);
static void ReadableStreamDefaultControllerPullSteps(xsMachine* the, ReadableStreamController* controller, xsSlot* request);
static void ReadableStreamDefaultControllerReleaseSteps(xsMachine* the, ReadableStreamController* controller);

static void ReadableStreamDefaultControllerStartAlgorithmResolved(xsMachine* the);
static void ReadableStreamDefaultControllerStartAlgorithmRejected(xsMachine* the);
static void ReadableStreamDefaultControllerPullAlgorithmResolved(xsMachine* the);
static void ReadableStreamDefaultControllerPullAlgorithmRejected(xsMachine* the);

static void ReadableStreamDefaultControllerCallPullIfNeeded(xsMachine* the, ReadableStreamDefaultController* controller);
static void ReadableStreamDefaultControllerClearAlgorithms(xsMachine* the, ReadableStreamDefaultController* controller);
static txBoolean ReadableStreamDefaultControllerShouldCallPull(xsMachine* the, ReadableStreamDefaultController* controller);

static void ReadableStreamDefaultController_mark(xsMachine* the, void* it, xsMarkRoot markRoot);
static const xsHostHooks ReadableStreamDefaultControllerHooks ICACHE_RODATA_ATTR = {
	ReadableStreamDefaultController_destructor,
	ReadableStreamDefaultController_mark,
	NULL
};
static const ReadableStreamControllerDispatchRecord ReadableStreamDefaultControllerDispatchRecord = {
	"ReadableStreamDefaultController",
	ReadableStreamDefaultControllerCancelSteps,
	ReadableStreamDefaultControllerPullSteps,
	ReadableStreamDefaultControllerReleaseSteps,
};

// 4.6 ReadableStreamDefaultController

void ReadableStreamDefaultController_destructor(void* it)
{
}
void ReadableStreamDefaultController_mark(xsMachine* the, void* it, xsMarkRoot markRoot)
{
	ReadableStreamDefaultController self = it;
	StreamMarkHandle(the, self->stream);
	StreamMarkReference(the, self->queue);
	StreamMarkReference(the, self->strategySizeAlgorithm);
	StreamMarkReference(the, self->target);
	StreamMarkAlgorithm(the, &self->startAlgorithm, markRoot);
	StreamMarkAlgorithm(the, &self->pullAlgorithm, markRoot);
	StreamMarkAlgorithm(the, &self->cancelAlgorithm, markRoot);
}
void ReadableStreamDefaultController_get_desiredSize(xsMachine* the)
{
	ReadableStreamDefaultController* controller = mxStreamHandle(ReadableStreamDefaultController, mxThis);
	txNumber size = ReadableStreamDefaultControllerGetDesiredSize(the, controller);
	if (c_isnan(size))
		xsResult = xsNull;
	else
		xsResult = xsNumber(size);
}
void ReadableStreamDefaultController_close(xsMachine* the)
{
	ReadableStreamDefaultController* controller = mxStreamHandle(ReadableStreamDefaultController, mxThis);
	if (!ReadableStreamDefaultControllerCanCloseOrEnqueue(the, controller))
		xsTypeError("cannot close");
	ReadableStreamDefaultControllerClose(the, controller);
}
void ReadableStreamDefaultController_enqueue(xsMachine* the)
{
	ReadableStreamDefaultController* controller = mxStreamHandle(ReadableStreamDefaultController, mxThis);
	if (!ReadableStreamDefaultControllerCanCloseOrEnqueue(the, controller))
		xsTypeError("cannot enqueue");
	if (mxArgc > 0)
		mxPushSlot(mxArgv(0));
	else
		mxPushUndefined();
	ReadableStreamDefaultControllerEnqueue(the, controller, the->stack);
	mxPop();
}
void ReadableStreamDefaultController_error(xsMachine* the)
{
	ReadableStreamDefaultController* controller = mxStreamHandle(ReadableStreamDefaultController, mxThis);
	if (mxArgc > 0)
		mxPushSlot(mxArgv(0));
	else
		mxPushUndefined();
	ReadableStreamDefaultControllerError(the, controller, the->stack);
	mxPop();
}
// 4.6.4 Internal methods
void ReadableStreamDefaultControllerCancelSteps(xsMachine* the, ReadableStreamController* it, txSlot* reason, xsSlot* result)
{
	ReadableStreamDefaultController* controller = (ReadableStreamDefaultController*)it;
	ResetQueue(the, (*controller)->queue);
	txBoolean success = (*((*controller)->cancelAlgorithm.call))(the, (StreamStuff*)it, reason, result);
	if (success)
		fxCreateResolvedPromise(the, result, result);
	else
		fxCreateRejectedPromise(the,result, result);
	ReadableStreamDefaultControllerClearAlgorithms(the, controller);
}
void ReadableStreamDefaultControllerPullSteps(xsMachine* the, ReadableStreamController* it, xsSlot* request)
{
	ReadableStreamDefaultController* controller = (ReadableStreamDefaultController*)it;
	if (GetQueueLength(the, (*controller)->queue) > 0) {
		txSlot* value;
		mxTemporary(value);
		DequeueValueSize(the, (*controller)->queue, value);
		if (((*controller)->closeRequested) && (GetQueueLength(the, (*controller)->queue) == 0)) {
			ReadableStreamDefaultControllerClearAlgorithms(the, controller);
			ReadableStreamClose(the, (*controller)->stream);
		}
		else {
			ReadableStreamDefaultControllerCallPullIfNeeded(the, controller);
		}
		ReadableStream* stream = (*controller)->stream;
		ReadableStreamReader* reader = (*stream)->reader;
		(*((*reader)->chunkSteps))(the, reader, request, value);
		mxPop();
	}
	else {
		ReadableStreamAddReadRequest(the, (*controller)->stream, request);
		ReadableStreamDefaultControllerCallPullIfNeeded(the, controller);
	}
} 
void ReadableStreamDefaultControllerReleaseSteps(xsMachine* the, ReadableStreamController* controller)
{
}

// 4.9.4 Default controllers
ReadableStreamDefaultController* CreateReadableStreamDefaultController(xsMachine* the, ReadableStream* stream)
{
	mxPushReference((*stream)->closures);
	mxGetID(xsID_readableStreamDefaultController);
	xsSlot* instance = fxNewHostInstance(the);
	xsSlot* reference = the->stack;
	fxSetHostChunk(the, reference, NULL, sizeof(ReadableStreamDefaultControllerRecord));
	fxSetHostHooks(the, reference, (xsHostHooks*)&ReadableStreamDefaultControllerHooks);
	ReadableStreamDefaultController* controller = (ReadableStreamDefaultController*)fxGetHostHandle(the, reference);
	(*controller)->reference = instance;
	(*controller)->dispatch = (ReadableStreamControllerDispatch)&ReadableStreamDefaultControllerDispatchRecord;
	
	(*controller)->queue = CreateValueSizeQueue(the);
	
	(*controller)->startAlgorithm.resolved = fxNewHostFunctionWithHandle(the, controller, ReadableStreamDefaultControllerStartAlgorithmResolved, 1); mxPop();
	(*controller)->startAlgorithm.rejected = fxNewHostFunctionWithHandle(the, controller, ReadableStreamDefaultControllerStartAlgorithmRejected, 1); mxPop();
	(*controller)->pullAlgorithm.resolved = fxNewHostFunctionWithHandle(the, controller, ReadableStreamDefaultControllerPullAlgorithmResolved, 1); mxPop();
	(*controller)->pullAlgorithm.rejected = fxNewHostFunctionWithHandle(the, controller, ReadableStreamDefaultControllerPullAlgorithmRejected, 1); mxPop();

	(*controller)->stream = stream;
	(*stream)->controller = (ReadableStreamController*)controller;
	
	mxPop(); // controller
	return controller;
}

void ReadableStreamDefaultControllerStartAlgorithmResolved(xsMachine* the)
{
	ReadableStreamDefaultController* controller = (ReadableStreamDefaultController*)fxGetHostFunctionHandle(the);
	(*controller)->started = 1;
	ReadableStreamDefaultControllerCallPullIfNeeded(the, controller);
}
void ReadableStreamDefaultControllerStartAlgorithmRejected(xsMachine* the)
{
	ReadableStreamDefaultController* controller = (ReadableStreamDefaultController*)fxGetHostFunctionHandle(the);
	ReadableStreamDefaultControllerError(the, controller, mxArgv(0));
}
void ReadableStreamDefaultControllerPullAlgorithmResolved(xsMachine* the)
{
	ReadableStreamDefaultController* controller = (ReadableStreamDefaultController*)fxGetHostFunctionHandle(the);
	(*controller)->pulling = 0;
	if ((*controller)->pullAgain) {
		(*controller)->pullAgain = 0;
		ReadableStreamDefaultControllerCallPullIfNeeded(the, controller);
	}
}
void ReadableStreamDefaultControllerPullAlgorithmRejected(xsMachine* the)
{
	ReadableStreamDefaultController* controller = (ReadableStreamDefaultController*)fxGetHostFunctionHandle(the);
	ReadableStreamDefaultControllerError(the, controller, mxArgv(0));
}
void ReadableStreamDefaultControllerCallPullIfNeeded(xsMachine* the, ReadableStreamDefaultController* controller)
{
	if (!ReadableStreamDefaultControllerShouldCallPull(the, controller))
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
txBoolean ReadableStreamDefaultControllerCanCloseOrEnqueue(xsMachine* the, ReadableStreamDefaultController* controller)
{
	ReadableStream* stream = (*controller)->stream;
	return ((!(*controller)->closeRequested) && ((*stream)->state == mcStreamReadable)) ? 1 : 0;
}
void ReadableStreamDefaultControllerClearAlgorithms(xsMachine* the, ReadableStreamDefaultController* controller)
{
	(*controller)->strategySizeAlgorithm = NULL;
	(*controller)->pullAlgorithm.call = NULL;
	(*controller)->pullAlgorithm.callback = NULL;
	(*controller)->cancelAlgorithm.call = NULL;
	(*controller)->cancelAlgorithm.callback = NULL;
}
void ReadableStreamDefaultControllerClose(xsMachine* the, ReadableStreamDefaultController* controller)
{
	if (!ReadableStreamDefaultControllerCanCloseOrEnqueue(the, controller))
		return;
	(*controller)->closeRequested = 1;
	if (GetQueueLength(the, (*controller)->queue) == 0) {
		ReadableStreamDefaultControllerClearAlgorithms(the, controller);
		ReadableStreamClose(the, (*controller)->stream);
	}
}
void ReadableStreamDefaultControllerEnqueue(xsMachine* the, ReadableStreamDefaultController* controller, xsSlot* chunk)
{
	if (!ReadableStreamDefaultControllerCanCloseOrEnqueue(the, controller))
		return;
	ReadableStream* stream = (*controller)->stream;
	if (IsReadableStreamLocked(the, stream) && (ReadableStreamGetNumReadRequests(the, stream) > 0))
		ReadableStreamFulfillReadRequest(the, stream, chunk, false);
	else {
		mxTry(the) {
			xsNumberValue chunkSize = 1;
			if ((*controller)->strategySizeAlgorithm) {
				mxPushUndefined();
				mxPushReference((*controller)->strategySizeAlgorithm);
				mxCall();
				mxPushSlot(chunk);
				mxRunCount(1);
				chunkSize = fxToNumber(the, the->stack);
				mxPop();
			}
			EnqueueValueSize(the, (*controller)->queue, chunk, chunkSize);
		}
		mxCatch(the) {
			ReadableStreamDefaultControllerError(the, controller, &mxException);
			fxJump(the);
		}
	}
	ReadableStreamDefaultControllerCallPullIfNeeded(the, controller);
}
void ReadableStreamDefaultControllerError(xsMachine* the, ReadableStreamDefaultController* controller, xsSlot* error)
{
	ReadableStream* stream = (*controller)->stream;
	if ((*stream)->state != mcStreamReadable)
		return;
	ResetQueue(the, (*controller)->queue);
	ReadableStreamDefaultControllerClearAlgorithms(the, controller);
	ReadableStreamError(the, stream, error);
}
txNumber ReadableStreamDefaultControllerGetDesiredSize(xsMachine* the, ReadableStreamDefaultController* controller)
{
	ReadableStream* stream = (*controller)->stream;
	if ((*stream)->state == mcStreamErrored)
		return C_NAN;
	if ((*stream)->state == mcStreamClosed)
		return 0;
	return (*controller)->strategyHWM - GetQueueTotalSize(the, (*controller)->queue);
}
txBoolean ReadableStreamDefaultControllerHasBackpressure(xsMachine* the, ReadableStreamDefaultController* controller)
{
	return (ReadableStreamDefaultControllerShouldCallPull(the, controller)) ? 0 : 1;
}
txBoolean ReadableStreamDefaultControllerShouldCallPull(xsMachine* the, ReadableStreamDefaultController* controller)
{
	ReadableStream* stream = (*controller)->stream;
	if (!ReadableStreamDefaultControllerCanCloseOrEnqueue(the, controller))
		return 0;
	if (!(*controller)->started)
		return 0;
	if (IsReadableStreamLocked(the, stream) && (ReadableStreamGetNumReadRequests(the, stream) > 0))
		return 1;
	txNumber desiredSize = ReadableStreamDefaultControllerGetDesiredSize(the, controller);
	return desiredSize > 0;
}
void ReadableStreamDefaultControllerStart(xsMachine* the, ReadableStreamDefaultController* controller)
{
	txSlot* result;
	mxTemporary(result);
	txBoolean success = (*((*controller)->startAlgorithm.call))(the, (StreamStuff*)controller, NULL, result);
	fxChainAlgorithm(the, result, success, (*controller)->startAlgorithm.resolved, (*controller)->startAlgorithm.rejected);
	mxPop(); // result
}

