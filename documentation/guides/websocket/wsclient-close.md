---
name: Close Connection using WebSocket Client
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Closing a WebSocket connection is a multi-step process. To start to close, send a `close` control message.

The buffer passed to `write()` as part of the control message contains the [status code](https://www.rfc-editor.org/info/rfc6455/#section-7.4.1).

When the close process completes, `onClose()` is invoked.

Calling the `close()` method immediately terminates the connection, rather than initiating a clean disconnect.

```js
const WebSocketClient = device.network.ws.io;
const wsc = new WebSocketClient({
	...device.network.ws,
	host: "example.com",
	path: "/ws",
	onWritable(count) {
		if (!this.once) {
			this.once = true;
			trace("Starting close.\n");
			this.write(Uint8Array.of(1001 >> 8, 1001).buffer,
				{opcode: WebSocketClient.close});
		}
	},
	onClose() {
		trace("Connection closed.\n");
	}
});
```

---

When the server closes the connection, the client receives a `close` control message. The message includes the [status code](https://www.rfc-editor.org/info/rfc6455/#section-7.4.1) and reason string. 

When the multi-step close process completes, `onClose` is invoked.

```js
const WebSocketClient = device.network.ws.io;
const wsc = new WebSocketClient({
	...device.network.ws,
	host: "example.com",
	path: "/ws",
	onControl(opcode, data) {
		if (WebSocketClient.close !== opcode)
			return;

		trace("Connection closing.\n");
		const bytes = new Uint8Array(data);
		const code = (bytes[0] << 8) | bytes[1];
		const reason = String.fromArrayBuffer(
				bytes.buffer.slice(2));
		trace(`  ${code}:${reason}\n`);
	},
	onClose() {
		trace("Connection closed.\n");
	}
});
```
