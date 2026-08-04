---
name: Close Connection using MQTT()
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Use the `end()` method to begin the process of disconnecting from the MQTT server.

```js
import * as mqtt from "mqtt/js";

const mc = mqtt.connect("mqtt://broker.hivemq.com");
mc.end();
```

---

To begin the close immediately without waiting for any inflight messages to complete, set the `force` argument to `true`.

```js
import * as mqtt from "mqtt/js";

const mc = mqtt.connect("mqtt://broker.hivemq.com");
mc.end(true);
```

---

Provide a callback to be notified when close completes. If the optional force argument is passed to `end()`, it must be first.

```js
import * as mqtt from "mqtt/js";

const mc = mqtt.connect("mqtt://broker.hivemq.com");
mc.end(false, () => {
	trace("mqtt connection close complete\n");
});
```

---

If the server closes the connection, the client receives a `close` event. If the connection is lost (e.g. a networking error), the client receives an `error` event. 

You may use `on()` instead of `addEventListener()`.

```js
import * as mqtt from "mqtt/js";

const mc = mqtt.connect("mqtt://broker.hivemq.com");
mc.addEventListener("close", () => {
	trace("mqtt connection remote close\n");
});
mc.addEventListener("error", () => {
	trace("mqtt connection lost\n");
});
```
