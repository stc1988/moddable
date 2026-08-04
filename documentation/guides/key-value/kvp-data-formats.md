---
name: Change Data Formats using Key-Value Pair
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

To override the default data type of `buffer`, specify the type in the `format` property to `open()`.

Supported types are "buffer", "int8", "uint8", "int16", "uint16", "int32", "uint32", "int64", "uint64", and "string".

```js
const settings = device.keyValue.open({
	path: "settings",
	format: "string"
});

settings.write("hello", "world");
trace(`${settings.read("hello")}\n`);

settings.write("json", JSON.stringify({ hello: "world" }));
trace(`${settings.read("json")}\n`);
let json = JSON.parse(settings.read("json"));

settings.close();
```

---

To change the data type of values after opening the domain, set the `format` property.

When reading a value, the `format` must match the `format` used when the value was written. There is no automatic type conversion.

```js
const settings = device.keyValue.open({
	path: "settings"
});

settings.format = "string";
settings.write("hello", "world");
settings.format = "buffer";
settings.write("byte", Uint8Array.of(1));
settings.format = "uint32";
settings.write("32 bits", 0xffff_ffff);

settings.format = "string";
trace(`${settings.read("hello")}\n`);
settings.format = "buffer";
trace(`${new Uint8Array(settings.read("byte")).toHex()}\n`);
settings.format = "uint32";
trace(`${settings.read("32 bits")}\n`);

settings.close();
```
