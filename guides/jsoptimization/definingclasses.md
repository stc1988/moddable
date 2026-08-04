---
name: Define Class Methods
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

[Arrow functions](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Functions/Arrow_functions) are a popular tool for JavaScript developers. Not only do they solve the vexing `this` binding challenge, but they do so using a concise syntax. As a result, developers sometimes use arrow functions where there is a more efficient alternative. 

---

One particularly ill-advised place to use arrow functions is in defining the methods of a class. Using arrow functions here puts the methods on each instance of `Example`. If there are 10 instances of Example, that's 30 unnecessary function instances taking up RAM. Those unnecessary function instances are created each time the class is instantiated, so they also increase execution time.

Using the standard class syntax instead puts the functions on the prototype of `Example` so they are shared by all instances.

```js
/* BEFORE */
class Example {
	begin = options => {
		// code here
	}
	send = buffer => {
		// code here
	}
	end = () => {
		// code here
	}
}

/* AFTER */
class Example {
	begin(options) {
		// code here
	}
	send(buffer) {
		// code here
	}
	end() {
		// code here
	}
}
```
