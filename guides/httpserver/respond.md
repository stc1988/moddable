---
name: Respond to Request
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-25
---

If your complete response is available from `onRoute()`, you can use the static route module to reply. Your response can include HTTP headers and the HTTP status. If the `status` is omitted, it defaults to 200.

```js
import StaticRoute from "embedded:network/http/server/route/static";

const server = new device.network.http.server.io({
	...device.network.http.server,
	onRoute(request) {
		if (("GET" === request.method) &&
			("/" === request.path)) {
			return {
				...StaticRoute,
				data: "<h1>Hello</h1>",
				status: 200,
				contentType: "text/html"
			}
		}
	}
});
```

---

A static route also accepts binary data as the response body.

This example uses the `headers` property to set the response headers.

```js
import StaticRoute from "embedded:network/http/server/route/static";
import Headers from "headers";

const server = new device.network.http.server.io({
	...device.network.http.server,
	onRoute(request) {
		if (("GET" === request.method) &&
			("/rng" === request.path)) {
			const randomBytes = new Uint8Array(16);
			for (let i = 0; i < 16; i++)
				randomBytes[i] = Math.irandom(256);
			return {
				...StaticRoute,
				data: randomBytes,
				headers: new Headers([
					["content-type", "application/octet-stream"],
					["rng", "javascript"],
					["date", new Date().toUTCString()]
				])
			}
		}
	}
});
```

---

If your complete response is not available from `onRoute()`, such as when the response is dynamically generated, you can't use the `static` route. Instead, implement your own route handler.

This example route responds with a random number of letters from `a` to `z`. Notice that `onResponse()` reports the length of the complete response and `onWritable()` generates precisely the number of characters necessary to fill the output buffer.

Note that `write()` only accepts binary data such as an `ArrayBuffer` or `Uint8Array`. Text must be [converted to binary](../binary/index.md) to pass to `write()`.

```js
const server = new device.network.http.server.io({
	...device.network.http.server,
	onRoute(request) {
		if (("GET" === request.method) &&
			("/" === request.path))
			return dynamicRoute;
	}
});

const dynamicRoute = {
	onResponse(response) {
		const responseLength = 1024 + Math.irandom(8 * 1024);
		response.headers.set("content-length", responseLength);
		response.headers.set("content-type", "text/plain");
		this.respond(response);
	},
	onWritable(count) {
		const bytes = new Uint8Array(count);
		bytes[count - 1] = 10;
		for (let i = 0; i < count - 1; i++)
			bytes[i] = Math.irandom(97, 123); // a..z
		this.write(bytes);
	}
};
```

---

If you don't know the size of the response from `onRoute()`, you can implement a handler to send the response as an HTTP chunked response. This is similar to the preceding example, but you don't set the `content-length` header in `onResponse()`, you set the `transfer-encoding` header to `"chunked"`, and you call `write()` with no arguments to signal the end of the response.

```js
const server = new device.network.http.server.io({
	...device.network.http.server,
	onRoute(request) {
		if (("GET" === request.method) &&
			("/" === request.path))
			return dynamicRoute;
	}
});

const dynamicRoute = {
	onResponse(response) {
		this.remaining = 1024 + Math.irandom(8 * 1024);
		response.headers.set("content-type", "text/plain");
		response.headers.set("transfer-encoding", "chunked");
		this.respond(response);
	},
	onWritable(count) {
		if (0 === this.remaining)
			return void this.write();

		count = Math.min(this.remaining, count);
		this.remaining -= count;
		const bytes = new Uint8Array(count);
		bytes[count - 1] = 10;
		for (let i = 0; i < count - 1; i++)
			bytes[i] = Math.irandom(97, 123); // a..z
		this.write(bytes);
	}
};
```

---

You may not have the data for your response ready when `onResponse()` is called. In that case, you can call `respond()` later when the data is available. This example waits two seconds, and then calls [`fetch()`](../http/fetch-making-http-request.md) to retrieve a web page to use for the response.

```js
import Timer from "timer";
import { fetch } from "fetch";

const server = new device.network.http.server.io({
	...device.network.http.server,
	onRoute(request) {
		if (("GET" === request.method) &&
			("/" === request.path))
			return fetchRoute;
	}
});

const fetchRoute = {
	onResponse(response) {
		Timer.set(async () => {
			const fetchResponse = await fetch("http://example.com");
			const data = await fetchResponse.arrayBuffer();
			response.headers.set(
				"content-type", "text/plain");
			response.headers.set(
				"content-length", data.byteLength);
			this.bytes = new Uint8Array(data);
			this.position = 0;
			this.respond(response);
		}, 2_000);
	},
	onWritable(count) {
		this.write(this.bytes.subarray(
			this.position, this.position + count));
		this.position += count;
	}
};
```
