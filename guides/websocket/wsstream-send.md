---
name: Send Message using WebSocketStream
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Get a writer from the `writable` stream and use `write()` to transmit messages. Strings are sent as text messages.

```js
import WebSocketStream from "web/websocketstream";

const wsStream = new WebSocketStream(
		"ws://example.com/ws");
const {writable} = await wsStream.opened;
const writer = writable.getWriter();
await writer.write("This is a message.");
await writer.write("This is another message. ❤️");
```

---

To transmit binary messages, pass an `ArrayBuffer`, `DataView`, or `TypedArray` to `write()`.

```js
import WebSocketStream from "web/websocketstream";

const wsStream = new WebSocketStream(
		"ws://example.com/ws");
const {writable} = await wsStream.opened;
const writer = writable.getWriter();
await writer.write(new ArrayBuffer(20));
await writer.write(Uint8Array.of(1, 2, 3));
await writer.write(
	new DataView(new ArrayBuffer(10)));
```

---

The promise returned by `write()` resolves when the message has been transmitted. Awaiting it applies backpressure, so messages larger than the available network buffers are sent in fragments rather than held in memory.

This example sends several 1 KB messages, waiting for each to be sent before queuing the next.

```js
import WebSocketStream from "web/websocketstream";

const wsStream = new WebSocketStream(
		"ws://example.com/ws");
const {writable} = await wsStream.opened;
const writer = writable.getWriter();
const data = new Uint8Array(1024);
for (let i = 0; i < 4; i++)
	await writer.write(data);
```

---

Because `writable` is a `WritableStream`, another stream can be piped to it. This example sends each value enqueued by a readable stream as a JSON encoded message.

```js
import WebSocketStream from "web/websocketstream";
import { ReadableStream } from "web/streams";
import Timer from "timer";

const sensor = new device.sensor.IMU({});
const samples = new ReadableStream({
	start(controller) {
		Timer.repeat(() => controller.enqueue(
			JSON.stringify(sensor.sample())), 100);
	}
});

const wsStream = new WebSocketStream(
		"ws://example.com/ws");
const {writable} = await wsStream.opened;
samples.pipeTo(writable);
```
