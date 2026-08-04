---
name: Enumerate Keys using Web Storage
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Web Storage instances have a `length` property that indicates the number of keys and a `key()` method to return the key at a given index. You can use these to iterate through the keys.

The order of keys is host-dependent. It may not match the order in which they were created.

```js
for (let i = 0; i < localStorage.length; i++)
	trace(`${localStorage.key(i)}\n`);
```
