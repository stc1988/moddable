---
name: Delete Keys using Key-Value Pair
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Use the `delete()` method to remove a key-value pair.

If the specified key is not in the domain, `delete()` does not throw an exception.

```js
const settings = device.keyValue.open({
	path: "settings"
});

settings.write("delete me", Uint8Array.of(1));
settings.delete("delete me");
settings.delete("delete me");

settings.close();
```

---

There is no API to delete a domain, but you can achieve the same result by deleting all the domain's keys.

This example uses the domain instance as an [iterator to retrieve the domain's keys](./kvp-enumerate.md).

```js
const settings = device.keyValue.open({
	path: "settings"
});

Array.from(settings).forEach(key => {
	settings.delete(key);
});

settings.close();
```
