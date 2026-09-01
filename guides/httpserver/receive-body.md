---
name: Receive Request Body
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-31
---

To receive the request body, you must implement a route handler with an `onReadable` callback. The request body is delivered in fragments so the handler must reassemble it, if needed. This example receives the body into a resizable `ArrayBuffer` which is converted to text and parsed as JSON. Naturally, reassembling fragments fails if there is not enough memory.

If you do not provide `onReadable`, the request body, if any, is ignored.

This example sets the `"content-length"` header to `0` to indicate an empty response body. This header must be set for an empty response because the absence of both `"content-length"` and `"transfer-encoding"` headers indicates an HTTP response that ends when the connection closes.

```js
const server = new device.network.http.server.io({
	...device.network.http.server,
	onRoute(request) {
		if ("PUT" !== request.method)
			return;

		const mime = request.headers.get("content-type");
		if (("/json" === request.path) &&
			("application/json" === mime))
			return jsonReceiveRoute;
	}
});

const jsonReceiveRoute = {
	onRequest(request) {
		this.body = new Uint8Array(
			new ArrayBuffer(0, {maxByteLength: 1024 * 1024}));
	},
	onReadable(count) {
		const buffer = this.read(count);
		const bytes = new Uint8Array(buffer);
		this.body.buffer.resize(
			this.body.length + count);
		this.body.set(bytes, this.body.length - count);
	},
	onResponse(response) {
		response.headers.set("content-length", 0);
		try {
			const text = String.fromArrayBuffer(this.body.buffer);
			const json = JSON.parse(text);
			// json available to construct response
		}
		catch {
			response.status = 400;	// bad request
		}
		this.respond(response);
	}
};
```

---

You can receive a URL encoded body from a form using a handler similar to the `jsonReceiveRoute` above.

```js
import { URLSearchParams } from "url";

const server = new device.network.http.server.io({
	...device.network.http.server,
	onRoute(request) {
		if ("POST" !== request.method)
			return;

		const mime = request.headers.get("content-type");
		if (("/form" === request.path) &&
			("application/x-www-form-urlencoded" === mime))
			return formReceiveRoute;
	}
});

const formReceiveRoute = {
	onRequest(request) {
		this.body = new Uint8Array(
			new ArrayBuffer(0, {maxByteLength: 1024 * 1024}));
	},
	onReadable(count) {
		const buffer = this.read(count);
		const bytes = new Uint8Array(buffer);
		this.body.buffer.resize(
			this.body.length + count);
		this.body.set(bytes, this.body.length - count);
	},
	onResponse(response) {
		response.headers.set("content-length", 0);
		try {
			const text = String.fromArrayBuffer(this.body.buffer);
			const query = new URLSearchParams(text);
			// form query available to construct response
		}
		catch {
			response.status = 400;	// bad request
		}
		this.respond(response);
	}
};
```
