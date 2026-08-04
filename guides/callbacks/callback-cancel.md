---
name: Cancel Callback
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Use `Timer.clear()` to clear an active callback

```js
import Timer from "timer"

const id = Timer.set(() => {
	trace("callback after 1.5 seconds\n");
}, 1500);

Timer.clear(id);
```

---

You can cancel a timer from its callback.

```js
import Timer from "timer"

Timer.repeat(id => {
	Timer.clear(id);
}, 1500);
```

---

For convenience, it is safe to call `Timer.clear()` with `undefined` or `null`. No exception is thrown.

```js
import Timer from "timer"

Timer.clear(undefined);
```
