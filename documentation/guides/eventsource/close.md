---
name: Close
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

To close the connection to the server, call `close()`.

```js
import EventSource from "eventsource";

const src = new EventSource("http://www.example.com/sse");
src.close();
```
---

To be notified when the remote endpoint closes the connection, listen for `error` events.

```js
import EventSource from "eventsource";

const src = new EventSource("http://www.example.com/sse");
src.addEventListener("error", event => {
	trace("SSE connection closed\n");
	trace(`${JSON.stringify(event)}\n`);
});
```
