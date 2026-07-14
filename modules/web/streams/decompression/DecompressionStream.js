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
 */

import { ReadableStream, WritableStream } from "web/streams";
import Inflate from "inflate";

// gzip FLG bits
const FHCRC    = 0x02;
const FEXTRA   = 0x04;
const FNAME    = 0x08;
const FCOMMENT = 0x10;

class GzipHeaderParser {
	#state = "fixed";	// fixed -> [extra_len -> extra] -> [fname] -> [fcomment] -> [fhcrc] -> done
	#buffer = new Uint8Array(0);
	#flags = 0;
	#extraRemaining = 0;

	get done() { return this.#state === "done"; }

	push(chunk) {
		if (this.#buffer.byteLength) {
			const combined = new Uint8Array(this.#buffer.byteLength + chunk.byteLength);
			combined.set(this.#buffer, 0);
			combined.set(chunk, this.#buffer.byteLength);
			this.#buffer = combined;
		}
		else
			this.#buffer = chunk;
		return this.#parse();
	}

	#nextStateAfter(stage) {
		const f = this.#flags;
		if (stage <= 1 && (f & FEXTRA))   return "extra_len";
		if (stage <= 2 && (f & FNAME))    return "fname";
		if (stage <= 3 && (f & FCOMMENT)) return "fcomment";
		if (stage <= 4 && (f & FHCRC))    return "fhcrc";
		return "done";
	}

	#parse() {
		const buf = this.#buffer;
		let i = 0;

		while (this.#state !== "done") {
			if (this.#state === "fixed") {
				if (buf.byteLength - i < 10) break;
				if ((buf[i] !== 0x1f) || (buf[i + 1] !== 0x8b))
					throw new TypeError("invalid gzip magic");
				if (buf[i + 2] !== 0x08)
					throw new TypeError("unsupported gzip compression method");
				this.#flags = buf[i + 3];
				i += 10;
				this.#state = this.#nextStateAfter(1);
			}
			else if (this.#state === "extra_len") {
				if (buf.byteLength - i < 2) break;
				this.#extraRemaining = buf[i] | (buf[i + 1] << 8);
				i += 2;
				this.#state = "extra";
			}
			else if (this.#state === "extra") {
				const consume = Math.min(this.#extraRemaining, buf.byteLength - i);
				i += consume;
				this.#extraRemaining -= consume;
				if (this.#extraRemaining > 0) break;
				this.#state = this.#nextStateAfter(2);
			}
			else if (this.#state === "fname") {
				while ((i < buf.byteLength) && (buf[i] !== 0)) i++;
				if (i >= buf.byteLength) break;
				i++;
				this.#state = this.#nextStateAfter(3);
			}
			else if (this.#state === "fcomment") {
				while ((i < buf.byteLength) && (buf[i] !== 0)) i++;
				if (i >= buf.byteLength) break;
				i++;
				this.#state = this.#nextStateAfter(4);
			}
			else if (this.#state === "fhcrc") {
				if (buf.byteLength - i < 2) break;
				i += 2;
				this.#state = "done";
			}
		}

		if (this.#state === "done") {
			const remainder = buf.subarray(i);
			this.#buffer = new Uint8Array(0);
			return remainder;
		}
		this.#buffer = buf.subarray(i);
		return null;
	}
}

const kDefaultBufferSize = 2048;

class DecompressionStream {
	#readable;
	#writable;
	#format;

	constructor(format) {
		format = String(format);
		if (!["deflate", "deflate-raw", "gzip"].includes(format))
			throw new TypeError(`unsupported format: ${format}`);
		this.#format = format;

		const gzipParser = ("gzip" === format) ? new GzipHeaderParser() : null;
		let inflate = null;
		const pendingQueue = [];
		let pendingBytes = 0;
		let ended = false;
		let gzipTrailerRemaining = (format === "gzip") ? 8 : 0;
		let writeClosed = false;
		let readableController = null;
		let lastProduced = 0;

		// Rendezvous between writable.write() and readable.pull().
		// pull() awaits inputAvailable; write() / close() wake it.
		// write() awaits inputConsumed; pull() wakes it once it has drained pending
		// (or determined more input is required).
		let resolveInputAvailable = null;
		let resolveInputConsumed = null;
		const waitForInput    = () => new Promise(r => resolveInputAvailable = r);
		const waitForConsumed = () => new Promise(r => resolveInputConsumed = r);
		const wakeInput    = () => { const r = resolveInputAvailable; resolveInputAvailable = null; r?.(); };
		const wakeConsumed = () => { const r = resolveInputConsumed; resolveInputConsumed = null; r?.(); };

		const ensureInflate = () => {
			if (inflate)
				return;
			inflate = new Inflate({
				windowBits: ("deflate" === format) ? +15 : -15
			});
			inflate.onData = function(view) {
				lastProduced = view.byteLength;
				readableController.enqueue(view);
			};
			inflate.onEnd = function(err) {
				ended = true;
				if (err)
					readableController.error(new TypeError("decompression failed"));
			};
		};

		this.#writable = new WritableStream({
			async write(chunk) {
				try {
					// Normalize chunk to Uint8Array
					if (ArrayBuffer.isView(chunk)) {
						if (!(chunk instanceof Uint8Array))
							chunk = new Uint8Array(chunk.buffer, chunk.byteOffset, chunk.byteLength);
					}
					else if ((chunk instanceof ArrayBuffer) || (chunk instanceof SharedArrayBuffer))
						chunk = new Uint8Array(chunk);
					else
						throw new TypeError("invalid chunk");
					if (chunk.byteLength === 0)
						return;
	
					// Post-end: accept trailing gzip trailer bytes silently; error on extra data.
					if (ended) {
						if (gzipTrailerRemaining > 0) {
							const consume = Math.min(gzipTrailerRemaining, chunk.byteLength);
							gzipTrailerRemaining -= consume;
							if (consume === chunk.byteLength)
								return;
						}
						const e = new TypeError("data after end of compressed stream");
// 						readableController.error(e);
						throw e;
					}
	
					if (gzipParser && !gzipParser.done) {
						let payload;
						try {
							payload = gzipParser.push(chunk);
						}
						catch (e) {
// 							readableController?.error(e);
							throw e;
						}
						if (!payload || (payload.byteLength === 0))
							return;
						chunk = payload;
					}
	
					// Append the chunk view to the queue — no copy.
					pendingQueue.push(chunk);
					pendingBytes += chunk.byteLength;
					wakeInput();
					await waitForConsumed();
				}
				catch (e) {
					readableController?.error(e);
					throw e;
				}
			},
			close() {
				writeClosed = true;
				wakeInput();
			},
			abort(reason) {
				writeClosed = true;
				readableController?.error(reason);
				wakeInput();
				wakeConsumed();
			}
		}, { highWaterMark: 1, size: c => c?.byteLength ?? 1 });	// per the Compression Streams spec

		this.#readable = new ReadableStream({
			start(controller) {
				readableController = controller;
			},
			async pull(controller) {
				while (true) {
					while (pendingQueue.length === 0 && !writeClosed && !ended)
						await waitForInput();

					if (ended) {
						controller.close();
						wakeConsumed();
						return;
					}

					if (pendingQueue.length === 0 && writeClosed) {
						controller.error(new TypeError("incomplete compressed stream"));
						wakeConsumed();
						return;
					}

					ensureInflate();

					// Per the Compression Streams spec our readable uses a default strategy
					// (size = 1, HWM = 0), so desiredSize is in chunks not bytes — use a fixed
					// internal chunk size for inflate output.
					const buffer = new Uint8Array(kDefaultBufferSize);

					lastProduced = 0;
					let madeProgress = false;
					while ((pendingQueue.length > 0) && !ended && (lastProduced === 0)) {
						const head = pendingQueue[0];
						const beforeLen = head.byteLength;
						try {
							inflate.push(head, undefined, buffer);
						}
						catch (e) {
							controller.error(new TypeError("decompression failed: " + e.message));
							wakeConsumed();
							return;
						}
						const consumed = beforeLen - inflate.strm.avail_in;
						pendingBytes -= consumed;
						if (consumed === head.byteLength)
							pendingQueue.shift();		// fully consumed
						else if (consumed > 0)
							pendingQueue[0] = head.subarray(consumed);	// partially consumed — view, no copy
						else
							break;		// stalled on this chunk — wait for more input
						madeProgress = true;
					}

					if (ended) {
						// Leftover queued bytes are the leading gzip trailer that inflate didn't
						// read; subtract from the remaining-trailer budget. (avail_in on the final
						// push is the head buffer's leftover; whole-queue leftover is pendingBytes.)
						if (gzipTrailerRemaining > 0)
							gzipTrailerRemaining = Math.max(0, gzipTrailerRemaining - pendingBytes);
					}

					// Wake write() if the queue drained, if inflate stalled (needs more input), or
					// if the stream has ended (any leftover queued bytes are gzip trailer).
					if ((pendingQueue.length === 0) || !madeProgress || ended)
						wakeConsumed();

					if (ended) {
						controller.close();
						return;
					}

					if (lastProduced > 0)
						return;
				}
			},
			cancel(reason) {
				writeClosed = true;
				wakeInput();
				wakeConsumed();
			}
		});
	}
	get readable() { return this.#readable; }
	get writable() { return this.#writable; }
	get format()   { return this.#format; }
}

export default DecompressionStream;
