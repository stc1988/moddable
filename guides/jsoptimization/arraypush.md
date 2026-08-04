---
name: Append to an Array
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Scripts use `push()` to add one item to the end of an array. When there are several items to add to the array at once, it is common to use three calls to `push()`.

Instead, you can add all of the items with a single call to `push()`. This eliminates the overhead of multiple function calls and grows the array just once instead of three times.

```js
/* BEFORE */
array.push("one");
array.push("two");
array.push("three");

/* AFTER */
array.push("one", "two", "three");
```
