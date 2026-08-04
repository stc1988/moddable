---
name: Enumerate Keys using Key-Value Pair
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

The domain returned by `open()` may be used as an [iterator](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Iterator) to retrieve all keys in the domain.

The order of keys returned by the iterator is host-dependent. It may not match the order in which they were created.

```js
const settings = device.keyValue.open({
	path: "settings"
});

for (const key of settings)
	trace(`${key}\n`);

settings.close();
```
