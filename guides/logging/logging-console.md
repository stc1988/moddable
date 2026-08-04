---
name: Logging with Console
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

This guide covers the basic logging capabilities of `console`. [MDN](https://developer.mozilla.org/en-US/docs/Web/API/console) provides information on the remaining APIs.

---

`console.log()` is the most commonly used method of `console`. It outputs the message and appends a linefeed.

```js
console.log("hello, world.");
-> hello, world.

console.log("hello,");
console.log(" world.");
-> hello,
->  world.
```

---

`console.log()` accepts an unlimited number of arguments, and concatenates them together, separated by a space.

```js
const world = "world.";
console.log("hello,", world);
-> hello, world.
```

---

`console.log()` works with template literals.

```js
const world = "world";
console.log(`hello ${world}.`);
```

---

If an argument to `console.log()` is not a string, it is converted to a string using `String(arg)`.

```js
console.log("Time now:", new Date);
```

---

When logging to xsbug, the xsbug log pane colorizes log lines output with `console.error()`, `console.warn()`, and `console.info()`.

```js
console.error("Error!");
console.warn("Warning!");
console.info("Info!");
```
