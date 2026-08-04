---
name: Combine ArrayBuffers
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

The `concat()` method of `ArrayBuffer` combines multiple buffers into a single `ArrayBuffer`. 

The `concat()` method is an extension to the standard JavaScript `ArrayBuffer`.

```js
const buffer1 = Uint8Array.of(0, 1).buffer;
const buffer2 = Uint8Array.of(2).buffer;
const buffer3 = Uint8Array.of(3, 4).buffer;

const combined = buffer1.concat(buffer2, buffer3);
// => ArrayBuffer of 0, 1, 2, 3, 4
```
