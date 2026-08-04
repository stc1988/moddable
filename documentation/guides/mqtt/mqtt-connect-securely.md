---
name: Connect Securely to MQTT Server using MQTT()
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

To make a secure connection to an MQTT server, use an `mqtts:` URL.

```js
import * as mqtt from "mqtt/js";

const mc = mqtt.connect("mqtts://broker.hivemq.com");
```

---

Note that most projects do not include any TLS certificates by default. [Include Public Certificates](../tls/certificates-public.md) explains how to set-up the certificates for your project.