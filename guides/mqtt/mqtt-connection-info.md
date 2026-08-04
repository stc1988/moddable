---
name: Get Connection Information using MQTT()
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Check the `connected` and `reconnecting` properties to check the connection state.

```js
import * as mqtt from "mqtt/js";

const mc = mqtt.connect("mqtt://broker.hivemq.com");
trace(`Connected ${mc.connected}\n`);
trace(`Reconnecting ${mc.reconnecting}\n`);
```
---

Use `getLastMessageId()` to get the ID of the last message published.

```js
import * as mqtt from "mqtt/js";

const mc = mqtt.connect("mqtt://broker.hivemq.com");
mc.publish("xs/test", "message");
trace(`lastMessageId ${mc.getLastMessageId()}\n`);
```
