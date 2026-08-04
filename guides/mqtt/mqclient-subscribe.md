---
name: Subscribe to Topic using MQTT Client
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Use `write()` to send a subscribe message. A single subscribe message may subscribe to one or more topics.

Messages may only be sent if the connection has been established and there is sufficient output space available in the MQTT Client instance to buffer the message. This example waits for the first invocation on `onWritable()` to know it can send its subscription request.

```js
const MQTTClient = device.network.mqtt.io;
new MQTTClient({
	...device.network.mqtt,
	host: "test.mosquitto.org",
	onWritable(count) {
		if (this.once) return;
		this.once = true;
		trace("Connected\n");

		this.write(null, {
			operation: MQTTClient.SUBSCRIBE,
			items: [
				{topic: "test/xs/+", QoS: 2},
				{topic: "test/419"}
			]
		});
	}
});
```
---

To unsubscribe from a topic, use `write()` to send an unsubscribe message. Like subscribe, a single message may unsubscribe from one or more topics.

```js
const MQTTClient = device.network.mqtt.io;
const mc = new MQTTClient({
	...device.network.mqtt,
	host: "test.mosquitto.org",
	onWritable(count) {
		/* sent MQTTClient.SUBSCRIBE as above */
	}
});
// some time later
mc.write(null, {
	operation: MQTTClient.UNSUBSCRIBE,
	items: [
		{topic: "test/xs/+"},
		{topic: "test/419"}
	]
});
```

---

To receive a notification when subscribe and unsubscribe requests complete, use `onControl()` to monitor for acknowledgements. Set the message ID when sending the request to match with the message acknowledgements.

```js
const MQTTClient = device.network.mqtt.io;
const mc = new MQTTClient({
	...device.network.mqtt,
	host: "test.mosquitto.org",
	onControl(operation, msg) {
		switch (operation) {
			case MQTTClient.SUBACK:
				trace(`SUBACK ${msg.id}\n`);
				break;
			case MQTTClient.UNSUBACK:
				trace(`UNSUBACK ${msg.id}\n`);
				break;
		}
	}
});
// wait for writable
mc.write(null, {
	operation: MQTTClient.SUBSCRIBE,
	items: [
		{topic: "test/419"}
	],
	id: 6
});
```
