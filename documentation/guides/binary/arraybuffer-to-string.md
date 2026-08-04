---
name: Convert ArrayBuffer to String
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

The Moddable SDK provides two ways to convert an `ArrayBuffer` to a `String`. The `String.fromArrayBuffer()` method is lightweight and sufficient for most embedded JavaScript needs. The standard [`TextDecoder`](https://developer.mozilla.org/en-US/docs/Web/API/TextDecoder) class is Web-compatible and more full featured.

---

`String.fromArrayBuffer()` requires no set-up and converts most buffers containing UTF-8 encoded data to a JavaScript string.

`String.fromArrayBuffer()` does not correctly handle UTF-8 values represented as surrogate pairs in UTF-16. If you expect to process such strings, use `TextDecoder` instead.

```js
// ASCII values for "hello"
const bytes = Uint8Array.of(104, 101, 108, 108, 111);
const string = String.fromArrayBuffer(bytes.buffer);
// => "hello"
```

---

To extract a string from part of a buffer, use the optional offset and length arguments to `String.fromArrayBuffer()` 

```js
// ASCII values for "hello"
const bytes = Uint8Array.of(104, 101, 108, 108, 111);
const string = String.fromArrayBuffer(bytes.buffer, 1, 2);
// => "el"
```

---

To use the `TextDecoder` class, instantiate a `TextDecoder` instance and call its `decode()` method.

```js
// ASCII values for "hello"
const bytes = Uint8Array.of(104, 101, 108, 108, 111);
const decoder = new TextDecoder;
const string = decoder.decode(bytes.buffer);
// => "hello"
```
