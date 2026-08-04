---
name: Get File Information
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

To get information about an open file, call `status()` on the file instance.

```js
const help = device.files.openFile({
	path: "docs/help.txt",
	mode: "r"
});
const status = help.status();
trace(`file size: ${status.size} bytes\n`);
trace(`isFile: ${status.isFile()}\n`);
trace(`isDirectory: ${status.isDirectory()}\n`);
trace(`isSymbolicLink: ${status.isSymbolicLink()}\n`);
```

---

To get information about a file that isn't open, call `status()` on a parent directory. The example uses `device.files`, the root directory instance.

```js
const status = device.files.status("docs/help.txt");
trace(`file size: ${status.size} bytes\n`);
trace(`isFile: ${status.isFile()}\n`);
trace(`isDirectory: ${status.isDirectory()}\n`);
trace(`isSymbolicLink: ${status.isSymbolicLink()}\n`);
```
