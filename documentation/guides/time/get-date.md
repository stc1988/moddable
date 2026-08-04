---
name: Get Date
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Use the standard [JavaScript `Date` object](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date) to get the current date. Then, use its methods to get the parts of the date.

```js
const now = new Date();

trace(now.toDateString(), "\n");
// => Fri Nov 07 2025

trace(now.getFullYear(), "\n");
// => 2025

trace(now.getMonth() + 1, "\n");
// => 11

trace(now.getDate(), "\n");
// => 7
```
---

You can also access the parts of the date in UTC, without the local time zone applied.

```js
const now = new Date();

trace(now.getUTCFullYear(), "\n");
// => 2025

trace(now.getUTCMonth() + 1, "\n");
// => 11

trace(now.getUTCDate(), "\n");
// => 7
```
