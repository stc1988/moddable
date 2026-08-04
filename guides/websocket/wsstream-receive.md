---
name: Receive Message using WebSocketStream
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Get a reader from the `readable` stream and use `read()` to receive messages. The value is a string or a `Uint8Array`, depending on whether the server sent a text or binary message. Each read delivers one complete message.

The `done` property is `true` once the server closes the connection.

```js
import WebSocketStream from "web/websocketstream";

const wsStream = new WebSocketStream(
		"ws://example.com/ws");
const {readable} = await wsStream.opened;
const reader = readable.getReader();
while (true) {
	const {value, done} = await reader.read();
	if (done)
		break;

	trace(`message received\n`);
	if ("string" === typeof value)
		trace(`  string: ${value}\n`);
	else
		trace(`  binary: ${value.toHex()}\n`);
}
```

---

Because `readable` is a `ReadableStream`, messages may be received with a `for await` loop.

```js
import WebSocketStream from "web/websocketstream";

const wsStream = new WebSocketStream(
		"ws://example.com/ws");
const {readable} = await wsStream.opened;
for await (const value of readable)
	trace(`message received: ${value}\n`);
```

---

If the received messages are JSON text,  a `TransformStream` may be used to convert them to objects. 

```js
import WebSocketStream from "web/websocketstream";
import { TransformStream } from "web/streams";

const wsStream = new WebSocketStream(
		"ws://example.com/ws");
const {readable} = await wsStream.opened;
const parseJSON = new TransformStream({
	transform(chunk, controller) {
		controller.enqueue(JSON.parse(chunk));
	}
});
for await (const msg of readable.pipeThrough(parseJSON))
	trace(`${JSON.stringify(msg)}\n`);
```

If a message is not valid JSON, `JSON.parse()` throws, which errors the stream and exits the loop.
