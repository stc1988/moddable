---
name: Make Secure Request using HTTP Client
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-14
---

To make a secure HTTPS request using the HTTP Client, simply change both occurrences of `device.network.http.client` to `device.network.https.client`. There are no changes to calling `request()`.

```js
const http = new device.network.https.client.io({ 
	...device.network.https.client,
	host: "example.com"
});
http.request({
	path: "/",
	onReadable() {
		this.decoder ??= new TextDecoder();
		this.text ??= [];
		this.text.push(
			this.decoder.decode(this.read(), {stream: true}));
	},
	onDone(error) {
		http.close();
		this.text.push(this.decoder.decode());
		trace(this.text.join(""));
	}
});
```
---

Note that most projects do not include any TLS certificates by default. [Include Public Certificates](../tls/certificates-public.md) explains how to set-up the certificates for your project. [Include Private Certificates](../tls/certificates-private.md) describes how to work with private certificates.
