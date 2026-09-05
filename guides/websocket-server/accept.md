---
name: Accept WebSocket Request
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-26
---

When the WebSocket Handshake route has successfully completed, it calls `onDone()`. At this time, the connection is ready to begin communicating using the WebSocket protocol, but it has not yet begun. To do that, the connection is handed off to a WebSocket client. Once the hand off is complete, the connection is owned by the WebSocket client and the server no longer references it.

This example shows handing the connection off to the Web platform's `WebSocket()`. The next example shows handing off to ECMA-419's WebSocket Client.

```js
import WebSocketHandshake
	from "embedded:network/http/server/route/ws/handshake";
import WebSocket from "WebSocket";

const server = new device.network.http.server.io({
	...device.network.http.server,
	onRoute(request) {
		if ("GET" !== request.method)
			return;

		if ("/ws" === request.path)
			return websocketRoute;
	}
});

const websocketRoute = {
	...WebSocketHandshake,
	onDone() {
		const ws = new WebSocket({
			attach: this.detach()});
		ws.addEventListener("open", () => {
			ws.send("Hello, WebSocket.");
		});
		ws.addEventListener("message", event => {
			const data = event.data;
			trace(`onmessage ${data}\n`);
		});
	}
};
```

---

The preceding example can be changed to use the ECMA-419 WebSocket Client instead by modifying the `websocketRoute`.

See the [WebSocket client Guide](../websocket/index.md) for information on using the ECMA-419 WebSocket Client.

```js
import WebSocketClient
	from "embedded:network/websocket/client";
const websocketRoute = {
	...WebSocketHandshake,
	onDone() {
		const ws = new WebSocketClient({
			attach: this.detach(),
			onReadable(count) {
				// etc
			},
			onWritable(count) {
				// etc
			}
		});
	}
};
```
