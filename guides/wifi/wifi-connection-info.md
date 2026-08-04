---
name: Get Connection Information
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

The state of the Wi-Fi connection is available from properties on the instance. The properties may be read at any time. The `onChanged()` callback is invoked when a property changes with `what` set to the name of the property that changed.

This example logs the initial value of each property and their updates.

```js
import WiFi from "embedded:network/interface/wifi";

const wifi = new WiFi({
	onChanged(what) {
		trace(`Wi-Fi ${what} is ${this[what]}\n`);
	}
});

["address", "connection", "MAC", "SSID", "BSSID", "RSSI", "channel"].forEach(what => {
	trace(`Wi-Fi ${what} is ${wifi[what]}\n`);
});
```
