---
name: Disconnect
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Use `disconnect()` to disconnect from a Wi-Fi access point.  This example disconnects immediately after connecting, a useful technique when validating Wi-Fi credentials.

Note that the Wi-Fi module's `close()` method does not disconnect from Wi-Fi to allow multiple instances of the Wi-Fi module to co-exist.

```js
import WiFi from "embedded:network/interface/wifi";

const wifi = new WiFi({});
wifi.connect({
	SSID: "my SSID",
	password: "my password",
	onChanged() {
		if (this.connection >= 400) {
			trace(`Connection ready @ ${this.address}.\n`);
			this.disconnect();
		}
	}
});
```
