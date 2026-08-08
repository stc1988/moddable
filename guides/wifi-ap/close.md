---
name: Close Access Point
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-07
---

When the Access Point is no longer needed, use `close()` to bring it down. This example closes the access point after ten seconds.

When the access point is closed, all connections to it are immediately terminated.

```js
import WiFiAccessPoint from
	"embedded:network/interface/wifi/accesspoint";
import Timer from "timer";

const ap = new WiFiAccessPoint({
	SSID: "short lived"
});

Timer.set(() => {
	ap.close();
}, 10_000);
```
