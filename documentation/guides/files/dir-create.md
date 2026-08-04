---
name: Create Directory
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

To create a new directory, call `createDirectory()`.

```js
device.files.createDirectory("docs/dir");
```

---

To check if a directory already exists, call `status()`. This example only creates the directory if it does not already exist.

```js
const status = device.files.status("docs/dir");
if (!status.isDirectory())
	device.files.createDirectory("docs/dir");
```

---

`createDirectory()` returns `true` if the directory is successfully created and `false` if the directory already exists.

If the directory cannot be created, an exception is thrown.

```js
if (device.files.createDirectory("docs/dir"))
	trace(`directory created\n`);
else
	trace(`directory exists\n`);
```
