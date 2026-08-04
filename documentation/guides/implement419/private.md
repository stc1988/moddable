---
name: Keep Instance Surface Clean
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Your implementation of an ECMA-419 class must not expose its internal implementation. This means that any methods and properties that are not specified by ECMA-419 may not be accessible to scripts. This is important to avoid potential collisions, to allow scripts to attach their own properties to the instance, and to avoid scripts modifying or depending on internal state.

---

There are several techniques for hiding implementations in JavaScript. The most straightforward is to use the [private fields and private methods](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Classes/Private_elements) that are part of JavaScript.

```js
class My419Example {
	#state = "uninitialized";

	constructor() {
		// initialize target
		this.#detectHardware();
	}
	#detectHardware() {
		if (/* hardware detected */)
			this.#state = "found";
		else
			throw new Error("hardware not found");
	}
}
```

---

Note that each private method has a small runtime memory cost. While this may be insignificant for many projects, it is a good reminder not to over-factor code into tiny helper functions. Sometimes private methods can be implemented as static functions instead. For example, if `#detectHardware()` doesn't need to access private fields or call private methods, the previous example could be rewritten.

```js
function detectHardware() {
	if (/* hardware detected */)
		return "found";
	throw new Error("hardware not found");
}

class My419Example {
	#state = "uninitialized";

	constructor() {
		// initialize target
		this.#state = detectHardware();
	}
}
```
