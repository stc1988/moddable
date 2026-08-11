---
name: Connect
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-10
---

Use `connect()` to initiate a connection to a Wi-Fi access point. The connection is established asynchronously with progress reported through the `onChanged()` callback.

If the connection attempt fails, `onChanged` is called with `this.connection` set to 200 (disconnected) or less. You don't need a timer to detect a failed connection attmept.

```js
import WiFi from "embedded:network/interface/wifi";

const wifi = new WiFi({
	onChanged() {
		if (this.connection >= 500)
			trace(`Connection ready @ ${this.address}.\n`);
		else if (this.connection >= 300)
			trace(`Connected to Wi-Fi.\n`);
		else if (this.connection <= 200)
			trace(`Wi-Fi connection failed.\n`);
	}
});

wifi.connect({
	SSID: "my SSID",
	password: "my password",
});
```
