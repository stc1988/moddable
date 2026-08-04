---
name: Connect Securely to Server using WebSocketStream
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

To make a secure WebSocket connection, change the URL scheme from "ws:" to "wss:". There are no other changes.

```js
import WebSocketStream from "web/websocketstream";

const wsStream = new WebSocketStream(
		"wss://example.com/ws");
const {readable, writable} = await wsStream.opened;
```

---

Note that most projects do not include any TLS certificates by default. [Include Public Certificates](../tls/certificates-public.md) explains how to set-up the certificates for your project.
