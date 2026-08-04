---
name: Close Connection using WebSocketStream
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Use the `close()` method to disconnect from the WebSocket server. The connection is closed with status code 1000, a normal closure. Closing requires an open connection, so wait for `opened` before calling `close()`.

```js
import WebSocketStream from "web/websocketstream";

const wsStream = new WebSocketStream(
		"ws://example.com/ws");
await wsStream.opened;
wsStream.close();
```

---

Indicate the reason for closing by passing an object with `closeCode` and `reason` properties. The [status code](https://www.rfc-editor.org/info/rfc6455/#section-7.4.1) must be 1000 or in the range 3000 to 4999. When `closeCode` is provided, `reason` is required and must be no longer than 123 bytes when encoded as UTF-8.

```js
import WebSocketStream from "web/websocketstream";

const wsStream = new WebSocketStream(
		"ws://example.com/ws");
await wsStream.opened;
wsStream.close({closeCode: 4000, reason: "sensor offline"});
```

---

Closing a WebSocket connection is a multi-step process. The `closed` promise resolves when the close completes, whether the close was started by the client or the server. It resolves to the [status code](https://www.rfc-editor.org/info/rfc6455/#section-7.4.1) and reason string sent by the server.

```js
import WebSocketStream from "web/websocketstream";

const wsStream = new WebSocketStream(
		"ws://example.com/ws");
await wsStream.opened;
const {closeCode, reason} = await wsStream.closed;
trace(`remote close\n`);
trace(`  status code ${closeCode}\n`);
trace(`  reason ${reason}\n`);
```

---

Closing or canceling the readable or writable streams closes the WebSocket connection. Calling `close()` on the writer of the `writable` stream closes with status code 4001, aborting it closes with 4002; canceling the `readable` stream closes with 4000.

```js
import WebSocketStream from "web/websocketstream";

const wsStream = new WebSocketStream(
		"ws://example.com/ws");
const {writable} = await wsStream.opened;
const writer = writable.getWriter();
await writer.write("last message");
await writer.close();
```
