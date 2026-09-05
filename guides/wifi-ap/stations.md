---
name: Manage Stations
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-07
---

Many uses of a Wi-Fi Access Point Network Interface don't need to be aware of when connections begin and end. There are situations where it is necessary, for example to display the number of active connections in a user interface or to make network requests to the connected devices.

The `onConnect()` callback is called when a new connection is established; `onDisconnect()`, when the connection ends. Both callbacks are passed a connection instance.

```js
import WiFiAccessPoint from
	"embedded:network/interface/wifi/accesspoint";

let stations = 0;
const ap = new WiFiAccessPoint({
	SSID: "example",
	onConnect(station) {
		stations += 1;
		trace(`stations: ${stations}\n`);
	},
	onDisconnect(station) {
		stations -= 1;
		trace(`stations: ${stations}\n`);
	}
});
```

---

Each station instance has `address` and `MAC` properties for the IP address and MAC address of the remote device.

```js
import WiFiAccessPoint from
	"embedded:network/interface/wifi/accesspoint";

const ap = new WiFiAccessPoint({
	SSID: "example",
	onConnect(station) {
		trace(`IP: ${station.address}\n`);
		trace(`MAC: ${station.MAC}\n`);
	}
});
```

---

The station instance's `close()` method ends the station. This example uses `close()` to limit each station to one minute.

```js
import WiFiAccessPoint from
	"embedded:network/interface/wifi/accesspoint";
import Timer from "timer";

const timeouts = new Map();
const ap = new WiFiAccessPoint({
	SSID: "example",
	onConnect(station) {
		const timer = Timer.set(() => {
			timeouts.delete(station);
			station.close();
		}, 60_000);
		timeouts.set(station, timer);
	},
	onDisconnect(station) {
		const timer = timeouts.get(station);
		Timer.clear(timer);
		timeouts.delete(station);
	}
});
```
