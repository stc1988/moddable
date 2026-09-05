---
name: Create Access Point
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-07
---

The Wi-Fi Access Point Network Interface's constructor begins the process of bringing up the Wi-Fi access point.

This example creates an access point with a password. For an open access point (no password) omit the `password` property.

Use the `onChanged()` callback to know when the access point is ready.

```js
import WiFiAccessPoint from
	"embedded:network/interface/wifi/accesspoint";

const ap = new WiFiAccessPoint({
	SSID: "example",
	password: "secret",
	onChanged(name) {
		if ("connection" === name) {
			if (this.connection >= 400)
				trace(`AP up as ${this.SSID}\n`);
		}
	}
});
```

---

The Wi-Fi channel is selected automatically. To request a specific channel, use the `channel` property.

You might prefer a specific channel if a Wi-Fi scan showed that it is quieter than others.

```js
import WiFiAccessPoint from
	"embedded:network/interface/wifi/accesspoint";

const ap = new WiFiAccessPoint({
	SSID: "example",
	channel: 11
});
```

---

To create a hidden Wi-Fi access point, set the `hidden` property to true.

To request a specific security mode, set the `authentication` property to "none", "wpa2_psk", "wpa_wpa2_psk", "wpa3_psk", or "wpa2_wpa3_psk".

```js
import WiFiAccessPoint from
	"embedded:network/interface/wifi/accesspoint";

const ap = new WiFiAccessPoint({
	SSID: "WPA3 Example",
	password: "password",
	hidden: true,
	authentication: "wpa3_psk"
});
```

---

The host implementation limits the number of devices that may connect to the access point simultaneously. To set a specific limit, set `max`. This example sets `max` to `1` to allow only a single connection at a time.

```js
import WiFiAccessPoint from
	"embedded:network/interface/wifi/accesspoint";

const ap = new WiFiAccessPoint({
	SSID: "example",
	max: 1
});
```

---

The host implementation decides a default for the Wi-Fi beacon interval. You may wish to change this, for example to reduce power consumption.

```js
import WiFiAccessPoint from
	"embedded:network/interface/wifi/accesspoint";

const ap = new WiFiAccessPoint({
	SSID: "example",
	interval: 1000
});
```
