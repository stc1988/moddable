---
name: Immediate Callback
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Calling `Timer.set()` with only a callback function argument invokes the callback as soon as possible.

```js
import Timer from "timer"

Timer.set(() => {
	trace("immediate callback\n");
});
```
---

The callback is not invoked immediately, but at the next opportunity when no other JavaScript is executing in the virtual machine. This has several benefits:

- The callback runs after any other code in the function and its callers.
- The callback runs on an empty stack, which reduces the potential for a stack overflow.
- Dividing an operation into pieces may increase overall system responsiveness by reducing the time the JavaScript event loop is blocked.

---

Fans of JavaScript Promises might be tempted to use a Promise as a standard alternative to `Timer.set()`. This works, but is less efficient. If possible, use `Timer.set()` instead.

```js
Promise.resolve().then(() => {
  trace("immediate callback\n");
});
```
