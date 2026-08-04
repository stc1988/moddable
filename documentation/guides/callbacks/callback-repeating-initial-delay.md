---
name: Repeating Callback with Initial Delay
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Use `Timer.set()` to schedule a callback to be invoked at a repeating interval following an initial delay. The repeating interval and initial delay are in milliseconds.

In this example, the callback is first invoked after 1000 milliseconds, then 2000 milliseconds later (after 3000 milliseconds have elapsed), and every 2000 milliseconds thereafter.

```js
import Timer from "timer"

Timer.set(() => {
	trace("tick\n");
}, 1000, 2000);
```
---

You can also invoke a callback immediately and then at a repeating interval.

```js
import Timer from "timer"

Timer.set(() => {
	trace("tick\n");
}, 0, 2000);
```
