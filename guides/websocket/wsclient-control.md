---
name: Control Messages using WebSocket Client
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

The WebSocket protocol uses control messages for communication that is separate from the messages sent and received. The most common control message is [`close`](./wsclient-close.md) which initiates a clean shutdown of the connection.

---

`ping` and `pong` are control messages to check if the connection is still active. You send a `ping` using `write()` and receive the `pong` response in `onControl()`.


```js
const WebSocketClient = device.network.ws.io;
const wsc = new WebSocketClient({
	...device.network.ws,
	host: "example.com",
	path: "/ws",
	onWritable(count) {
		if (!this.once) {
			this.once = true;
			this.write(ArrayBuffer.fromString("ping!!"),
				{opcode: WebSocketClient.ping});
		}
	},
	onControl(opcode, data) {
		if (WebSocketClient.pong === opcode)
			trace(`Pong\n`);
	}
});
```

---

If you receive a `ping` from the server, `onControl()` is invoked to let you know the server sent a keep-alive. But, you do not need to send a response because the WebSocket Client sends the `pong` response required by the protocol.

```js
const WebSocketClient = device.network.ws.io;
const wsc = new WebSocketClient({
	...device.network.ws,
	host: "example.com",
	path: "/ws",
	onControl(opcode, data) {
		if (WebSocketClient.ping === opcode)
			trace(`Ping\n`);
	}
});
```
