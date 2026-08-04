---
name: Suspend Callback
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Use `Timer.schedule()` to suspend a callback. Once a callback has been suspended, it will not be invoked again until it is rescheduled.

Suspending a callback and later rescheduling is often lighter than creating and canceling callbacks.

---

This example creates a repeating callback that suspends itself when invoked. Two seconds after being suspended, another callback reschedules the repeating callback to be invoked again in 1000 milliseconds.

```js
import Timer from "timer"

const repeating = Timer.repeat(() => {
	trace("tick\n");
	// suspend
	Timer.schedule(repeating);
}, 5000);

Timer.set(() => {
	Timer.schedule(repeating, 1000, 1000);
}, 7000);
```

---

Sometimes you want a callback to begin in an unscheduled state, so it can be scheduled later. Both `Timer.set()` and `Timer.repeat()` create scheduled callbacks. Use either and then immediately unschedule the callback.

```js
import Timer from "timer"

const id = Timer.repeat(() => {
	trace("tick\n");
}, 1000);

// immediately suspend
Timer.schedule(id);
```
