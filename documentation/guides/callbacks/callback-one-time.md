---
name: One-Time Callback
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Use `Timer.set()` to schedule a callback to be invoked at a future time. The delay interval is in milliseconds which is 1500 in this example.

```js
import Timer from "timer"

Timer.set(() => {
	trace("callback after 1.5 seconds\n");
}, 1500);
```
