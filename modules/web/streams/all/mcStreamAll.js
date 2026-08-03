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

import { AbortSignal, AbortController } from "web/abortsignal";

// 4.2 ReadableStream
function buildReadableStream(underlyingSource, strategy, closures, kind) { 
	return native("buildReadableStream").call(this, underlyingSource, strategy, closures, kind);
}
function buildReadableStreamFrom(iterable, closures) { 
	return native("buildReadableStreamFrom").call(this, iterable, closures);
}
class ReadableStream extends Native("ReadableStream_destructor") {
	constructor(underlyingSource, strategy) {
		if (underlyingSource === null)
			throw new TypeError("invalid underlyingSource");
		super();
		if (!underlyingSource || !("type" in underlyingSource) || (underlyingSource.type === undefined))
			buildReadableStream.call(this, underlyingSource, strategy, closures, true);
		else {
			if (String(underlyingSource.type) !== "bytes")
				throw new TypeError("invalid underlyingSource type");
			if (strategy && ("size" in strategy))
				throw new RangeError('The strategy for a byte stream cannot have a size function');
			buildReadableStream.call(this, underlyingSource, strategy, closures, false);
		}
	}
	get locked() { return native("ReadableStream_get_locked").call(this); }
	cancel(reason) { return native("ReadableStream_cancel").call(this, reason); }
	getReader(options) {
		if (!options || !("mode" in options) || (options.mode === undefined))
			return new ReadableStreamDefaultReader(this);
		if (String(options.mode) !== "byob")
			throw new TypeError("invalid mode");
		return new ReadableStreamBYOBReader(this);
	}
	pipeThrough(transform, options) { return native("ReadableStream_pipeThrough").call(this, transform, options); }
	pipeTo(destination, options) { return native("ReadableStream_pipeTo").call(this, destination, options); }
	tee() { return native("ReadableStream_tee").call(this); }
	values(options) {
		const preventCancel = Boolean(options?.preventCancel);
		return buildReadableStreamAsyncIterator.call(this, preventCancel);
	}
	[Symbol.asyncIterator]() {
		return buildReadableStreamAsyncIterator.call(this, false);
	}
	static from(iterable) { return buildReadableStreamFrom.call(this, iterable, closures); }
}

// 4.2.5 Asynchronous iteration
const readableStreamAsyncIteratorPrototype = Object.create(Object.getPrototypeOf(Object.getPrototypeOf(async function* () {}).prototype), {
	[Symbol.toStringTag]: {
		value: "ReadableStreamAsyncIterator",
		configurable: true
	}
});
Object.assign(readableStreamAsyncIteratorPrototype, {
	next() { return native("ReadableStreamAsyncIterator_next").call(this); },
	return(value) { return native("ReadableStreamAsyncIterator_return").call(this, value); },
});

function buildReadableStreamAsyncIterator(preventCancel) { 
	return native("buildReadableStreamAsyncIterator").call(this, preventCancel);
}

// 4.4 ReadableStreamDefaultReader
class ReadableStreamDefaultReader extends Native("ReadableStreamDefaultReader_destructor") {
	constructor(stream) { super(); native("ReadableStreamDefaultReader_constructor").call(this, stream); }
	get closed() { return native("ReadableStreamDefaultReader_get_closed").call(this); }
	cancel(reason) { return native("ReadableStreamDefaultReader_cancel").call(this, reason); }
	read() { return native("ReadableStreamDefaultReader_read").call(this); }
	releaseLock() { return native("ReadableStreamDefaultReader_releaseLock").call(this); }
}

// 4.5 ReadableStreamBYOBReader
class ReadableStreamBYOBReader extends Native("ReadableStreamBYOBReader_destructor") {
	constructor(stream) { super(); native("ReadableStreamBYOBReader_constructor").call(this, stream); }
	cancel(reason) { return native("ReadableStreamBYOBReader_cancel").call(this, reason); }
	get closed() { return native("ReadableStreamBYOBReader_get_closed").call(this); }
	read() { return native("ReadableStreamBYOBReader_read").call(this); }
	releaseLock() { return native("ReadableStreamBYOBReader_releaseLock").call(this); }
}

// 4.6 ReadableStreamDefaultController
class ReadableStreamDefaultController extends Native("ReadableStreamDefaultController_destructor") {
	constructor() {
		throw new TypeError("new ReadableStreamDefaultController");
	}
	get desiredSize() { return native("ReadableStreamDefaultController_get_desiredSize").call(this); }
	close() { return native("ReadableStreamDefaultController_close").call(this); }
	enqueue(chunk) { return native("ReadableStreamDefaultController_enqueue").call(this, chunk); }
	error(e) { return native("ReadableStreamDefaultController_error").call(this, e); }
}

// 4.7 ReadableByteStreamController
class ReadableByteStreamController extends Native("ReadableByteStreamController_destructor") {
	constructor() {
		throw new TypeError("new ReadableByteStreamController");
	}
	get byobRequest() { return native("ReadableByteStreamController_get_byobRequest").call(this); }
	get desiredSize() { return native("ReadableByteStreamController_get_desiredSize").call(this); }
	close() { return native("ReadableByteStreamController_close").call(this); }
	enqueue(chunk) { return native("ReadableByteStreamController_enqueue").call(this, chunk); }
	error(e) { return native("ReadableByteStreamController_error").call(this, e); }
}

// 4.8 ReadableStreamBYOBRequest 
class ReadableStreamBYOBRequest extends Native("ReadableStreamBYOBRequest_destructor") {
	constructor() {
		throw new TypeError("new ReadableStreamBYOBRequest");
	}
	get view() { return native("ReadableStreamBYOBRequest_get_view").call(this); }
	respond(bytesWritten) { return native("ReadableStreamBYOBRequest_respond").call(this, bytesWritten); }
	respondWithNewView(view) { return native("ReadableStreamBYOBRequest_respondWithNewView").call(this, view); }
}

// 5.2 WritableStream
function buildWritableStream(underlyingSink, strategy, closures) { 
	return native("buildWritableStream").call(this, underlyingSink, strategy, closures);
}
class WritableStream extends Native("WritableStream_destructor") {
	constructor(underlyingSink, strategy) {
		super();
		return buildWritableStream.call(this, underlyingSink, strategy, closures);
	}
	get locked() { return native("WritableStream_get_locked").call(this); }
	abort(reason) { return native("WritableStream_abort").call(this, reason); }
	close() { return native("WritableStream_close").call(this); }
	getWriter() {
		return new WritableStreamDefaultWriter(this);
	}
}

// 5.3 WritableStreamDefaultWriter
class WritableStreamDefaultWriter extends Native("WritableStreamDefaultWriter_destructor") {
	constructor(stream) { super(); native("WritableStreamDefaultWriter_constructor").call(this, stream); }
	get closed() { return native("WritableStreamDefaultWriter_get_closed").call(this); }
	get desiredSize() { return native("WritableStreamDefaultWriter_get_desiredSize").call(this); }
	get ready() { return native("WritableStreamDefaultWriter_get_ready").call(this); }
	abort(reason) { return native("WritableStreamDefaultWriter_abort").call(this, reason); }
	close() { return native("WritableStreamDefaultWriter_close").call(this); }
	releaseLock() { return native("WritableStreamDefaultWriter_releaseLock").call(this); }
	write(chunk) { return native("WritableStreamDefaultWriter_write").call(this, chunk); }
}

// 5.4 WritableStreamDefaultController
class WritableStreamDefaultController extends Native("WritableStreamDefaultController_destructor") {
	constructor() {
		throw new TypeError("new WritableStreamDefaultController");
	}
	get signal() { return native("WritableStreamDefaultController_get_signal").call(this); }
	error(e) { return native("WritableStreamDefaultController_error").call(this, e); }
}

// 6.2 TransformStream
function buildTransformStream(transformer, writableStrategy, readableStrategy, closures) { 
	return native("buildTransformStream").call(this, transformer, writableStrategy, readableStrategy, closures);
}
class TransformStream extends Native("TransformStream_destructor") {
	constructor(transformer, writableStrategy, readableStrategy) {
		super();
		if (transformer) {
			if ('readableType' in transformer) {
				throw new RangeError('Invalid readableType specified');
			}
			if ('writableType' in transformer) {
				throw new RangeError('Invalid writableType specified');
			}
		}
		return buildTransformStream.call(this, transformer, writableStrategy, readableStrategy, closures);
	}
	get readable() { return native("TransformStream_get_readable").call(this); }
	get writable() { return native("TransformStream_get_writable").call(this); }
}

// 6.3 TransformStreamDefaultController
class TransformStreamDefaultController extends Native("TransformStreamDefaultController_destructor") {
	constructor(token) {
		throw new TypeError("new TransformStreamDefaultController");
	}
	get desiredSize() { return native("TransformStreamDefaultController_get_desiredSize").call(this); }
	enqueue(chunk) { return native("TransformStreamDefaultController_enqueue").call(this, chunk); }
	error(reason) { return native("TransformStreamDefaultController_error").call(this, reason); }
	terminate() { return native("TransformStreamDefaultController_terminate").call(this); }
}

// 7.2 ByteLengthQueuingStrategy
class ByteLengthQueuingStrategy {
	#highWaterMark;
	constructor(init) {
		const highWaterMark = init?.highWaterMark;
		if (highWaterMark === undefined)
			throw new TypeError("invalid highWaterMark");
		this.#highWaterMark = Number(highWaterMark);
	}
	get highWaterMark() {
		return this.#highWaterMark;
	}
	size(chunk) {
		return chunk.byteLength;
	}
}

// 7.3 CountQueuingStrategy
class CountQueuingStrategy {
	#highWaterMark;
	constructor(init) {
		const highWaterMark = init?.highWaterMark;
		if (highWaterMark === undefined)
			throw new TypeError("invalid highWaterMark");
		this.#highWaterMark = Number(highWaterMark);
	}
	get highWaterMark() {
		return this.#highWaterMark;
	}
	size() {
		return 1;
	}
}

const closures = Object.freeze({
	readableStream: ReadableStream.prototype,
	readableStreamAsyncIteratorPrototype,
	ReadableStreamDefaultReader,
	ReadableStreamBYOBReader,
	readableStreamDefaultController: ReadableStreamDefaultController.prototype,
	readableByteStreamController: ReadableByteStreamController.prototype,
	readableStreamBYOBRequest: ReadableStreamBYOBRequest.prototype,
	writableStream: WritableStream.prototype,
	WritableStreamDefaultWriter,
	writableStreamDefaultController: WritableStreamDefaultController.prototype,
	transformStreamDefaultController: TransformStreamDefaultController.prototype,
	AbortController,
	AbortSignal,
	ignore(value) { 
// 		trace(`ignore ${ value }\n`);
	},
}, true);

export { 
	ReadableStream,
	ReadableStreamDefaultReader,
	ReadableStreamBYOBReader,
	ReadableStreamDefaultController,
	ReadableByteStreamController,
	ReadableStreamBYOBRequest,
	WritableStream,
	WritableStreamDefaultWriter,
	WritableStreamDefaultController,
	TransformStream,
	TransformStreamDefaultController,
	ByteLengthQueuingStrategy,
	CountQueuingStrategy,
} 
