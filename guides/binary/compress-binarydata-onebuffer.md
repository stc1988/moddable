---
name: Compress Binary Data – One Buffer
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

The `Deflate` module provides [zlib](https://www.zlib.net) compression using a subset of the [Pako](https://github.com/nodeca/pako/blob/master/README.md) API.


---

Compress a single binary data buffer. The second argument to `push()` is `true` to indicate this is the only buffer to compress.

```js
const data = Uint8Array.of(0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7);
const deflator = new Deflate({});

deflator.push(data, true);
// => deflator.result is a Uint8Array with compressed data

deflator.close()
```
