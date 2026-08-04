---
name: Set System Date and Time
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Use `Time.set()` to set the current date and time of the device. The argument is Unix time in seconds.


```js
import Time from "time"

Time.set(1762555086);
trace((new Date()).toString(), "\n");
// => "Fri Nov 07 2025 14:38:06 GMT-0800"
```

---

To set the time and date with millisecond precision, use a fractional value.

```js
import Time from "time"

Time.set(1762555086.500);
```

---

`Time.set()` is widely available on microcontrollers but not in the simulator.
