---
name: Setting Options with configure()
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-11
---

ECMA-419 uses [`configure()`](https://419.ecma-international.org/#peripheral-class-pattern-configure-method) extensively to change the settings of an instance. The committee chose the general-purpose `configure()` over dozens of special-purpose, limited-use APIs. This keeps the API small and understandable. It provides a simple way to extend the API for features specific to a single hardware component. It can also be more efficient as setting several properties at once often allows combining what would otherwise be several hardware transactions. In addition, `configure()` reflects a common hardware programming paradigm, the omnipresent [`ioctl`](https://www.man7.org/linux/man-pages/man2/ioctl.2.html).

```js
globalThis.screen.configure({
	flip: "h",
	brightness: 1.0,
	rotation: 90
});
```

---

Note that `configure()` is only to configure the instance's behavior, not how the instance communicates with the hardware. For example, it should not be used to change the baud rate used to communicate with a GPS sensor, but could be used to change the target accuracy of the GPS location.

ECMA-419 defines the hardware connection to be fixed at the time of construction. To modify communication properties, close the instance and reopen a new one.

---

By definition, `configure()` only changes the properties present at the root of the options object. The absence of a property means "don't change" not "reset to default." A typical implementation uses `in` to check for the existence of known properties. Unknown properties are silently ignored.

```js
class DisplayExample {
	flags = 0;
	constructor() { /* placeholder */ }
	configure(options) {
		let flags = this.flags;
		if ("flip" in options) {
			const value = ["", "h", "v",
				"hv"].indexOf(options.flip);
			if (value < 0)
				throw new Error(`invalid flip: ${options.flip}`);
			flags = (flags & ~0x03) | value;
		}
		if ("rotation" in options) {
			flags &= (~0x03 << 2);
			flags |=
				(Math.idiv(options.rotation, 90) & 0x03) << 2;
		}
		if (flags !== this.flags) {
			/* set modified flags on hardware */
		}
	}
}
```

---

When a new instance is created, it can be useful to reset all hardware options to a known state. This is particularly important after a soft reset, where the hardware component is not powered down. Some hardware components have a reset command or pin for reset. For others, you may need to modify some settings directly. A convenient way to do this can be to call `configure()` from the constructor.

```js
class DisplayExample {
	flags = 0;
	constructor() {
		/* initialize hardware connection */
		this.configure({
			flip: "",
			brightness: 0.5,
			rotation: 0
		});
	}
	configure(options) {
			/* as in the above example */
	}
}
```
