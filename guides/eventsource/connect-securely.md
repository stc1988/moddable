---
name: Connect Securely
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

To make a secure connection to a server that supports Server-Sent Events, use an `https:` URL.

```js
import EventSource from "eventsource";

const src = new EventSource("https://www.example.com/sse");
```

---

Note that most projects do not include any TLS certificates by default. [Include Public Certificates](../tls/certificates-public.md) explains how to set-up the certificates for your project.