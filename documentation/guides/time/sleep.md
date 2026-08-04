---
name: Sleep
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

To sleep for an interval of time, use `Timer.delay()`. The sleep interval is in milliseconds, so this example sleeps for 50 milliseconds.

```js
import Timer from "timer"

Timer.delay(50);
```
---

`Timer.delay()` yields execution which allows other threads to run. For this reason, it is strongly preferred over a `for` loop to busy wait.

Note that JavaScript on the Web platform avoids blocking so much that it does not provide a function to block for a period of time. However, Embedded JavaScript sometimes needs to block for a short interval, for example in a device driver after performing a hardware operation that requires time to complete. `Timer.delay()` is invaluable in such situations. It should be used sparingly.
