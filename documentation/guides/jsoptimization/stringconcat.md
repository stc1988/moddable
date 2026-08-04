---
name: Build a String
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

When building a string from several pieces, it is common to use the addition operator to combine the pieces. This approach is simple and, for a few small strings, it is also reasonably efficient. Even more efficient is `concat()`, which eliminates memory used for intermediate results.

```js
/* BEFORE */
let s1 = "one";
let string = s1 + 2 + "three";
// => "one2three"

/* AFTER */
let s1 = "one";
let string = s1.concat(2, "three");
// => "one2three"
```
---

If you have more than a few substrings to combine, [`concat()`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/concat) may be impractical. A good alternative is creating an array of the substrings and combining them with [`join()`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/join) .

```js
let array = [];
array.push("one");
array.push(2, "three");
array.join("");
// => "one2three"
```

---

As an added bonus, if you need a separator between the substrings, `join()` makes that easy.

```js
let array = ["one", 2, "three"];
array.join("-");
// => "one-2-three"
```
---

**Note**: Using `+` to build long, complex strings is so common among web developers that the JavaScript engines in web browsers have sophisticated optimizations that make it just as fast as the techniques described above. To keep the engine size small, those optimizations aren't available to Embedded JavaScript developers. Fortunately, Embedded JavaScript developers don't tend to work with large strings often.
