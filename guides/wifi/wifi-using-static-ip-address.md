---
name: Use Static IP Address
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

The IP address assigned to the Wi-Fi connection is determined by host and network policies. Usually, an IP address is dynamically assigned by DHCP. You can use a static IP address by calling `configure()`.

This example calls `configure()` to use a static IP address immediately after the connection is established but before the default IP address is assigned.

```js
import WiFi from "embedded:network/interface/wifi";

const wifi = new WiFi({});
wifi.connect({
	SSID: "my SSID",
	password: "my password",
	onChanged() {
		if (this.connection >= 400)
			trace(`Connection ready @ ${this.address}.\n`);
		else if (this.connection >= 300) {
			trace(`Connected to Wi-Fi.\n`);
			this.configure({
				static: {
					address: "192.168.4.31",
					mask: "255.255.255.0",
					gateway: "192.168.4.1"
				}
			});
		}
		else if (this.connection <= 200)
			trace(`Wi-Fi connection failed.\n`);
	}
});
```

---

To revert to a dynamically assigned IP address, call `configure()` with `static` set to `false`.

```js
this.configure({
	static: false
});
```
