---
name: Connection Information
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

The `readyState` property indicates the current state of the EventSource connection.

```js
import EventSource from "eventsource";

const src = new EventSource("http://www.example.com/sse");
switch (src.readyState) {
    case EventSource.CONNECTING:
        trace(`Connecting\n`);
        break;
    case EventSource.OPEN:
        trace(`Open\n`);
        break;
    case EventSource.CLOSED:
        trace(`Closed\n`);
        break;
}
```
