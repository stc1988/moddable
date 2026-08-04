---
name: Open Directory
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Opening a directory is an advanced feature that most projects will not need to use.

Opening a directory creates a new directory instance that is limited to accessing a subtree of the file system. This can be passed to an API to sandbox it to a portion of the file system.

---

To open a directory, call `openDirectory()` with the path.

The new directory instance has the same methods as `device.files`, so you can use it to open files, delete files, enumerate directories, and other operations.

```js
const docs = device.files.openDirectory({path: "files/docs"});

// opens files/docs/foo.md
const f = docs.openFile({path: "foo.md", mode: "r"});
f.close();

// deletes files/docs/foo.md
docs.delete("foo.md");

// enumerates files/docs/assets
for (let name of docs.scan("assets"))
	trace(`${name}\n`);
```
