---
name: Delete File
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

To delete a file, call `delete()`.

```js
device.files.delete("docs/help.txt");
```

---

`delete()` returns `true` if the file is successfully deleted and `false` if the file does not exist.

If the file cannot be deleted, an exception is thrown.

```js
if (device.files.delete("docs/help.txt"))
	trace(`file deleted\n`);
else
	trace(`no file to delete\n`);
```
