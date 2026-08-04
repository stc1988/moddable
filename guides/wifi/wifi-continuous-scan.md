---
name: Scan Continuously for Access Points
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

A Wi-Fi scan typically takes a few seconds, with the actual duration determined by the host implementation. A long scan may discover additional access points. Chain together individual scans by initiating new scans from `onComplete()` to scan continuously.

```js
import WiFi from "embedded:network/interface/wifi";

const wifi = new WiFi({});
function scan() {
	wifi.scan({
		onFound(ap) {
			trace(`Found ${ap.SSID}\n`);
			trace(`  channel: ${ap.channel}\n`);
			trace(`  security: ${ap.security}\n`);
			trace(`  RSSI: ${ap.RSSI}\n`);
		},
		onComplete() {
			trace(`Start next scan\n`);
			scan();
		}
	});
}
scan();
```
