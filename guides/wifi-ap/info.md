---
name: Get Information
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-07
---

The Wi-Fi Access Point instance has several properties that reflect its current state.

The name of the access point is the `SSID` property. The current operation status is `connection` with values of 400 or above indicating that the access point is operational. The Wi-Fi channel used by the access point is the `channel` property.

```js
import WiFiAccessPoint from
	"embedded:network/interface/wifi/accesspoint";

const ap = new WiFiAccessPoint({
	SSID: "example",
	onChanged() {
		if (this.connection >= 400) {
			trace(`SSID: ${this.SSID}\n`);
			trace(`connection: ${this.connection}\n`);
			trace(`channel: ${this.channel}\n`);
		}
	}
});
```

---

The IP address and MAC address are on the `address` and `MAC` properties, respectively.

```js
import WiFiAccessPoint from
	"embedded:network/interface/wifi/accesspoint";

const ap = new WiFiAccessPoint({
	SSID: "example",
	onChanged() {
		if (this.connection >= 400) {
			trace(`IP: ${ap.address}\n`);
			trace(`MAC: ${ap.MAC}\n`);
		}
	}
});
```
