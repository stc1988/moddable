---
name: Connect to Server using WebSocketStream
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

To connect to a WebSocket server, pass the "ws:" URL to the `WebSocketStream` constructor. The connection handshake occurs asynchronously. Await the `opened` promise to know when the connection has been established. It resolves to an object with `readable` and `writable` streams used to receive and send messages.

```js
import WebSocketStream from "web/websocketstream";

const wsStream = new WebSocketStream(
		"ws://example.com/ws");
const {readable, writable} = await wsStream.opened;
trace(`connected\n`);
```

---

To request a subprotocol be used, pass a list of acceptable subprotocols using the `protocols` option.

```js
import WebSocketStream from "web/websocketstream";

const wsStream = new WebSocketStream(
	"ws://example.com/ws", {
		protocols: ["mqtt", "soap"]
	}
);
const {readable, writable} = await wsStream.opened;
```

---

If the connection cannot be established, the `opened` promise rejects.

```js
import WebSocketStream from "web/websocketstream";

const wsStream = new WebSocketStream(
		"ws://example.com/ws");
try {
	await wsStream.opened;
	trace(`connected\n`);
}
catch {
	trace(`unable to connect\n`);
}
```

---

The `url` property is the URL passed to the constructor.

```js
import WebSocketStream from "web/websocketstream";

const wsStream = new WebSocketStream(
		"ws://example.com/ws");
trace(`${wsStream.url}\n`);
```
