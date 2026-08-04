---
name: Convert Binary Data to Hex
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Use the JavaScript standard [`toHex()` method](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Uint8Array/toHex) of `UInt8Array` to convert binary data to a hex encoded string.

```js
const bytes = Uint8Array.of(0, 1, 2, 3, 4, 255);
const hex = bytes.toHex();
// => "0001020304ff"
```

---

To convert an `ArrayBuffer` to hex, wrap it in a `Uint8Array`.

```js
const buffer = new ArrayBuffer(2);
const hex = (new Uint8Array(buffer)).toHex();
// => "0000"
```
