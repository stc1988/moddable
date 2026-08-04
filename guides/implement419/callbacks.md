---
name: Calling Callbacks
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

ECMA-419 uses callbacks extensively to deliver notifications. Callbacks are used instead of [Events](https://developer.mozilla.org/en-US/docs/Web/API/Event) or [Promises](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise) because they are typically more efficient. Often callbacks are passed to the constructor to be called later.

---

Callbacks may only be called when there is nothing else running. Specifically this means that methods defined by ECMA-419 may not directly call a callback. This eliminates common bugs that happen when a callback is invoked at an unexpected time and it ensures that there is consistent free stack space for the callback's execution.

A timer is a common way to defer execution of a callback to meet this requirement.

```js
import Timer from "timer";

class ExampleTCP {
	#onWritable;
	#timer;

	constructor(options) {
		this.#onWritable = options.onWritable;
		if (this.#onWritable)
			this.#timer = Timer.set(() => this.#onWritable());
	}
	close() {
		Timer.clear(this.#timer);
		this.#timer = undefined;
	}
}
```

---

Callbacks must be invoked with `this` set to the calling instance (scripts can override this using [`function.prototype.bind()`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Function/bind) or [arrow functions](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Functions/Arrow_functions)). The preceding examples calls `onWritable` with the correct `this` because the callback is called through a private field, `this.#onWritable`. If a callback is stored on another object, use [`function.prototype.call()`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Function/call) to set `this` to the instance.

```js
import Timer from "timer";

class ExampleTCP {
	#callbacks = {};
	#timer;

	constructor(options) {
		const {onWritable} = options;
		if (onWritable) {
			this.#callbacks.onWritable = onWritable;
			this.#timer = Timer.set(() => this.#callbacks.onWritable.call(this));
		}
	}
	close() {
		Timer.clear(this.#timer);
		this.#timer = undefined;
	}
}
```

---

Perhaps the most misunderstood callback is `onError()`. This callback is called when the instance detects a failure outside of a public method call. For example, when a TCP socket unexpectedly disconnects.

If an error occurs within a public API call, usually the right thing to do is throw an exception immediately. If it is appropriate to call `onError()`, the callback needs to be deferred as shown above using a `Timer`. 

A common mistake is invoking `onError()` from the constructor. The constructor is a public API, it cannot invoke callbacks directly. Therefore, it should throw an exception.
