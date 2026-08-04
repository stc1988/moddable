---
name: Connect Securely to Server using WebSocket Client
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

To connect securely to a WebSocket server using WebSocket Client, simply change both occurrences of `device.network.ws` to `device.network.wss`. There are no other changes.

```js
const wsc = new device.network.wss.io({
	...device.network.wss,
	host: "example.com",
	path: "/ws",
	onWritable(count) {
		if (!this.once) {
			this.once = true;
			trace("Connected\n");
		}
	}
});
```

---

Note that most projects do not include any TLS certificates by default. [Include Public Certificates](../tls/certificates-public.md) explains how to set-up the certificates for your project. [Include Private Certificates](../tls/certificates-private.md) describes how to work with private certificates.
