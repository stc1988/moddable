---
name: Iterate Over a String
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

The most common way to iterate through the characters in a string is with a [`for`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Statements/for) loop. This creates a new string in memory for each character which increases the load on the garbage collector. Instead of extracting a string, you can use [`charCodeAt()`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/charCodeAt) to get the Unicode value of each character.

```js
const str = "012345 áéîôÛ 😀";

/* BEFORE */
for (let i = 0, length = str.length; i < length; i++) {
	const char = str[i];
	if ("5" === char)
		trace("found five\n");
}

/* AFTER */
for (let i = 0, length = str.length; i < length; i++) {
	const charCode = str.charCodeAt(i);
	if (53 === charCode)
		trace("found five\n");
}
```

---

There is some runtime overhead in calling `charCodeAt()` for each character. An alternative is to use a [`for...of`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Statements/for...of) loop with a [string iterator](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/Symbol.iterator). For longer strings, the iterator outperforms the simple for loop and is more concise.

```js
const str = "012345 áéîôÛ 😀";

for (const char of str) {
	if ("5" === char)
		trace("found five\n");
}
```

---

**Note**: There are some subtle distinctions between how these approaches handle surrogate pairs. Often embedded developers are working with simple ASCII text, so these distinctions don't matter. For code that works with emojis and certain languages, like Chinese, the differences can be important.
