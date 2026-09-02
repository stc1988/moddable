---
name: Create Captive Portal
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-09-02
---

To create a captive portal, you provide the name (`SSID`) for the Wi-Fi access point the user will connect to, along with an `onPage()` callback to [provide the web pages](./webpages.md) displayed by the captive portal.

The `SSID` is optional and defaults to `"Moddable"`. Don't worry about choosing a unique SSID. The Captive Portal module scans for SSID collisions and appends random characters as needed to your SSID to ensure a unique name.

```js
import CaptivePortal from "captiveportal";

new CaptivePortal({
	SSID: "Moddable-AP",
	onPage(path) {
		// see Provide Web Pages for Captive Portal
	}
});
```

---

To specify a password for the access point, set the `password` property as in this example.

If you don't provide a password, the captive portal creates a random password (see [Get Information from Captive Portal](./info.md) for how to get that password).

To create an open access point, pass an empty string (`""`) for `password`.

```js
import CaptivePortal from "captiveportal";

new CaptivePortal({
	SSID: "Lightbulb Setup",
	password: "brightly",
	onPage(path) {
		// see Provide Web Pages for Captive Portal
	}
});
```

---

The captive portal has a couple of options you may never need to use.

- Set the `port` for the HTTP server to use. The default is 80, which is the most compatible choice.
- Request the Wi-Fi access point operate on a specific Wi-Fi channel. This is only a request because the hardware may not support all channels and because the host may not support requesting a specific channel, for example if it already has a Wi-Fi station mode connection. By default, the captive portal tries to select a channel that is relatively clear.

```js
import CaptivePortal from "captiveportal";

new CaptivePortal({
	SSID: "Lightbulb Setup",
	port: 8080,
	channel: 11,
	onPage(path) {
		// see Provide Web Pages for Captive Portal
	}
});
```
