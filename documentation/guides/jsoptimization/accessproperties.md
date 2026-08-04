---
name: Accessing Properties
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

The most direct way to access a property is by name: `object.foo`. An alternative is to write `object["foo"]`. These are equivalent, but the form `object.foo` is more efficient. That's because when you use the `object["foo"]` the JavaScript may need to convert the string to an internal XS identifier at runtime.

If the value between braces (`[]`) is a constant that is a valid JavaScript property name, use the direct form.

---

The most common use of the `object["foo"]` form is in with a [for-in loop](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Statements/for...in). In this case the code using the `[]` notation doesn't know the name of the property, so it cannot use the more direct `object.foo` form. Another reason is because a property name cannot be expressed using the direct form, for example because it is a number or contains a space.

```js
for (let name in obj) {
	const value = obj[name];
	trace(`${name} = ${value}\n`);
}

let v1 = obj[12];
let v2 = obj["a property"];
```

---

**Note**: Often when code uses `[]` notation to access properties, it is a sign that a `Map` may be a [better choice](./maps.md).
