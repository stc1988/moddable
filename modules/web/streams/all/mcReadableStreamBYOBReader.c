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

static void ReadableStreamBYOBReaderCancel(xsMachine* the, ReadableStreamReader* it);
static void ReadableStreamBYOBReaderClose(xsMachine* the, ReadableStreamReader* it);
static void ReadableStreamBYOBReaderError(xsMachine* the, ReadableStreamReader* it, xsSlot* e);

static const ReadableStreamReaderDispatchRecord ReadableStreamBYOBReaderDispatchRecord = {
	"ReadableStreamBYOBReader",
	ReadableStreamBYOBReaderCancel,
	ReadableStreamBYOBReaderClose,
	ReadableStreamBYOBReaderError,
};

// 4.4 ReadableStreamBYOBReader

static void ReadableStreamBYOBReader_mark(xsMachine* the, void* it, xsMarkRoot markRoot);
static const xsHostHooks ReadableStreamBYOBReaderHooks ICACHE_RODATA_ATTR = {
	ReadableStreamBYOBReader_destructor,
	ReadableStreamBYOBReader_mark,
	NULL
};

void ReadableStreamBYOBReader_constructor(xsMachine* the)
{
	ReadableStream* stream = NULL;
	if (mxArgc > 0)
		stream = mxStreamHandle(ReadableStream, mxArgv(0));
	else
		xsTypeError("no stream");
	if (IsReadableStreamLocked(the, stream))
		xsTypeError("stream locked");
	if (!IsReadableByteStreamController(the, (*stream)->controller))
		xsTypeError("invalid controller");
	
	xsSetHostChunk(xsThis, NULL, sizeof(ReadableStreamBYOBReaderRecord));
	xsSetHostHooks(xsThis, (xsHostHooks*)&ReadableStreamBYOBReaderHooks);
	ReadableStreamBYOBReader* reader = (ReadableStreamBYOBReader*)fxGetHostHandle(the, mxThis);
	(*reader)->reference = xsToReference(xsThis);
	(*reader)->dispatch = (ReadableStreamReaderDispatch)&ReadableStreamBYOBReaderDispatchRecord;
	
	(*reader)->stream = stream;
	(*stream)->reader = (ReadableStreamReader*)reader;
	ReadableStreamReaderGenericInitialize(the, (ReadableStreamReader*)reader);
	
	(*reader)->queue = CreateSlotQueue(the);
}

void ReadableStreamBYOBReader_destructor(void* it)
{
}

void ReadableStreamBYOBReader_mark(xsMachine* the, void* it, xsMarkRoot markRoot)
{
	ReadableStreamBYOBReader self = it;
	StreamMarkHandle(the, self->stream);
	StreamMarkReference(the, self->closedPromise);
	StreamMarkReference(the, self->queue);
	StreamMarkHandle(the, self->context);
}

void ReadableStreamBYOBReader_get_closed(xsMachine* the)
{
	ReadableStreamBYOBReader* reader = mxStreamHandle(ReadableStreamBYOBReader, mxThis);
	fxGetPromiseRecordPromise(the, (*reader)->closedPromise, mxResult);
}

void ReadableStreamBYOBReader_cancel(xsMachine* the)
{
	ReadableStreamBYOBReader* reader = mxStreamHandle(ReadableStreamBYOBReader, mxThis);
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

void ReadableStreamBYOBReader_read(xsMachine* the)
{
	ReadableStreamBYOBReader* reader = mxStreamHandle(ReadableStreamBYOBReader, mxThis);
	xsSlot* view;
	txInteger min = 1;
	xsVars(1);
	{
		mxTry(the) {
			if (mxArgc > 0)
				mxPushSlot(mxArgv(0));
			else
				mxPushUndefined();
			view = the->stack;
			ViewInfoRecord info;
			GetViewInfo(the, view, &info);
			if (info.byteLength == 0)
				xsTypeError("view.byteLength == 0");
			if (info.bufferByteLength == 0)
				xsTypeError("view.buffer.byteLength == 0");
			if (mxArgc > 1) {
				xsSlot* options = mxArgv(1);
				mxPushSlot(options);
				if (fxRunTest(the)) {
					mxPushSlot(options);
					if (mxHasID(xsID_min)) {
						mxPushSlot(options);
						mxGetID(xsID_min);
						min = fxToInteger(the, the->stack);
						mxPop();
					}
				}
			}
			if (min <= 0)
				xsTypeError("options.min <= 0");
			if (min > (info.byteLength / info.elementSize))
				xsRangeError("options.min > view.length");
		}
		mxCatch(the) {
			fxCreateRejectedPromise(the, &mxException, mxResult);				
			mxException = xsUndefined;
			return;
		}
	}
	(*reader)->chunkSteps = ReadableStreamReaderChunkSteps;
	(*reader)->closeSteps = ReadableStreamReaderCloseSteps;
	(*reader)->errorSteps = ReadableStreamReaderErrorSteps;
	xsSlot* record = fxCreatePromiseRecord(the, mxResult);
	ReadableStreamBYOBReaderRead(the, reader, view, min, mxResult);
	fxGetPromiseRecordPromise(the, record, mxResult);
}

void ReadableStreamBYOBReader_releaseLock(xsMachine* the)
{
	ReadableStreamBYOBReader* reader = mxStreamHandle(ReadableStreamBYOBReader, mxThis);
	if (!(*reader)->stream)
		return;
	ReadableStreamReaderGenericRelease(the, (ReadableStreamReader*)reader);	
}

ReadableStreamReader* AcquireReadableStreamBYOBReader(xsMachine* the, ReadableStream* stream)
{
	mxPushReference((*stream)->closures);
	mxGetID(xsID_ReadableStreamBYOBReader);
	mxNew();
	mxPushReference((*stream)->reference);
	mxRunCount(1);
	mxPop();
	return (ReadableStreamReader*)((*stream)->reader);
}
txBoolean IsReadableStreamBYOBReader(xsMachine* the, ReadableStreamReader* reader)
{
	return ((*reader)->dispatch == &ReadableStreamBYOBReaderDispatchRecord) ? 1 : 0;
}
void ReadableStreamBYOBReaderCancel(xsMachine* the, ReadableStreamReader* reader)
{
	xsSlot* request;
	mxTemporary(request);
	while (GetSlotQueueLength(the, (*reader)->queue) > 0) {
		DequeueSlot(the, (*reader)->queue, request);
		(*(*reader)->closeSteps)(the, reader, request, NULL);
	}
	mxPop();
}
void ReadableStreamBYOBReaderClose(xsMachine* the, ReadableStreamReader* reader)
{
}
void ReadableStreamBYOBReaderError(xsMachine* the, ReadableStreamReader* reader, xsSlot* error)
{
	xsSlot* request;
	mxTemporary(request);
	while (GetSlotQueueLength(the, (*reader)->queue) > 0) {
		DequeueSlot(the, (*reader)->queue, request);
		(*(*reader)->errorSteps)(the, reader, request, error);
	}
	mxPop();
}
void ReadableStreamBYOBReaderRead(xsMachine* the, ReadableStreamBYOBReader* reader, xsSlot* view, txInteger min, xsSlot* request)
{
	ReadableStream* stream = (*reader)->stream;
	(*stream)->disturbed = 1;
	if ((*stream)->state == mcStreamErrored) {
		mxPushSlot((*stream)->storedError);
		(*(*reader)->errorSteps)(the, (ReadableStreamReader*)reader, request, the->stack);
		mxPop();
	}
	else {
		ReadableByteStreamControllerPullInto(the, (ReadableByteStreamController*)((*stream)->controller), view, min, request);
	}
}
