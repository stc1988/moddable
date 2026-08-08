---
name: Provide HTTP Server
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-07
---

The access point is a device on the Wi-Fi network, so it can provide network services to connected devices. 

Network services should be created after the Wi-Fi Access point is ready. These examples omit creating the access point and waiting for it to be ready in the interest of brevity. See [DNS Redirect](./dns.md) for a complete example of creating a network service after the access point is ready.

---

This example creates a simple HTTP server with a welcome page.

See the [HTTP Server Guide](../http-server/index.md) for more information.

```js
import HTTPServer from "embedded:network/http/server";
import Listener from "embedded:io/socket/listener";
import WebPage from "embedded:network/http/server/options/webpage";

const httpServer = new HTTPServer({
	io: Listener,
	port: 8080,
	onConnect(connection) {
		connection.accept({
			onRequest(request) {
				if ("/" === request.path) {
					this.route = {
						...WebPage,
						data: "Welcome!",
					}
				}
				else {
					this.route = {
						...WebPage,
						data: "Not found",
						status: 404
					}
				}
			}
		});
	}
});
```

---

The access point can use DNS Service Discovery to claim a local name and then advertise the HTTP server. This makes it easier for devices to discover the HTTP server.

See the [DNS Service Discovery Guide](../dns-sd/index.md) for more information.

```js
const dnssd = new (device.network.dnssd.io)(
			device.network.dnssd);
dnssd.claim({
	host: "accesspoint",
	onReady() {
		dnssd.advertise({
			serviceType: "_http._tcp",
			name: "419 Web Server",
			host: "accesspoint",
			port: 8080
		});
	}
});
```
