---
name: Avoid Copying Buffers
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Embedded JavaScript developers often work with [binary data buffers](../binary/index.md). Making copies of data in buffers can be expensive, especially for large buffers. It requires additional memory and the copy operation takes time (and the RAM access speed on embedded devices is often not that fast). Fortunately, there are ways to avoid copying buffers.

---

The [`subarray()`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/TypedArray/subarray) method of TypedArray creates a new TypedArray instance that references a part of the original TypedArray. Compare this to `slice()` which makes a copy of the buffer.

Because `subarray()` does not make a copy of the buffer, changes to the original buffer will be reflected in the subarray and changes to the subarray will be reflected on the original buffer.

```js
/* BEFORE */
let bytes = Uint8Array.of(0, 1, 2, 3, 4, 5);
let part = bytes.slice(1, 3);
// => [1, 2]
bytes[1] = 100;
// => part[0] = 1

/* AFTER */
let bytes = Uint8Array.of(0, 1, 2, 3, 4, 5);
let part = bytes.subarray(1, 3);
// => [1, 2]
bytes[1] = 100;
// => part[0] = 100
```

---

A `DataView` is commonly used to work with binary data that contains several different types of data. It can be convenient to simultaneously access the underlying binary data as both a `DataView` and a TypedArray, for example when a `Uint8Array` is embedded in the binary data.

```js
let view = getDataView();
let bytes = new Uint8Array(
	view.buffer,
	view.byteOffset + 10,
	20);
// => bytes references data from
//    offset 10 to 30 of view
```

---

You can also go the other way: creating a `DataView` that references data within a TypedArray:

```js
let bytes = getUint8Array();
let view = new DataView(
	bytes.buffer,
	bytes.byteOffset + 10,
	20);
// => view references data from
//    offset 10 to 30 of bytes
```
---

You can pass buffers created using these "no copy" techniques to many standard APIs. For example, you can call the `TextDecoder` on bytes embedded within a `DataView` to extract a string. This example reads the length of the string in bytes from offset 50 in the `DataView` and creates a string from that number of bytes that follow.

```js
let view = getDataView();
let stringBytes = new Uint8Array(
	view.buffer,
	view.byteOffset + 51,
	view.getUint8(50));
let decoder = new TextDecoder();
let string = decoder.decode(stringBytes);
```
