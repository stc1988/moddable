---
name: Reschedule Callback
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Use `Timer.schedule()` to reschedule when a callback is invoked.

---

This example reschedules a one-time callback. It is invoked after 1000 milliseconds, instead of 5000 milliseconds as originally scheduled.

```js
import Timer from "timer"

const id = Timer.set(() => {
	trace("callback\n");
}, 5000);
Timer.schedule(id, 1000);
```

---

`Timer.schedule()` can also reschedule a repeating callback by passing two intervals – the initial interval and the repeating interval. This behaves like a repeating callback with an initial delay.

In this example, each invocation of the callback reschedules it to be invoked after either 100 ot 1000 milliseconds depending on the system load. The second interval (2000 milliseconds here) must have a non-zero value or the timer is converted to a one-time callback.

```js
import Timer from "timer"

Timer.repeat(id => {
	doBackgroundWork();
	Timer.schedule(id, isSystemBusy() ? 1000 : 100, 2000);
}, 1000);
```
