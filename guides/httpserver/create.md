---
name: Create Server
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-25
---

To create an HTTP server, just instantiate it with the port number to listen on. If the `port` property is omitted, it defaults to 80.

```js
const server = new device.network.http.server.io({
	...device.network.http.server,
	port: 8080
});
```

This server has no routes, so it replies to all requests with an HTTP 404 (Not Found) error. ["Route Request"](./route.md) explains how to bind requests to handlers.

---

When you are done with the server, call its `close()` method. This closes the server's listener and immediately terminates all active connections.

```js
const server = new device.network.http.server.io({
	...device.network.http.server
});
server.close();
```

---

Sometimes you don't need the HTTP server to be available on a particular port, such as when you are advertising a service using [DNS Service Discovery](../dns-sd/index.md). In that case, set `port` to `0` and a port is assigned automatically. Check the server's `port` property to get the port assigned.

```js
const server = new device.network.http.server.io({
	...device.network.http.server,
	port: 0
});
trace(`HTTP server on port ${server.port}\n`);
```
