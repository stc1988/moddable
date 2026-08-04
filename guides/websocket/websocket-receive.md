---
name: Receive Message using WebSocket()
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

When the WebSocket client receives a message, it fires a `"message"` event. The message data is a string or `ArrayBuffer`, based on what the server sent.

```js
import WebSocket from "WebSocket";

const ws = new WebSocket("ws://example.com/ws");
ws.addEventListener("message", event => {
	trace(`message received\n`);
	if ("string" === typeof event.data)
		trace(`  string: ${event.data}\n`);
	else {
		const bytes = new Uint8Array(event.data);
		trace(`  binary: ${bytes.toHex()}\n`);
	}
});
```

---

If the received messages are JSON text, convert them to objects using `JSON.parse()`.

```js
import WebSocket from "WebSocket";

const ws = new WebSocket("ws://example.com/ws");
ws.addEventListener("message", event => {
	const msg = JSON.parse(event.data);
	trace(`${JSON.stringify(msg)}\n`);
});
```
