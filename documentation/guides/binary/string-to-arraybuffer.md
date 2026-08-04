---
name: Convert String to ArrayBuffer
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

The Moddable SDK provides two ways to convert a `String` to an `ArrayBuffer`. The `ArrayBuffer.fromString()` method is lightweight and sufficient for most embedded JavaScript needs. The standard [`TextEncoder`](https://developer.mozilla.org/en-US/docs/Web/API/TextEncoder) class is Web-compatible and more full featured.

---

`ArrayBuffer.fromString()` requires no set-up and converts JavaScript strings to UTF-8 encoded data.

`ArrayBuffer.fromString()` does not correctly convert strings containing surrogate pairs. If you expect to process such strings, use `TextEncoder` instead.

```js
const string = "hello";

const buffer = ArrayBuffer.fromString(string);
// => ArrayBuffer of 104, 101, 108, 108, 111
```

---

To use the `TextEncoder` class, instantiate a `TextEncoder` instance and call its `encode()` method. The `encode()` method returns a `Uint8Array`; access its `ArrayBuffer` through the `buffer` property.

```js
const string = "hello";
const encoder = new TextEncoder;

const bytes = encoder.encode(string);
// => Uint8Array of 104, 101, 108, 108, 111

const buffer = bytes.buffer;
// => ArrayBuffer of 104, 101, 108, 108, 111
```
