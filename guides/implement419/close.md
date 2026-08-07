---
name: `close()` and `[Symbol.dispose]`
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

The `close()` method is responsible for releasing all resources held by the instance. Callers expect the resources to be released immediately so that they can be reused by another instance.

---

`close()` is defined by ECMA-419 to be safe to call more than once. That means that if `close()` is called more than once, all executions beyond the first should do nothing. A simple way to achieve this is to set any resources to `undefined` in `close()` and then use [optional chaining](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Optional_chaining) to check if they are `undefined`.

```js
class ExampleSensor {
	#i2c;
	#interrupt;

	constructor(options) {
		// initialized #i2c & #interrupt
	}
	close() {
		this.#i2c?.close();
		this.#i2c = undefined;
		this.#interrupt?.close();
		this.#interrupt = undefined;
	}
}
```

---

After `close()`, any other method invoked on the instance should throw an exception. The obvious way to do this check is with an explicit check that the object is closed, for example using a `#state` property. 

However, for many classes, no additional code may be required. Consider a sensor's `sample()` method. It will almost immediately make an I²C `read()` call. That will throw after `close()` because `this.#i2c` is `undefined`. From an ECMA-419 perspective, that's sufficient. You are free to do more, of course. If your implementation wants to provide specific error message to assist developers in this unusual situation, that's fine.

```js
class ExampleSensor {
	#i2c;

	constructor(options) {
		// initialized #i2c
	}
	close() {
		this.#i2c?.close();
		this.#i2c = undefined;
	}
	sample() {
		const value = this.#i2c.writeRead(Uint8Array.of(1), 2);
		return (new Uint8Array(value))[0];
	}
}
```

---

After `close()` is called, an instance may not invoke any callbacks. That means no callbacks from within `close()` or any later time.

This example binds a callback to the `onReadable()` notification of a [Digital](https://419.ecma-international.org/#io-classes-digital) interrupt. The `close()` method releases the Digital instance, canceling any pending callbacks.

If you use a timer to invoke a callback, clear the timer in `close()`.

This rule does not apply to [asynchronous `close()`](https://419.ecma-international.org/#base-class-pattern-close-method). Most `close()` methods defined by ECMA-419 are synchronous.

This requirement can be difficult to implement in C code for native callbacks that cannot be canceled once in flight.

```js
class ExampleSensor {
	#interrupt;

	constructor(options) {
		const {interrupt, onSample} = options;

		if (interrupt && onSample) {
			this.#interrupt = new interrupt.io({
				...interrupt,
				edge: interrupt.io.Rising,
				onReadable: onSample.bind(this)
			});
		}
	}
	close() {
		this.#interrupt?.close();
		this.#interrupt = undefined;
	}
}
```

---

ECMA-419 aliases `[Symbol.dispose]`, defined by [Explicit Resource Management](https://github.com/tc39/proposal-explicit-resource-management), to the `close()` method. This allows ECMA-419 instances to use Explicit Resource Management features including `using` and `DisposableStack`.

A straightforward way to perform this alias is with a `static` code block in your class.

```js
class ExampleSensor {
	#i2c;

	constructor(options) {
		// initialized #i2c
	}
	close() {
		this.#i2c?.close();
		this.#i2c = undefined;
	}

	static {
		this.prototype[Symbol.dispose] = this.prototype.close;
	}
}
```

