---
name: Get Time of Day
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Use the standard [JavaScript `Date` object](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date) to get the current time. Then, use its methods to get the parts of the time.

```js
const now = new Date();

trace(now.toTimeString(), "\n");
// => "14:15:50 GMT-0800"

trace(now.getHours(), "\n");
// => 14

trace(now.getMinutes(), "\n");
// => 17

trace(now.getSeconds(), "\n");
// => 50

trace(now.getMilliseconds(), "\n");
// => 513
```
---

You can also access the parts of the time in UTC, without the local time zone applied.

```js
const now = new Date();

trace(now.getUTCHours(), "\n");
// => 22

trace(now.getUTCMinutes(), "\n");
// => 17

trace(now.getUTCSeconds(), "\n");
// => 50

trace(now.getUTCMilliseconds(), "\n");
// => 513
```
