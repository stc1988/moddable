---
name: Receive Response Headers using HTTP Client
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

HTTP response headers are provided to the `onHeaders()` callback as a standard JavaScript [`Map`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Map).

This example traces all the headers to the console.

```js
const http = new device.network.http.io({
	...device.network.http,
	host: "example.com"
});
http.request({
	path: "/",
	onHeaders(status, headers, statusText) {
		headers.forEach(
			(value, key) => trace(`${key}: ${value}\n`));
	},
	onReadable() {
		this.read();
	},
	onDone(error) {
		http.close();
	}
});
```

---

The HTTP Client implementation must accumulate all the headers received into a `Map`. Some headers, such as cookies, can be quite large. If you are only interested in certain headers, you can provide a list of the headers you need in the `headersMask` property. This allows the HTTP Client to ignore headers not in the `headersMask`.

```js
const http = new device.network.http.io({ 
	...device.network.http,
	host: "example.com"
});
http.request({
	path: "/",
	headersMask: ["content-type"],
	onHeaders(status, headers, statusText) {
		trace(`Content-Type: ${headers.get("content-type")}\n`);
		// => "text/html"
		trace(`Date: ${headers.get("date")}\n`);
		// => undefined
	},
	onReadable() {
		this.read();
	},
	onDone(error) {
		http.close();
	}
});
```

---

Note that HTTP header names must not include uppercase letters. Use lowercase letters instead.
