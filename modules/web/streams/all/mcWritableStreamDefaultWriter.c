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

static void WritableStreamDefaultWriterAbort(xsMachine* the, WritableStreamDefaultWriter* writer, xsSlot* reason, xsSlot* promise);
static void WritableStreamDefaultWriterClose(xsMachine* the, WritableStreamDefaultWriter* writer, xsSlot* promise);
static void WritableStreamDefaultWriterEnsureClosedPromiseRejected(xsMachine* the, WritableStreamDefaultWriter* writer, xsSlot* error);

static const StreamDispatchRecord WritableStreamDefaultWriterDispatchRecord = {
	"WritableStreamDefaultWriter",
};

static void WritableStreamDefaultWriter_mark(xsMachine* the, void* it, xsMarkRoot markRoot);
static const xsHostHooks WritableStreamDefaultWriterHooks ICACHE_RODATA_ATTR = {
	WritableStreamDefaultWriter_destructor,
	WritableStreamDefaultWriter_mark,
	NULL
};

// 5.3 WritableStreamDefaultWriter
WritableStreamDefaultWriter* AcquireWritableStreamDefaultWriter(xsMachine* the, WritableStream* stream)
{
	mxPushReference((*stream)->closures);
	mxGetID(xsID_WritableStreamDefaultWriter);
	mxNew();
	mxPushReference((*stream)->reference);
	mxRunCount(1);
	mxPop();
	return (WritableStreamDefaultWriter*)((*stream)->writer);
}
void WritableStreamDefaultWriter_constructor(xsMachine* the)
{
	WritableStream* stream = NULL;
	if (mxArgc > 0)
		stream = mxStreamHandle(WritableStream, mxArgv(0));
	else
		xsTypeError("no stream");
	if (IsWritableStreamLocked(the, stream))
		xsTypeError("stream locked");
	
	xsSetHostChunk(xsThis, NULL, sizeof(WritableStreamDefaultWriterRecord));
	xsSetHostHooks(xsThis, (xsHostHooks*)&WritableStreamDefaultWriterHooks);
	WritableStreamDefaultWriter* writer = (WritableStreamDefaultWriter*)fxGetHostHandle(the, mxThis);
	(*writer)->reference = xsToReference(xsThis);
	(*writer)->dispatch = (StreamDispatch)&WritableStreamDefaultWriterDispatchRecord;
	
	(*writer)->stream = stream;
	(*stream)->writer = writer;
	
	switch((*stream)->state) {
	case mcStreamWritable:
		if (!WritableStreamCloseQueuedOrInFlight(the, stream) && (*stream)->backpressure)
			(*writer)->readyPromise = fxCreatePromiseRecord(the, NULL);
		else
			(*writer)->readyPromise = fxCreateResolvedPromiseRecord(the, &mxUndefined, NULL);
		(*writer)->closedPromise = fxCreatePromiseRecord(the, NULL);
		break;
	case mcStreamClosed:
		(*writer)->readyPromise = fxCreateResolvedPromiseRecord(the, &mxUndefined, NULL);
		(*writer)->closedPromise = fxCreateResolvedPromiseRecord(the, &mxUndefined, NULL);
		break;
	case mcStreamErrored:
		(*writer)->readyPromise = fxCreateRejectedPromiseRecord(the, (*stream)->storedError, NULL);
		fxHandlePromiseRecord(the, (*writer)->readyPromise, (*stream)->closures);
		(*writer)->closedPromise = fxCreateRejectedPromiseRecord(the, (*stream)->storedError, NULL);
		fxHandlePromiseRecord(the, (*writer)->closedPromise, (*stream)->closures);
		break;
	case mcStreamErroring:
		(*writer)->readyPromise = fxCreateRejectedPromiseRecord(the, (*stream)->storedError, NULL);
		fxHandlePromiseRecord(the, (*writer)->readyPromise, (*stream)->closures);
		(*writer)->closedPromise = fxCreatePromiseRecord(the, NULL);
		break;
	}
}
void WritableStreamDefaultWriter_destructor(void* it)
{
}
void WritableStreamDefaultWriter_mark(xsMachine* the, void* it, xsMarkRoot markRoot)
{
	WritableStreamDefaultWriter self = it;
	StreamMarkHandle(the, self->stream);
	StreamMarkReference(the, self->closedPromise);
	StreamMarkReference(the, self->readyPromise);
}
void WritableStreamDefaultWriter_get_closed(xsMachine* the)
{
	WritableStreamDefaultWriter* writer = mxStreamHandle(WritableStreamDefaultWriter, mxThis);
	fxGetPromiseRecordPromise(the, (*writer)->closedPromise, mxResult);
}
void WritableStreamDefaultWriter_get_desiredSize(xsMachine* the)
{
	WritableStreamDefaultWriter* writer = mxStreamHandle(WritableStreamDefaultWriter, mxThis);
	if (!(*writer)->stream) {
		xsTypeError("no stream");
	}
	txNumber size = WritableStreamDefaultWriterGetDesiredSize(the, writer);
	if (c_isnan(size))
		xsResult = xsNull;
	else
		xsResult = xsNumber(size);
}
void WritableStreamDefaultWriter_get_ready(xsMachine* the)
{
	WritableStreamDefaultWriter* writer = mxStreamHandle(WritableStreamDefaultWriter, mxThis);
	fxGetPromiseRecordPromise(the, (*writer)->readyPromise, mxResult);
}
void WritableStreamDefaultWriter_abort(xsMachine* the)
{
	WritableStreamDefaultWriter* writer = mxStreamHandle(WritableStreamDefaultWriter, mxThis);
	if (!(*writer)->stream) {
		mxReturnPromiseRejectedWithTypeError("no stream");
	}
	if (mxArgc > 0)
		mxPushSlot(mxArgv(0));
	else
		mxPushUndefined();
	WritableStreamDefaultWriterAbort(the, writer, the->stack, mxResult);
	mxPop();
}
void WritableStreamDefaultWriter_close(xsMachine* the)
{
	WritableStreamDefaultWriter* writer = mxStreamHandle(WritableStreamDefaultWriter, mxThis);
	if (!(*writer)->stream) {
		mxReturnPromiseRejectedWithTypeError("no stream");
	}
	if (WritableStreamCloseQueuedOrInFlight(the, (*writer)->stream)) {
		mxReturnPromiseRejectedWithTypeError("stream closed");
	}
	WritableStreamDefaultWriterClose(the, writer, mxResult);
}
void WritableStreamDefaultWriter_releaseLock(xsMachine* the)
{
	WritableStreamDefaultWriter* writer = mxStreamHandle(WritableStreamDefaultWriter, mxThis);
	if (!(*writer)->stream)
		return;
	WritableStreamDefaultWriterRelease(the, writer);
}
void WritableStreamDefaultWriter_write(xsMachine* the)
{
	WritableStreamDefaultWriter* writer = mxStreamHandle(WritableStreamDefaultWriter, mxThis);
	if (!(*writer)->stream) {
		mxReturnPromiseRejectedWithTypeError("no stream");
	}
	if (mxArgc > 0)
		mxPushSlot(mxArgv(0));
	else
		mxPushUndefined();
	WritableStreamDefaultWriterWrite(the, writer, the->stack, mxResult);
	mxPop();
}

// 5.5.3 Writers
void WritableStreamDefaultWriterAbort(xsMachine* the, WritableStreamDefaultWriter* writer, xsSlot* reason, xsSlot* promise)
{
	WritableStream* stream = (*writer)->stream;
	mxStreamAssert(stream != NULL);
	WritableStreamAbort(the, stream, reason, promise);
}
void WritableStreamDefaultWriterClose(xsMachine* the, WritableStreamDefaultWriter* writer, xsSlot* promise)
{
	WritableStream* stream = (*writer)->stream;
	mxStreamAssert(stream != NULL);
	WritableStreamClose(the, stream, promise);
}
void WritableStreamDefaultWriterCloseWithErrorPropagation(xsMachine* the, WritableStreamDefaultWriter* writer, xsSlot* promise)
{
	WritableStream* stream = (*writer)->stream;
	mxStreamAssert(stream != NULL);
	uint8_t state = (*stream)->state;
	if (WritableStreamCloseQueuedOrInFlight(the, stream) || (state == mcStreamClosed)) {
		fxCreateResolvedPromise(the, &mxUndefined, promise);
		return;
	}
	if (state == mcStreamErrored) {
		fxCreateRejectedPromise(the, (*stream)->storedError, promise);
		return;
	}
	mxStreamAssert((state == mcStreamWritable) || (state == mcStreamErroring));
	WritableStreamDefaultWriterClose(the, writer, promise);
}
void WritableStreamDefaultWriterEnsureClosedPromiseRejected(xsMachine* the, WritableStreamDefaultWriter* writer, xsSlot* error)
{
	WritableStream* stream = (*writer)->stream;
	if (fxIsPromiseRecordPending(the, (*writer)->closedPromise))
		fxRejectPromiseRecord(the, (*writer)->closedPromise, error);
	else
		(*writer)->closedPromise = fxCreateRejectedPromiseRecord(the, error, NULL);
	fxHandlePromiseRecord(the, (*writer)->closedPromise, (*stream)->closures);
}
void WritableStreamDefaultWriterEnsureReadyPromiseRejected(xsMachine* the, WritableStreamDefaultWriter* writer, xsSlot* error)
{
	WritableStream* stream = (*writer)->stream;
	if (fxIsPromiseRecordPending(the, (*writer)->readyPromise))
		fxRejectPromiseRecord(the, (*writer)->readyPromise, error);
	else
		(*writer)->readyPromise = fxCreateRejectedPromiseRecord(the, error, NULL);
	fxHandlePromiseRecord(the, (*writer)->readyPromise, (*stream)->closures);
}
txNumber WritableStreamDefaultWriterGetDesiredSize(xsMachine* the, WritableStreamDefaultWriter* writer)
{
	WritableStream* stream = (*writer)->stream;
	mxStreamAssert(stream != NULL);
	uint8_t state = (*stream)->state;
	if ((state == mcStreamErrored) || (state == mcStreamErroring))
		return C_NAN;
	if (state == mcStreamClosed)
		return 0;
	return WritableStreamDefaultControllerGetDesiredSize(the, (*stream)->controller);
}
void WritableStreamDefaultWriterRelease(xsMachine* the, WritableStreamDefaultWriter* writer)
{
	WritableStream* stream = (*writer)->stream;
	mxStreamAssert(stream != NULL);
	mxStreamAssert((*stream)->writer == writer);
	mxPush(mxTypeErrorConstructor);
	fxNewError(the, "writer released");
	WritableStreamDefaultWriterEnsureReadyPromiseRejected(the, writer, the->stack);
	WritableStreamDefaultWriterEnsureClosedPromiseRejected(the, writer, the->stack);
	mxPop();
	(*stream)->writer = NULL;
	(*writer)->stream = NULL;
}
void WritableStreamDefaultWriterWrite(xsMachine* the, WritableStreamDefaultWriter* writer, xsSlot* chunk, xsSlot* promise)
{
	WritableStream* stream = (*writer)->stream;
	mxStreamAssert(stream != NULL);
	WritableStreamDefaultController* controller = (*stream)->controller;
	txNumber chunkSize = WritableStreamDefaultControllerGetChunkSize(the, controller, chunk);
	if (stream != (*writer)->stream) {
		mxPush(mxTypeErrorConstructor);
		fxNewError(the, "stream writer mismatch");
		fxCreateRejectedPromise(the, the->stack, promise);
		mxPop();
		return;
	}
	uint8_t state = (*stream)->state;
	if (state == mcStreamErrored) {
		fxCreateRejectedPromise(the, (*stream)->storedError, promise);
		return;
	}
	if (WritableStreamCloseQueuedOrInFlight(the, stream) || (state == mcStreamClosed)) {
		mxPush(mxTypeErrorConstructor);
		fxNewError(the, "stream closing or closed");
		fxCreateRejectedPromise(the, the->stack, promise);
		mxPop();
		return;
	}
	if (state == mcStreamErroring) {
		fxCreateRejectedPromise(the, (*stream)->storedError, promise);
		return;
	}
	mxStreamAssert(state == mcStreamWritable);
	txSlot* request = WritableStreamAddWriteRequest(the, stream);
	WritableStreamDefaultControllerWrite(the, controller, chunk, chunkSize);
	fxGetPromiseRecordPromise(the, request, promise);
}








