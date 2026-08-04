---
name: Connect to Server using WebSocket Client
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

To connect to a WebSocket server, pass the constructor an options object with the host name, endpoint path, and WebSocket protocol configuration from `device`.

When the connection has been established, `onWritable()` is invoked signaling that the connection is ready.

```js
const wsc = new device.network.ws.io({
	...device.network.ws,
	host: "example.com",
	path: "/ws",
	onWritable(count) {
		if (!this.once) {
			this.once = true;
			trace("Connected\n");
		}
	}
});
```

---

Request subprotocols with the `protocol` property and specify HTTP request headers with the `headers` property.

```js
const wsc = new device.network.ws.io({
	...device.network.ws,
	host: "example.com",
	path: "/ws",
	protocol: "mqtt,soap",
	headers: new Map([
		["custom-header", "secret"]
	]),
	onWritable(count) {
		if (!this.once) {
			this.once = true;
			trace("Connected\n");
		}
	}
});
```
