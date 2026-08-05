---
name: Enumerate Directory
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-05
---

Use the [iterator](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Iterator) returned by `scan()` to retrieve the names of all files, directories, and symbolic links in a directory.

```js
for (let name of device.files.scan("docs/dir"))
	trace(`${name}\n`);
```

---

This example concisely creates an array containing the names of all markdown files in a directory by chaining iterators.

```js
const markdowns = device.files.scan("docs")
		.filter(name => name.endsWith(".md"))
		.toArray();
```

---

The order of files returned by the iterator is host-dependent and may not be alphabetically sorted. Use `sort()` to generate an alphabetically ordered list.

```js
const sorted = device.files.scan("docs")
		.toArray()
		.sort((a, b) =>
			a.toLowerCase().localeCompare(b.toLowerCase()));
```
