---
name: Reconnect Automatically
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

The Wi-Fi module does not automatically attempt to reconnect when the connection is dropped. This allows your project to fully manage the Wi-Fi connection policy.

Detect connection failures by using `onChanged()` to to watch for `this.connection` to fall to 200 or lower. When a connection is dropped, this example initiates a new connection attempt immediately by calling `connect()` from the disconnect notification.

```js
import WiFi from "embedded:network/interface/wifi";

const wifi = new WiFi({});
function connect() {
	wifi.connect({
		SSID: "my SSID",
		password: "my password",
		onChanged() {
			if (this.connection >= 400) {
				trace(`Connection ready.\n`);
				trace(`Assigned IP address ${this.address}.\n`);
				this.success = true;
			}
			else if (this.connection >= 300)
				trace(`Connected to Wi-Fi.\n`);
			else if (this.connection <= 200) {
				trace(`Wi-Fi connection failed.\n`);
				if (this.success) {
					trace(`Attempting to Wi-Fi reconnect.\n`);
					connect();
				}
			}
		}
	});
};
connect();
```
