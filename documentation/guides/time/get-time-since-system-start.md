---
name: Get Time Since System Start
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

When measuring an interval of time, it can be more efficient to use elapsed system time instead of the time of day. `Time.ticks` returns the number of milliseconds since the system started. `Time.ticks` is often more efficient than `Date.now()`.

```js
import Time from "time"

trace(`Time.ticks ${Time.ticks} ms\n`);
```

---

Because elapsed system time may be only 32-bits on some microcontrollers, it will eventually wrap around. To help safely calculate elapsed time, use `Time.delta()`.

```js
import Time from "time"

const start = Time.ticks;
// do some long running operation
const end = Time.ticks;
const elapsed = Time.delta(start, end);
```

---

For convenience, if `Time.delta` is called with a single argument, the end time defaults to the current time.

```js
import Time from "time"

const start = Time.ticks;
// do some long running operation
const elapsed = Time.delta(start);
```
