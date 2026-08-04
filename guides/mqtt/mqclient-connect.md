---
name: Connect to MQTT Server using MQTT Client
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

To connect to an MQTT server, pass the constructor an options object with the host name, endpoint path, and MQTT protocol configuration from `device`.

When the connection has been established, `onWritable()` is invoked signaling that the connection is ready.

```js
new device.network.mqtt.io({
	...device.network.mqtt,
	host: "test.mosquitto.org",
	onWritable(count) {
		if (this.once) return;
		this.once = true;
		trace("Connected\n");
	}
});
```

---

Specify the username and password using the options object argument.

```js
new device.network.mqtt.io({
	...device.network.mqtt,
	host: "test.mosquitto.org",
	user: "mqtt user",
	password: "secret"
});
```

---

Configure the keep-alive interval (in milliseconds) using the options object.

```js
new device.network.mqtt.io({
	...device.network.mqtt,
	host: "test.mosquitto.org",
	keepAlive: 6_000
});
```

---

Specify a [will](https://www.hivemq.com/blog/mqtt-essentials-part-9-last-will-and-testament/) to send when the server detects unexpected connection termination.

```js
new device.network.mqtt.io({
	...device.network.mqtt,
	host: "test.mosquitto.org",
	will: {
		topic: "service/status",
		message: ArrayBuffer.fromString("gone!"),
		QoS: 0,
		retain: false
	}
});
```
