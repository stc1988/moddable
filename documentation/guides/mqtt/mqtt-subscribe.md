---
name: Subscribe to Topic using MQTT()
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Use `subscribe()` to subscribe to a topic. 

If a subscription is requested before the MQTT connection has been established, it is queued to be sent once the connection is ready.

```js
import * as mqtt from "mqtt/js";

const mc = mqtt.connect("mqtt://broker.hivemq.com");
mc.subscribe("xs/test");
```
---

To subscribe to a topic only after the connection is established, wait for the `connect` event.

```js
import * as mqtt from "mqtt/js";

const mc = mqtt.connect("mqtt://broker.hivemq.com");
mc.addEventListener("connect", () => {
	mc.subscribe("xs/test");
});
```
---

To receive a notification when the subscription completes, pass a callback.

```js
import * as mqtt from "mqtt/js";

const mc = mqtt.connect("mqtt://broker.hivemq.com");
mc.subscribe("xs/test", error => {
	if (error)
		trace("subscribe failed\n");
	else
		trace("subscribe succeeded\n");
});
```

---

To unsubscribe from a topic, use `unsubscribe()`.

Like `subscribe()`, unsubscribe requests made before the connection is established are queued and a callback may be passed to receive a notification when unsubscribe completes.

```js
import * as mqtt from "mqtt/js";

const mc = mqtt.connect("mqtt://broker.hivemq.com");
mc.subscribe("xs/test");
mc.unsubscribe("xs/test");
```
