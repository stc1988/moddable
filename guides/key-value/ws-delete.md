---
name: Delete Keys using Web Storage
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Use the `removeItem()` method to remove a key-value pair.

If the specified key is not in the domain, `removeItem()` does not throw an exception.

```js
localStorage.setItem("delete me", "1");
localStorage.removeItem("delete me");
localStorage.removeItem("delete me");
```

---

There is no API to delete a domain, but you can achieve the same result by deleting all the domain's keys. Learn more about enumerating Web Storage keys [here](./ws-enumerate.md).

```js
while (localStorage.length)
	localStorage.removeItem(localStorage.key(0));
```
