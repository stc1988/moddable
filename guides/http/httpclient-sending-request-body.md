---
name: Send Request Body using HTTP Client
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-14
---

Signal the presence of a request body by including the "content-length" header. Implement `onWritable()` to provide the request body in fragments.

```js
const body = ArrayBuffer.fromString("This is a test. ".repeat(500));

const http = new device.network.http.client.io({
	...device.network.http.client,
	host: "httpbin.org"
});
http.request({
	path: "/post",
	method: "POST",
	headers: new Map([["content-length", body.byteLength]]),
	onWritable(count) {
		this.offset ??= 0;
		this.write(
			body.slice(this.offset, this.offset + count));
		this.offset += count;
	},
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

Sometimes you don't know the size of the request body when the transfer begins, such as when the request body is generated dynamically. In this case, set the "transfer-encoding" header to "chunked" to indicate the presence of a request body of unknown size. Do not set the "content-length" header. Signal the end of the request body by calling `write()` with no arguments after all fragments have been written.

```js
const http = new device.network.http.client.io({
	...device.network.http.client,
	host: "httpbin.org"
});
http.request({
	path: "/post",
	method: "POST",
	headers: new Map([["transfer-encoding", "chunked"]]),
	onWritable(count) {
		if (Math.random() > 0.05)
			this.write(new ArrayBuffer(count));
		else
			this.write();
	},
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

You can call `write()` outside `onWritable()`. To know how much space is available to write, this example uses `writable` to remember the most recent `count` passed to `onWritable`. The example attempts to write each half second for ten seconds, sending as much data as possible each time.

```js
import Timer from "timer";

const http = new device.network.http.client.io({
	...device.network.http.client,
	host: "httpbin.org"
});
http.request({
	path: "/post",
	method: "POST",
	headers: new Map([["transfer-encoding", "chunked"]]),
	onWritable(count) {
		this.writable = count;

		if (undefined !== this.end)
			return;

		this.end = Date.now() + 10_000;
		Timer.repeat(id => {
			if (Date.now() >= this.end) {
				this.write();
				Timer.clear(id);
				return;
			}
			if (this.writable)
				this.write(new ArrayBuffer(this.writable));
			this.writable = 0;
		}, 500);
	},
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
