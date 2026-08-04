---
name: Convert Hex to Binary Data
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Use the JavaScript standard [`Uint8Array.fromHex()` method](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Uint8Array/fromHex) to convert a hex encoded string to a `Uint8Array`.

```js
const string = "0001020304FF";
const bytes = Uint8Array.fromHex(string);
// => Uint8Array of 0, 1, 2, 3, 4, 255
```

---

`Uint8Array.fromHex()` is case insensitive.

```js
const string = "0001020304Ff";
const bytes = Uint8Array.fromHex(string);
// => Uint8Array of 0, 1, 2, 3, 4, 255
```
