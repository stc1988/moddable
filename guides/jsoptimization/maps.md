---
name: Map versus Object
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

You can use both [Objects](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object) and [Maps](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Map) for collections of items indexed by a key.

Maps are relatively new to JavaScript (over 10 years now!) so it is still common to see Objects used as a map.

```js
// using Object
let obj = new Object();
obj["one"] = 1;
obj["two"] = 2;
obj["three"] = 3;

// using Map
let map = new Map();
map.set("one", 1);
map.set("two", 2);
map.set("three", 3);
```

---

Maps are specified to take a more-or-less constant time to access an element. That means that adding, looking up, and deleting items in a map should always take a relatively short period of time. Objects have no such requirement. This means that in Embedded JavaScript you will likely get more predicable performance from a Map than an object. But, to provide this performance guarantee maps use a bit more memory per element than an object.

Which should you use? If the number of items is relatively small, say a few dozen, use whatever you are most comfortable coding. If the number of items may be large, a Map is probably your best bet. As always, to be certain measure the performance and memory using both.

---

For a collection of items that is **not** indexed by a key, scripts commonly use `Array`. [`Set`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Set) is a good alternative. The tradeoffs are similar to using a `Map` instead of an `Object`. Another benefit of `Set` is that removing an item requires less code.


```js
// Using Array
let array = [];
array.push("one", "two", "three");

let index = array.indexOf("two");
if (index >= 0)
	array.splice(index, 1); 
	
let hasThree = array.includes("three");
// => hasThree === true

// Using Set
let set = new Set();
set.add("one");
set.add("two");
set.add("three");

set.delete("two");

let hasThree = set.has("three");
// => hasThree === true
```
