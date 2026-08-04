---
name: Send Request Headers using HTTP Client
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Use the JavaScript [`Map`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Map) class to create a collection of HTTP headers, then pass that to `request()`.

```js
const headers = new Map([
	["content-type", "text/plain"],
	["date", Date()],
	["user-agent", "http client example"]
]);

const http = new device.network.http.io({
	...device.network.http,
	host: "httpbin.org"
});
http.request({
	path: "/encoding/utf8",
	headers,
	onReadable() {
		this.decoder ??= new TextDecoder();
		this.text ??= [];
		this.text.push(
			this.decoder.decode(this.read(), {stream: true}));
	},
	onDone(error) {
		http.close();
		this.text.push(this.decoder.decode());
		trace(this.text.join(""));
	}
});
```

---

Note that HTTP header names must not include uppercase letters. Use lowercase letters instead.
