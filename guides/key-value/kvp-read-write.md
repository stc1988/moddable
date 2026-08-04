---
name: Read and Write Values using Key-Value Pair
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

To access key-value pairs, first open a domain, then use `read()` and `write()`. Keys are always strings. Values default to buffers. Any [Byte Buffer](https://419.ecma-international.org/#byte-buffer) may be passed to `write()`, and `read()` returns an `ArrayBuffer`. You can [change the data format](./kvp-data-formats.md).

Note that domain and key names are case-sensitive and may contain any Unicode character. Most host implementations limit the length of domain and key names. For portability, limit them to 31 bytes.

```js
const settings = device.keyValue.open({
	path: "settings"
});

settings.write("byte", Uint8Array.of(1));
let value = settings.read("byte");
trace(`${new Uint8Array(value).toHex()}\n`);

settings.write("two bytes", Uint8Array.of(1, 2));
value = settings.read("two bytes");
trace(`${new Uint8Array(value).toHex()}\n`);

settings.close();
```

---

If the requested key is not present in the domain, `read()` returns `undefined` rather than throwing an exception.

```js
const settings = device.keyValue.open({
	path: "settings"
});

if (undefined === settings.read("once"))
	settings.write("once", Uint8Array.of(1));

settings.close();
```
