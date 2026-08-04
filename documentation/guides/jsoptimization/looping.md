---
name: Loop through an Array
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

A common way to iterate over the items in an array is using a `for` loop. Each pass through the loop requires that `array.length` be re-evaluated because the code inside the loop might modify `array`.

The overhead of re-evaluating `array.length` on each pass through the loop is minimal, it does eventually add up, especially when `array.length` is large. Accessing local variables is very fast, so a more efficient way to write this loop is to move `array.length` to a local variable. Of course, you can only do this if you know that the code inside the loop does not modify length of the array.


```js
/* BEFORE */
for (let i = 0; i < array.length; i++) {
	/* code here */
}

/* AFTER */
for (let i = 0, length = array.length; i < length; i++) {
	/* code here */
}
```

---

Another common way to iterate over the items in an array is using the [`forEach()`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/forEach) method or one of its cousins like [`some()`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/some) and [`every()`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/every). These methods are convenient, but they require the overhead of a function call for each item visited in the array. In addition to taking some time, that function call also requires an additional frame on the JavaScript stack which can lead to stack overflows. The risk of this increases when they are nested.

Don't hesitate to use these methods to make your code more readable and reliable, but be aware that they do have a cost. Based on your runtime experience, it may eventually make sense to replace them with a simple `for` loop.


```js
/* BEFORE */
let total = 0;
array.forEach(value => total += value)

/* AFTER */
let total = 0;
for (let i = 0, length = array.length; i < length; i++)
	total += array[i];

/* BEFORE */
let allEven = array.every(value => 0 == (value % 2));

/* AFTER */
let allEven = true;
for (let i = 0, length = array.length; i < length; i++) {
	if (array[i] % 2) {
		allEven = false;
		break;
	}
}
```
