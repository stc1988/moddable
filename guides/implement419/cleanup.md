---
name: Constructor Clean-up on Failure
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

The constructor is often the biggest single function in an ECMA-419 implementation. Because it does so much, there are many ways the constructor can fail, particularly when creating connections to hardware resources. If the constructor fails, it is essential to release all resources it allocated.

A convenient approach is calling `close()` when an exception is thrown. Because of the [requirement](./close.md) that close be safe to call multiple times, it can easily also clean up a partially initialized instance.

You cannot wait for the garbage collector to release the resources automatically. That may be too late for the script and ECMA-419 defines many instances as being unavailable for garbage collection until they are closed.

```js
class ExampleSensor {
	#i2c;
	#interrupt;

	constructor(options) {
		try {
			this.#i2c = /* initialize i2c */;
			this.#interrupt = /* initialize interrupt */;
		}
		catch (e) {
			this.close();
			throw e;
		}
	}
	close() {
		this.#i2c?.close();
		this.#i2c = undefined;
		this.#interrupt?.close();
		this.#interrupt = undefined;
	}
}
```
