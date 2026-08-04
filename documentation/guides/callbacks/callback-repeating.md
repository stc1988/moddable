---
name: Repeating Callback
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Use `Timer.repeat()` to schedule a callback to be invoked at a regular interval. The interval is in milliseconds and the callback is first invoked after the interval. In this example, the interval is 1000 milliseconds.


```js
import Timer from "timer"

Timer.repeat(() => {
	trace("tick\n");
}, 1000);
```
---

The callback is invoked when no other JavaScript is running, so it may be delayed if the JavaScript event loop is blocked.

There is no drift accumulated: if a callback is delayed, the next invocation will still be at the originally scheduled time.
