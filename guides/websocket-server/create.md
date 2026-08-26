---
name: Create WebSocket Server Endpoint
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-26
---

To create a WebSocket Server endpoint, use the HTTP server's `onRoute()` callback to bind to a path. The WebSocket Handshake route implements the WebSocket handshake.

You can have several different WebSocket endpoints on a single HTTP Server.

This example does not attach the connection to a WebSocket client. See [Accept WebSocket Request](./accept.md) for that.

```js
import WebSocketHandshake from "embedded:network/http/server/route/ws/handshake";

const server = new device.network.http.server.io({
	...device.network.http.server,
	onRoute(request) {
		if ("GET" !== request.method)
			return;

		if ("/ws" === request.path) {
			return {
				...WebSocketHandshake, 
				onDone() {
					// attach connection to ws client
				}
			};
		}
	}
});
```

---

In `onRoute()` you have access to the [complete HTTP request](../httpserver/route.md) which can be used to filter connections.

```js
import WebSocketHandshake from "embedded:network/http/server/route/ws/handshake";
import StaticRoute from "embedded:network/http/server/route/static";

const server = new device.network.http.server.io({
	...device.network.http.server,
	onRoute(request) {
		if ("GET" !== request.method)
			return;

		if ("/ws" === request.path) {
			if ("secret" !== request.headers.get("auth-token")) {
				return {
					...StaticRoute,
					status: 401,
					data: "invalid auth-token"
				}
			}

			return {
				...WebSocketHandshake, 
				onDone() {
					// attach connection to ws client
				}
			};
		}
	}
});
```