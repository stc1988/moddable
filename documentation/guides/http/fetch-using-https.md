---
name: Make Secure Request using fetch()
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

To make a secure request using `fetch()`, change the URL scheme from "http:" to "https:".

```js
import { fetch } from "fetch";

const response = await fetch("https://httpbin.org/encoding/utf8");
trace(await response.text(), "\n");
```

---

Note that most projects do not include any TLS certificates by default. [Include Public Certificates](../tls/certificates-public.md) explains how to set-up the certificates for your project.
