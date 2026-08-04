---
name: Connect Securely to Server using WebSocket()
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

To make a secure WebSocket request, change the URL scheme from "ws:" to "wss:".

```js
import WebSocket from "WebSocket";

const ws = new WebSocket("wss://example.com/ws");
```

---

Note that most projects do not include any TLS certificates by default. [Include Public Certificates](../tls/certificates-public.md) explains how to set-up the certificates for your project.