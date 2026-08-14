---
name: Make Request using HTTP Client
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-14
---

HTTP Client is a low-level API for making HTTP requests. Making an HTTP request always begins with two steps:

1. Create an HTTP Client instance for the domain. In this example, the domain is "example.com".
2. Create a request on that instance for a specific path. In this example, the path is `"/"`.

The HTTP Client invokes callback functions as the request is processed. This example uses the `onReadable()` callback to receive the response data. The data is provided in an `ArrayBuffer`. `onReadable()` may be called several times for a single response body, depending on how the fragments arrive. When all fragments have been received, the `onDone()` callback is invoked. This example uses the standard [`TextDecoder`](https://developer.mozilla.org/en-US/docs/Web/API/TextDecoder) class to convert the binary data to a string that is traced to the console from `onDone()`.

The `onDone()` callback calls the `close()` method of the HTTP Client to release its resources. 

```js
const http = new device.network.http.client.io({
	...device.network.http.client.client,
	host: "example.com"
});
http.request({
	path: "/",
	onHeaders(status, headers, statusText) {
		trace(`HTTP status ${status}:${statusText}\n`);
	},
	onReadable() {
		this.decoder ??= new TextDecoder();
		this.text ??= [];
		this.text.push(
			this.decoder.decode(this.read(),{stream: true}));
	},
	onDone(error) {
		http.close();
		this.text.push(this.decoder.decode());
		trace(this.text.join(""));
	}
});
```

---

You can make several HTTP requests to the same domain using a single instance of HTTP Client. This is often faster than using a separate HTTP Client for each request, as the underlying TCP socket can be shared by all requests eliminating the overhead of establishing a new connection for each request.

Requests are issued to the HTTP server sequentially in the order they are made.

```js
const http = new device.network.http.client.io({
	...device.network.http.client,
	host: "httpbin.org"
});

const handler = {
	onReadable() {
		this.decoder ??= new TextDecoder();
		this.text ??= [];
		this.text.push(
			this.decoder.decode(this.read(), {stream: true}));
	},
	onDone(error) {
		this.text.push(this.decoder.decode());
		trace(this.text.join(""));
	}
};

http.request({
	...handler,
	path: "/encoding/utf8"
});

http.request({
	...handler,
	path: "/json"
});
```

---

You can retrieve the response as JSON by calling `JSON.parse()` after receiving the complete response.

```js
const http = new device.network.http.client.io({
	...device.network.http.client,
	host: "httpbin.org"
});
http.request({
	path: "/json",
	onReadable() {
		this.decoder ??= new TextDecoder();
		this.text ??= [];
		this.text.push(
			this.decoder.decode(this.read(), {stream: true}));
	},
	onDone(error) {
		http.close();
		this.text.push(this.decoder.decode());
		const json = JSON.parse(this.text.join(""));
		trace(JSON.stringify(json), "\n");
	}
});
```

---

Retrieve the response as a `Uint8Array` by concatenating response body fragments.

```js
const http = new device.network.http.client.io({
	...device.network.http.client,
	host: "httpbin.org"
});
http.request({
	path: "/encoding/utf8",
	onReadable(count) {
		this.bytes ??= new Uint8Array(new ArrayBuffer(0,
				{maxByteLength: 8 * 1024 * 1024}));
		const length = this.bytes.length;
		this.bytes.buffer.resize(length + count);
		this.read(this.bytes.subarray(length, length + count))
	},
	onDone(error) {
		http.close();
		// this.bytes is the full response as Uint8Array
	}
});
```

---

The HTTP request method defaults to `"GET"`. Set the request method using the `method` option.

```js
const http = new device.network.http.client.io({
	...device.network.http.client,
	host: "example.com"
});
http.request({
	path: "/",
	method: "DELETE",
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
