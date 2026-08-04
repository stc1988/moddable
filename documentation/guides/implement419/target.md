---
name: Constructor Sets `target`
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

The [Base Class Pattern ](https://419.ecma-international.org/#base-class-pattern) defines the first argument of every constructor as an options object. If the options object has a `target` property, the constructor must assign that to the instance.

Note that `target` is set as an ordinary property. It is not necessary to use `Object.defineProperty()`. Because it is an ordinary property the caller may change or delete the property at any time.

```js
class Base {
	constructor(options) {
		if ("target" in options)
			this.target = options.target;
	}
}
```

---

`target` may be of any type, and the ECMA-419 implementation never accesses it after initialization in the constructor. The `target` property is useful for getting back to a context from a [callback](./callbacks.md).

```js
new Digital({
	target: this,
	pin: 1,
	mode: Digital.InputPullUp,
	edge: Digital.Rising | Digital.Falling,
	onReadable() {
		const value = this.read();
		this.target.update(value);
	}
});
```
