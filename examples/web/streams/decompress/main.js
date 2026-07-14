/*
 * Copyright (c) 2026  Moddable Tech, Inc.
 *
 *   This file is part of the Moddable SDK.
 *
 *   This work is licensed under the
 *       Creative Commons Attribution 4.0 International License.
 *   To view a copy of this license, visit
 *       <http://creativecommons.org/licenses/by/4.0>.
 *   or send a letter to Creative Commons, PO Box 1866,
 *   Mountain View, CA 94042, USA.
 *
 */

import * as streams from "web/streams";
for (let key in streams)
	globalThis[key] = streams[key];

import DecompressionStream from "web/decompressionstream";
import TextDecoderStream from "web/textdecoderstream";

const ReadableStream = globalThis.ReadableStream;

const expected = "The quick brown fox jumps over the lazy dog.\n";

// "The quick brown fox jumps over the lazy dog.\n" in three formats (produced via Node zlib)
const gzipBytes = new Uint8Array([
	0x1f,0x8b,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x13,0x0b,0xc9,0x48,0x55,0x28,0x2c,
	0xcd,0x4c,0xce,0x56,0x48,0x2a,0xca,0x2f,0xcf,0x53,0x48,0xcb,0xaf,0x50,0xc8,0x2a,
	0xcd,0x2d,0x28,0x56,0xc8,0x2f,0x4b,0x2d,0x52,0x28,0x01,0x4a,0xe7,0x24,0x56,0x55,
	0x2a,0xa4,0xe4,0xa7,0xeb,0x71,0x01,0x00,0x6a,0xcc,0x50,0xeb,0x2d,0x00,0x00,0x00
]);

const deflateBytes = new Uint8Array([
	0x78,0x9c,0x0b,0xc9,0x48,0x55,0x28,0x2c,0xcd,0x4c,0xce,0x56,0x48,0x2a,0xca,0x2f,
	0xcf,0x53,0x48,0xcb,0xaf,0x50,0xc8,0x2a,0xcd,0x2d,0x28,0x56,0xc8,0x2f,0x4b,0x2d,
	0x52,0x28,0x01,0x4a,0xe7,0x24,0x56,0x55,0x2a,0xa4,0xe4,0xa7,0xeb,0x71,0x01,0x00,
	0x7b,0xf6,0x10,0x12
]);

const deflateRawBytes = new Uint8Array([
	0x0b,0xc9,0x48,0x55,0x28,0x2c,0xcd,0x4c,0xce,0x56,0x48,0x2a,0xca,0x2f,0xcf,0x53,
	0x48,0xcb,0xaf,0x50,0xc8,0x2a,0xcd,0x2d,0x28,0x56,0xc8,0x2f,0x4b,0x2d,0x52,0x28,
	0x01,0x4a,0xe7,0x24,0x56,0x55,0x2a,0xa4,0xe4,0xa7,0xeb,0x71,0x01,0x00
]);

// >10KB plaintext gzip fixture — decompresses to ~12KB so the 2KB scratch in
// DecompressionStream rolls over multiple times, exercising the bounded-output loop.
const largeExpected = (() => {
	const line = "The quick brown fox jumps over the lazy dog. Pack my box with five dozen liquor jugs.\n";
	const body = line.repeat(Math.ceil(10000 / line.length));
	let tail = "";
	for (let i = 0; i < 2048; i++)
		tail += String.fromCharCode(33 + ((i * 31 + 7) % 94));
	return body + tail;
})();
const largeGzipBytes = new Uint8Array([
	0x1f,0x8b,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x13,0xed,0xcc,0xd7,0x56,0x83,0x40,
	0x00,0x45,0xd1,0x77,0xbf,0x62,0xec,0x1d,0x7b,0xef,0xbd,0x6b,0x54,0xec,0x1a,0x85,
	0x84,0x32,0x09,0x30,0xa1,0x07,0x2c,0xdf,0x2e,0x9f,0xa1,0x6b,0x9d,0xe7,0x7b,0xee,
	0xd6,0x5d,0x4b,0x84,0xa9,0x6c,0xb4,0x85,0x19,0xa9,0x3c,0x10,0xb6,0xea,0x8a,0x56,
	0xea,0x77,0x62,0xa1,0x32,0x2b,0x12,0x49,0x35,0x7b,0x46,0x59,0x88,0xa6,0x72,0x34,
	0x51,0x33,0xaa,0xce,0x2f,0x84,0x59,0x45,0xb9,0x4c,0x5c,0x61,0xcb,0xcc,0xaa,0xa6,
	0xd2,0x0a,0x84,0x27,0xc3,0x54,0x45,0xd5,0xd7,0x89,0xb5,0x1e,0x1d,0x15,0x15,0x15,
	0x15,0x15,0x15,0x15,0x15,0x15,0x15,0x15,0x15,0x15,0x15,0x15,0x15,0x15,0x15,0x15,
	0x15,0x15,0x15,0x15,0x15,0x15,0x15,0x15,0x15,0x15,0x15,0x15,0x15,0x15,0x15,0x15,
	0x15,0x15,0xf5,0x1f,0xa9,0x23,0x47,0xf6,0xf0,0xa1,0x35,0x74,0xd0,0x1c,0xdc,0x6f,
	0x0c,0xec,0x99,0xfd,0xbb,0x46,0xdf,0xce,0x47,0xef,0xf6,0xfb,0xcf,0x56,0xfd,0x7b,
	0xf3,0xed,0x6b,0xe3,0xf5,0x73,0xfd,0xa5,0x5c,0x7b,0x2e,0x56,0x9f,0xba,0x2b,0x8f,
	0xf9,0xf2,0x43,0xb6,0x74,0x9f,0x2e,0xde,0x25,0x0b,0x7a,0x3c,0x7f,0x1b,0xcd,0xdd,
	0x84,0xb3,0xd7,0x9d,0x99,0x9a,0x9a,0xbe,0x0a,0xa6,0x2e,0x7d,0xed,0xc2,0x9b,0x3c,
	0x6f,0x4f,0x9c,0xb5,0xc6,0x4f,0xe5,0xd8,0x89,0x3b,0x7a,0xec,0xa0,0xa3,0xa3,0xa3,
	0xa3,0xa3,0xa3,0xff,0x45,0xfd,0x17,0x94,0x8a,0xd9,0x0e,0x4e,0x2f,0x00,0x00
]);

// Same plaintext, but the gzip header has the FNAME flag set with "hi.txt\0" — exercises
// the variable-length header parser.
const gzipFnameBytes = new Uint8Array([
	0x1f,0x8b,0x08,0x08,0x00,0x00,0x00,0x00,0x00,0xff,0x68,0x69,0x2e,0x74,0x78,0x74,
	0x00,0x0b,0xc9,0x48,0x55,0x28,0x2c,0xcd,0x4c,0xce,0x56,0x48,0x2a,0xca,0x2f,0xcf,
	0x53,0x48,0xcb,0xaf,0x50,0xc8,0x2a,0xcd,0x2d,0x28,0x56,0xc8,0x2f,0x4b,0x2d,0x52,
	0x28,0x01,0x4a,0xe7,0x24,0x56,0x55,0x2a,0xa4,0xe4,0xa7,0xeb,0x71,0x01,0x00,0x6a,
	0xcc,0x50,0xeb,0x2d,0x00,0x00,0x00
]);

// Build a ReadableStream that emits `bytes` in slices of `chunkSize` (or all at once if undefined)
function makeByteStream(bytes, chunkSize) {
	return new ReadableStream({
		start(controller) {
			if (!chunkSize || (chunkSize >= bytes.byteLength)) {
				controller.enqueue(bytes);
			}
			else {
				for (let i = 0; i < bytes.byteLength; i += chunkSize)
					controller.enqueue(bytes.subarray(i, Math.min(i + chunkSize, bytes.byteLength)));
			}
			controller.close();
		}
	});
}

async function readAllAsText(stream) {
	const reader = stream.pipeThrough(new TextDecoderStream()).getReader();
	let text = "";
	for (;;) {
		const { value, done } = await reader.read();
		if (value) text += value;
		if (done) return text;
	}
}

async function runCase(label, format, bytes, chunkSize, expectedText = expected) {
	try {
		const decompressed = makeByteStream(bytes, chunkSize).pipeThrough(new DecompressionStream(format));
		const text = await readAllAsText(decompressed);
		const ok = (text === expectedText);
		trace(`${ok ? "PASS" : "FAIL"}  ${label}\n`);
		if (!ok) {
			trace(`  expected ${expectedText.length} chars, got ${text.length} chars\n`);
			if (expectedText.length < 100)
				trace(`  expected: ${JSON.stringify(expectedText)}\n  got:      ${JSON.stringify(text)}\n`);
		}
	}
	catch (e) {
		trace(`FAIL  ${label}  threw: ${e}\n`);
	}
}

async function expectFailure(label, format, bytes) {
	try {
		const decompressed = makeByteStream(bytes).pipeThrough(new DecompressionStream(format));
		await readAllAsText(decompressed);
		trace(`FAIL  ${label}  (expected error, got success)\n`);
	}
	catch (e) {
		trace(`PASS  ${label}  (rejected as expected: ${e.message || e})\n`);
	}
}

async function main() {
	trace("DecompressionStream tests\n");
	trace("=========================\n");

	// Whole-buffer cases
	await runCase("gzip whole",        "gzip",        gzipBytes);
	await runCase("deflate whole",     "deflate",     deflateBytes);
	await runCase("deflate-raw whole", "deflate-raw", deflateRawBytes);

	// Streaming: feed bytes one at a time to exercise header-buffering and incremental inflate
	await runCase("gzip 1-byte chunks",        "gzip",        gzipBytes, 1);
	await runCase("deflate 1-byte chunks",     "deflate",     deflateBytes, 1);
	await runCase("deflate-raw 1-byte chunks", "deflate-raw", deflateRawBytes, 1);

	// Streaming: 7-byte chunks (gzip header is 10 bytes, so this splits the fixed header)
	await runCase("gzip 7-byte chunks", "gzip", gzipBytes, 7);

	// gzip with FNAME flag set — exercises variable-length header parser
	await runCase("gzip+FNAME whole",         "gzip", gzipFnameBytes);
	await runCase("gzip+FNAME 3-byte chunks", "gzip", gzipFnameBytes, 3);

	// Large output (~12KB) to exercise the bounded-output loop in DecompressionStream.
	// The 2KB scratch must roll over ~6 times — both whole-input and chunked-input cases.
	await runCase("gzip 12KB whole",            "gzip", largeGzipBytes, undefined, largeExpected);
	await runCase("gzip 12KB 32-byte chunks",   "gzip", largeGzipBytes, 32,        largeExpected);

	// Negative cases
	try {
		new DecompressionStream("brotli");
		trace("FAIL  bad format constructor (expected TypeError)\n");
	}
	catch (e) {
		trace(`PASS  bad format constructor (TypeError: ${e.message})\n`);
	}

	// Corrupted body — flip a byte in the middle of the deflate payload
	const corruptDeflate = new Uint8Array(deflateBytes);
	corruptDeflate[10] ^= 0xff;
	await expectFailure("deflate corrupted body", "deflate", corruptDeflate);

	// Truncated stream — cut into the deflate payload so inflate never sees Z_STREAM_END.
	// (We can't reliably detect truncation of just the trailer because miniz's bit-buffer
	// prefetch makes the byte count unreliable near end-of-stream.)
	await expectFailure("gzip truncated", "gzip", gzipBytes.subarray(0, 30));

	// Bad gzip magic
	const badMagic = new Uint8Array(gzipBytes);
	badMagic[0] = 0x00;
	await expectFailure("gzip bad magic", "gzip", badMagic);

	await runBackpressureCase();

	trace("Done.\n");
}

// Real backpressure test: verify that decompression production is paced by consumption.
// We pipe DecompressionStream's readable through a counting TransformStream, pump in
// the full 12 KB-worth of input without reading, and assert that very few output chunks
// are produced. Without backpressure, all ~6 chunks would materialise before our first
// read; with backpressure, the pipe stalls after ~1.
async function runBackpressureCase() {
	try {
		const ds = new DecompressionStream("gzip");

		let produced = 0;
		const counter = new TransformStream({
			transform(chunk, controller) {
				produced++;
				controller.enqueue(chunk);
			}
		});
		const reader = ds.readable.pipeThrough(counter).getReader();

		// Pump the entire 255-byte gzip stream (which decompresses to ~12 KB) without
		// awaiting individual writes. Then close.
		const writer = ds.writable.getWriter();
		for (let i = 0; i < largeGzipBytes.byteLength; i += 32) {
			const end = Math.min(i + 32, largeGzipBytes.byteLength);
			writer.write(largeGzipBytes.subarray(i, end));
		}
		const closePromise = writer.close();

		// Yield many microtasks; without reading, the pipe should stall after one chunk.
		for (let i = 0; i < 100; i++)
			await Promise.resolve();
		const producedWhileIdle = produced;

		// Now drain.
		let total = 0;
		while (true) {
			const { value, done } = await reader.read();
			if (value) total += value.byteLength;
			if (done) break;
		}
		await closePromise;

		// Decompressed length must match. Production while idle must be small.
		// With proper backpressure the pipe pauses after ~1 chunk; without it, all ~6 land.
		const totalOk = (total === largeExpected.length);
		const heldByBackpressure = (producedWhileIdle <= 2);

		if (totalOk && heldByBackpressure)
			trace(`PASS  backpressure  (only ${producedWhileIdle} chunks produced while reader idle; total ${total} bytes)\n`);
		else
			trace(`FAIL  backpressure  producedWhileIdle=${producedWhileIdle} total=${total} expected=${largeExpected.length}\n`);
	}
	catch (e) {
		trace(`FAIL  backpressure  threw: ${e}\n`);
	}
}

main();
