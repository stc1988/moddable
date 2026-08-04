---
name: Decompress Binary Data – Streaming
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

The `Inflate` module provides [zlib](https://www.zlib.net) decompression using a subset of the [Pako](https://github.com/nodeca/pako/blob/master/README.md) API.

---

Stream multiple compressed input data buffers to `Inflate` by setting the second argument to `push()` to `true` on only the last buffer.

```js
const inflator = new Inflate();

const compressedData = Uint8Array.of(
	0x78, 0xda, 0xed, 0xc5, 0xb1, 0x0d,
	0x00, 0x20, 0x08, 0x00, 0x30, 0x04,
	0x84, 0xff, 0x3f, 0xf6, 0x0e, 0x93,
	0x76, 0x69, 0x9c, 0xac, 0xbe, 0xb3,
	0x61, 0xdb, 0xb6, 0x6d, 0xdb, 0xb6,
	0x6d, 0xdb, 0xb6, 0x6d, 0xdb, 0xb6,
	0xbf, 0xfd, 0x01, 0x19, 0x00, 0x70,
	0x01);

inflator.push(compressedData.subarray(0, compressedData.length >> 1), false);
inflator.push(compressedData.subarray(compressedData.length >> 1), true);

// => inflator.result is a Uint8Array with 8192 decompressed bytes

inflator.close();
```

---

Stream the output of `Inflate` by implementing `onData()` and `onEnd()` callbacks. Streaming output is useful when the decompressed data is larger than available memory.

This `compressedData` decompresses to 8192 bytes.

```js
const inflator = new Inflate();

inflator.onData = buffer => {
	// => buffer is Uint8Array of decompressed data
};

inflator.onEnd = error => {
	if (error) {
		// decompression failed. handle error.
	}
	else {
		// decompression successfully completed.
	}
};

const compressedData = Uint8Array.of(
	0x78, 0xda, 0xed, 0xc5, 0xb1, 0x0d,
	0x00, 0x20, 0x08, 0x00, 0x30, 0x04,
	0x84, 0xff, 0x3f, 0xf6, 0x0e, 0x93,
	0x76, 0x69, 0x9c, 0xac, 0xbe, 0xb3,
	0x61, 0xdb, 0xb6, 0x6d, 0xdb, 0xb6,
	0x6d, 0xdb, 0xb6, 0x6d, 0xdb, 0xb6,
	0xbf, 0xfd, 0x01, 0x19, 0x00, 0x70,
	0x01);
inflator.push(compressedData, true);

inflator.close();
```