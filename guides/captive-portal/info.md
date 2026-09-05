---
name: Get Information from Captive Portal
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-09-02
---

The captive portal reports configuration values to the `onInfo()` callback. The primary use of this is to provide the Wi-Fi credentials the user selected during provisioning. It can also provide other configuration such as the time settings or preferred language (see [Provide Web Pages for Captive Portal](./webpages.md)).

`onInfo()` is called with an object that contains an `event` property to indicate what is being configured. For example, the provisioned Wi-Fi credentials `event` is `"credentials"`.

```js
import CaptivePortal from "captiveportal";

const cp = new CaptivePortal({
	SSID: "Dryer Setup",
	onPage(path) {
		// see Provide Web Pages for Captive Portal
	},
	onInfo(msg) {
		if ("credentials" !== msg.event)
			return;
		trace(`Wi-Fi provisioned\n`);
		trace(` SSID: ${msg.SSID}\n`);
		if (msg.password)
			trace(` Password: ${msg.password}\n`);
	}
});
```

---

Your project probably will want to store the Wi-Fi credentials so that it can reconnect to Wi-Fi after restarting. [Key-Value Storage](../key-value/index.md) is a convenient way to store these. See the [Wi-Fi Guide](../wifi/index.md) for examples of connecting to a Wi-Fi access point.

```js
import CaptivePortal from "captiveportal";

new CaptivePortal({
	SSID: "Dryer Setup",
	onPage(path) {
		// see Provide Web Pages for Captive Portal
	},
	onInfo(msg) {
		if ("credentials" !== msg.event)
			return;

		using wifi = device.keyValue.open({
			path: "wifi",
			format: "string"
		});

		wifi.write("SSID", msg.SSID);
		if (msg.password)
			wifi.write("password", msg.password);
	}
});
```

---

The Wi-Fi provisioning process goes through several phases, starting with initializing the Wi-Fi access point and ending with the success or failure of provisioning. The captive portal reports the current phase to `onStatus()` along with any relevant details about the phase.

For example, once the Wi-Fi access point is ready to accept connections, the phase is `"ready"` and the actual SSID and password selected are reported. This status information is useful for projects that display a user interface that reflects the progress of provisioning. See the Captive Portal example's [readme](../../examples/io/wifiaccesspoint/captiveportal/readme.md) for a list of phases.

```js
import CaptivePortal from "captiveportal";

new CaptivePortal({
	SSID: "Dryer Setup",
	onPage(path) {
		// see Provide Web Pages for Captive Portal
	},
	onStatus(phase, details) {
		trace(`phase: ${phase}\n`);
		if (details)
			trace(` ${JSON.stringify(details)}\n`);
	}
});
```
