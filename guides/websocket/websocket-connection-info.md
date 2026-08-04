---
name: Get Connection Information using WebSocket()
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

The `readyState` property indicates the current state of the WebSocket connection.

```js
import WebSocket from "WebSocket";

const ws = new WebSocket("ws://example.com/ws");
switch (ws.readyState) {
	case WebSocket.CONNECTING:
		trace(`Connecting\n`);
		break;
	case WebSocket.OPEN:
		trace(`Open\n`);
		break;
	case WebSocket.CLOSING:
		trace(`Closing\n`);
		break;
	case WebSocket.CLOSED:
		trace(`Closed\n`);
		break;
}
```

---

The `bufferedAmount` property indicates the number of bytes that are currently enqueued by `send()` and waiting to be transmitted.

```js
import WebSocket from "WebSocket";

const ws = new WebSocket("ws://example.com/ws");
ws.send("this is a test".repeat(500));
trace(`bufferedAmount ${ws.bufferedAmount}\n`);
```

---

The `protocol` property indicates the protocol selected by the server from the options passed to the constructor. The `protocol` is known once the WebSocket handshake completes which is signaled by the `open` event.

```js
import WebSocket from "WebSocket";

const ws = new WebSocket("ws://example.com/ws", ["mqtt", "soap"]);
ws.addEventListener("open", event => {
	trace(`protocol ${ws.protocol}\n`);
});
```
