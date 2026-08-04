---
name: Connect
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Connect to server that supports Server-Sent Events by passing the URL to its HTTP or HTTPS endpoint.

```js
import EventSource from "eventsource";

const src = new EventSource("http://www.example.com/sse");
```
---

Set the HTTP request method, add request headers, and provide a request body by passing them in the constructor's optional options object. The request body is only sent for `PUT` and `POST` request methods. These properties are extensions provided by the Moddable SDK.

The `withCredentials` option is unimplemented because CORS is not applicable.

```js
import EventSource from "eventsource";

const src = new EventSource("http://www.example.com/sse", {
	method: "POST",
	headers: new Headers([["Content-Type", "application/json"]]),
	body: JSON.stringify({info: 1})
});
```
---

To send binary data for the request body, pass an `ArrayBuffer` for the body.

```js
import EventSource from "eventsource";

const src = new EventSource("http://www.example.com/sse", {
	method: "PUT",
	headers: new Headers([["Content-Type",
					"application/octet-stream"]]),
	body: new ArrayBuffer(128)
});
```
---

Listen for the `open` event to be notified when the initial connection is established.

EventSource automatically attempts to reconnect after a remote disconnect. The `open` event also fires on each successful reconnect.

```js
import EventSource from "eventsource";

const src = new EventSource("http://www.example.com/sse");
src.addEventListener("open", event => {
	trace("SSE connection established\n");
});
```
