---
name: Delete Directory
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

To delete a directory, call `delete()`.

```js
device.files.delete("docs/dir");
```

---

A directory must be empty in order to be deleted. You can recursively delete the contents of a directory prior to deleting the directory itself.

See [Enumerate Directory](./dir-enumerate.md) for more about directory iterators.

```js
function deleteDirectory(path) {
	for (let name of device.files.scan(path)) {
		const thisPath = (path ? (path + "/") : "") + name;
		if (device.files.status(thisPath).isFile())
			device.files.delete(thisPath);
		else
			deleteDirectory(thisPath);
	}
	device.files.delete(path);
}
deleteDirectory("docs");
```

---

`delete()` returns `true` if the directory is successfully deleted and `false` if the directory does not exist.

If the directory cannot be deleted, an exception is thrown.

```js
if (device.files.delete("docs/dir"))
	trace(`directory deleted\n`);
else
	trace(`no directory to delete\n`);
```
