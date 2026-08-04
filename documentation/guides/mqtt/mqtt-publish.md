---
name: Publish Message using MQTT()
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Use `publish()` to publish a message. The first argument is the topic and the second argument is the payload as a string, `ArrayBuffer`, `TypedArray`, or `DataView`.

If a message is published before the MQTT connection has been established, it is queued to be sent once the connection is ready.

```js
import * as mqtt from "mqtt/js";

const mc = mqtt.connect("mqtt://broker.hivemq.com");
mc.publish("xs/test", "message");
mc.publish("xs/test", Uint8Array.of(1,2,3));
```

---

To publish a message only after the connection is established, wait for the `connect` event.

```js
import * as mqtt from "mqtt/js";

const mc = mqtt.connect("mqtt://broker.hivemq.com");
mc.addEventListener("connect", () => {
	mc.publish("xs/test", "message");
	mc.publish("xs/test", Uint8Array.of(1,2,3));
});
```

---

Set the quality of service, retain flag, and duplicate flag using the optional options object.

```js
import * as mqtt from "mqtt/js";

const mc = mqtt.connect("mqtt://broker.hivemq.com");
mc.publish("xs/test", "message", {
	qos: 1,
	retain: true,
	dup: false
});
```

---

To receive a notification when the message has been published, pass a callback. The callback must come after the optional options object, when present.

```js
import * as mqtt from "mqtt/js";

const mc = mqtt.connect("mqtt://broker.hivemq.com");
mc.publish("xs/test", "message", {
	qos: 1,
}, error => {
	if (error)
		trace("publish failed\n");
	else
		trace("publish succeeded\n");
});
```
