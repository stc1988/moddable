---
name: Route Request
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-25
---

Before responding to a request, the server calls `onRoute()` to find the handler. `onRoute()` is code you provide. It can use the request method, requested path, and request headers to decide how to route the request.

This example responds to GET requests for the root path (`"/"`) with the welcome text. It uses the `static` route handler to reply.

If `onRoute()` does not return a value, the HTTP Server responds with HTTP 404 (not found).

```js
import StaticRoute
	from "embedded:network/http/server/route/static";

const server = new device.network.http.server.io({
	...device.network.http.server,
	onRoute(request) {
		if ("GET" !== request.method)
			return;
		
		if ("/" === request.path) {
			return {
				...StaticRoute,
				data: "Hello, root."
			};
		}
	}
});
```

---

Your `onRoute()` can use the HTTP request headers to decide how to handle the request. This example checks for an "auth-token" request header. If the header is missing, the request is rejected.

```js
import StaticRoute
	from "embedded:network/http/server/route/static";

const server = new device.network.http.server.io({
	...device.network.http.server,
	onRoute(request) {
		if (!request.headers.get("auth-token")) {
			return {
				...StaticRoute,
				status: 401,
				data: "auth-token header required"
			}
		}
		// route requests here
	}
});
```

---

Your `onRoute()` can also access the query parameters from the request path. This example checks the query for "auth-token." If it is not provided, the request is rejected.

```js
import StaticRoute
	from "embedded:network/http/server/route/static";
import { URLSearchParams } from "url";

const server = new device.network.http.server.io({
	...device.network.http.server,
	onRoute(request) {
		const query = new URLSearchParams(request.query);
		if (!query.get("auth-token")) {
			return {
				...StaticRoute,
				status: 401,
				data: "auth-token query required"
			}
		}
		// route requests here
	}
});
```

---

When debugging it can be useful to [log](../logging/index.md) the entire request. This example shows how.

```js
const server = new device.network.http.server.io({
	...device.network.http.server,
	onRoute(request) {
		trace(`Method: ${request.method}\n`);
		trace(`Path: ${request.path}\n`);
		trace(`Query: ${request.query}\n`);
		trace(`Headers:\n`);
		for (const [header, value] of request.headers)
			trace(`  ${header}: ${value}\n`);				
		// route requests here
	}
});
```
