---
name: Decompress Binary Data – One Buffer
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

The `Inflate` module provides [zlib](https://www.zlib.net) decompression using a subset of the [Pako](https://github.com/nodeca/pako/blob/master/README.md) API.

---

Decompress a single binary data buffer. The second argument to `push()` is `true` to indicate this is the only buffer to decompress.

The `compressedData` bytes may be generated using `Deflate`.

```js
const compressedData = Uint8Array.of(
		0x78, 0xda, 0x63, 0x60, 0x64, 0x62, 0x66, 0x61,
		0x65, 0x63, 0x67, 0x80, 0xd2, 0x00, 0x01, 0x98,
		0x00, 0x39);

const inflator = new Inflate();

inflator.push(compressedData, true);
// => inflator.result is a Uint8Array with decompressed data
// => decompressed data is [0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7]

inflator.close();
```
