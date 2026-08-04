---
name: Receive Messages using MQTT()
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

To receive MQTT messages, first [subscribe](./mqtt-subscribe.md) to one or more topics. Then listen for `message` events. The message payload is an `ArrayBuffer`.

For compatibility, the `toString()` method of the returned `ArrayBuffer` is overridden to convert the buffer to a JavaScript string.

```js
import * as mqtt from "mqtt/js";

const mc = mqtt.connect("mqtt://broker.hivemq.com");
mc.subscribe("xs/test");
mc.addEventListener("message", (topic, message) => {
	trace(`${topic}: ${message}\n`);
});
```
---

[MQTT.js](https://github.com/mqttjs/MQTT.js#example), the npm package that Moddable's MQTT() emulates, delivers messages as a Node.js [`Buffer`](https://nodejs.org/api/buffer.html), rather than a standard JavaScript `ArrayBuffer`. If your code runs with both the Moddable and npm implementations of MQTT(), you can check the payload type.

```js
import * as mqtt from "mqtt/js";

const mc = mqtt.connect("mqtt://broker.hivemq.com");
mc.subscribe("xs/test");
mc.addEventListener("message", (topic, message) => {
	if (message instanceof ArrayBuffer) {
		const bytes = new Uint8Array(message)
		trace(`${topic}: ${bytes.toHex()}\n`);
	}
	else
		trace(`${topic}: ${message.toString("hex")}\n`);
});
```
