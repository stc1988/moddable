---
name: Get Unix Time
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

[Unix Time](https://en.wikipedia.org/wiki/Unix_time) is the number of seconds elapsed since midnight January 1, 1970.

`Date.now()` returns Unix Time, but in milliseconds rather than seconds.

```js
const unixTimeNow = Math.round(Date.now() / 1000);
```

---

Note that you can also get the current time as Unix Time by creating a new `Date` instance, however this is less efficient. You may see this in code written before `Date.now()` was widely available.

```js
const d = new Date();
const unixTimeNow = Math.round(d / 1000);
```
