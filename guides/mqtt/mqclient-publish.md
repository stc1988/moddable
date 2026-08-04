---
name: Publish Message using MQTT Client
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Use `write()` to publish a message. The first argument is the payload as a [Byte Buffer](https://419.ecma-international.org/#byte-buffer). The second argument is an options object with details about the payload.

Messages may only be published if the connection has been established and there is sufficient output space available in the MQTT Client instance to buffer the message. This example waits for the first invocation on `onWritable()` to know it can send its initial message.

```js
const mc = new device.network.mqtt.io({
	...device.network.mqtt,
	host: "test.mosquitto.org",
	onWritable(count) {
		if (this.once) return;
		this.once = true;

		this.write(ArrayBuffer.fromString("hello"), {
			topic: "xs/test"
		});
	}
});
```

---

Set the quality of service, retain flag, and duplicate flags for the message on the options object.

```js
const mc = new device.network.mqtt.io({
	...device.network.mqtt,
	host: "test.mosquitto.org"
});
// wait for writable
mc.write(ArrayBuffer.fromString("hello"), {
	topic: "xs/test",
	QoS: 2,
	retain: true,
	duplicate: false
});
```

---

To receive a notification when the message has been published, use `onControl()` to monitor for acknowledgements. Set the message ID when publishing the message to match with the message acknowledgements.

Messages sent with QoS 0 are not acknowledged by the server. Messages sent with QoS 1 receive a `PUBACK` acknowledgement. Messages sent with QoS 2 receive both `PUBREC` and `PUBCOMP` acknowledgements.

```js
const MQTTClient = device.network.mqtt.io;
const mc = new MQTTClient({
	...device.network.mqtt,
	host: "test.mosquitto.org",
	onControl(operation, msg) {
		switch (operation) {
			case MQTTClient.PUBACK:
				trace(`PUBACK ${msg.id}\n`);
				break;
			case MQTTClient.PUBREC:
				trace(`PUBREC ${msg.id}\n`);
				break;
			case MQTTClient.PUBCOMP:
				trace(`PUBCOMP ${msg.id}\n`);
				break;
		}
	}
});
// wait for writable
mc.write(ArrayBuffer.fromString("hello"), {
	topic: "xs/test",
	QoS: 2,
	id: 5
});
```

---

A message may be published in fragments. This allows published messages that are larger than the network buffers of the device, or even bigger than the RAM of the device such as a file.

To send a message in fragments, include the `byteLength` property on the first fragment to indicate the total size of the message. After that, call `write()` as many times as needed to publish the full message. No options object is required for fragments beyond the first.

Note that fragments may only be written when there is enough buffer space. This example uses `onWritable()` to send fragments as space becomes available.

```js
let toPublish = new Uint8Array(16 * 1024);
let offset = 0;
const mc = new device.network.mqtt.io({
	...device.network.mqtt,
	host: "test.mosquitto.org",
	onWritable(count) {
		if (!toPublish) return;
		const fragment = toPublish.subarray(
					offset, offset + count);
		if (0 === offset) {
			this.write(fragment, {
				topic: "xs/frag",
				byteLength: toPublish.byteLength
			});
		}
		else
			this.write(fragment);
		offset += fragment.byteLength;
		if (offset === toPublish.byteLength)
			toPublish = null;
	}
});
```
