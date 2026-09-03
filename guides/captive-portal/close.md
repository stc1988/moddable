---
name: Close Captive Portal
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-09-02
---

When you are done with the captive portal, call `close()`. This ends any provisioning effort in progress, terminates any network connections, and shuts down the Wi-Fi access point.

If the device is connected to Wi-Fi in station mode, it does not terminate that connection. A successful provisioning session ends with the device connected to the Wi-Fi network the user selected. By leaving the connection in place, the device is ready to use the network immediately.

```js
import CaptivePortal from "captiveportal";

const cp = new CaptivePortal({
	SSID: "Moddable-AP",
	onPage(path) {
		// see Provide Web Pages for Captive Portal
	}
});

// some time later
cp.close();
```

---

When provisioning is complete, the web page sends a message to end the captive portal session which closes the instance immediately and calls `onClose()`.

```js
import CaptivePortal from "captiveportal";

new CaptivePortal({
	SSID: "Moddable-AP",
	onPage(path) {
		// see Provide Web Pages for Captive Portal
	},
	onClose() {
		trace(`captive portal closed by remote\n`);
	}
});
```

---

If an unexpected error occurs, `onError()` is called. You should immediately call `close()` to end the captive portal session

```js
import CaptivePortal from "captiveportal";

new CaptivePortal({
	SSID: "Moddable-AP",
	onPage(path) {
		// see Provide Web Pages for Captive Portal
	},
	onError(e) {
		trace(`captive portal failure: ${e}\n`);
		this.close();
	}
});
```
