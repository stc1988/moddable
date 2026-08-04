---
name: Receive
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

To receive messages, listen for `message` events. 

Note that `message` events are untyped Server-Sent Events. For typed Server-Sent Events, see the next example.

```js
import EventSource from "eventsource";

const src = new EventSource("http://www.example.com/sse");
src.addEventListener("message", event => {
	trace(`message lastEventID ${event.lastEventId}\n`);
	trace(`  data: ${event.data}\n`);
});
```

---

To receive messages of a specific type, listen for events of that type.

```js
import EventSource from "eventsource";

const src = new EventSource("http://www.example.com/sse");
src.addEventListener("ping", event => {
	trace(`ping lastEventID ${event.lastEventId}\n`);
	trace(`  data: ${event.data}\n`);
});
```
