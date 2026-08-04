---
name: Connect Securely to MQTT Server using MQTT Client
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

To make a secure MQTT connection using the MQTT Client, simply change both occurrences of `device.network.mqtt` to `device.network.mqtts`. There are no other changes.

```js
new device.network.mqtts.io({
	...device.network.mqtts,
	host: "test.mosquitto.org",
	onWritable(count) {
		if (this.once) return;
		this.once = true;
		trace("Connected\n");
	}
});
```

---

Note that most projects do not include any TLS certificates by default. [Include Public Certificates](../tls/certificates-public.md) explains how to set-up the certificates for your project. [Include Private Certificates](../tls/certificates-private.md) describes how to work with private certificates.
