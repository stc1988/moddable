---
name: Provide Web Pages for Captive Portal
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-09-02
---

The web browser on the user's phone or computer displays the captive portal user interface to provision Wi-Fi. The Captive Portal module does not include a built-in web site because each product will likely want an experience unique to their brand. To get started, you can simply use the simple provisioning web page provided by the Captive Portal example by including it in your project's manifest.

```json
{
	"data": {
		"provision/index.html": "$(MODDABLE)/examples/io/wifiaccesspoint/captiveportal/site/index.html"
	}
}
```

---

The captive portal calls `onPage()` to retrieve web pages, images, and other assets needed by the captive portal web page. Your callback returns the `content` as either a string or a byte buffer, along with the MIME type. If you're using the example single-page web site, this is very straightforward as shown in this example.

For more sophisticated provisioning web sites, the main addition is usually to map the file extension to a MIME type. The [Captive Portal example](../../examples/io/wifiaccesspoint/captiveportal/main.js) shows one way to do this.

```js
import CaptivePortal from "captiveportal";
import Resource from "Resource";

new CaptivePortal({
	SSID: "Washer Setup",
	password: "whirlpool",
	onPage(path) {
		path = ("/" === path) ? "index.html" : path.slice(1);
		path = `provision/${path}`;
		if (Resource.exists(path))
			return {content: new Resource(path),
						mimeType: "text/html"};
	}
});
```

---

Wi-Fi is not the only aspect of a product that needs to be configured. The captive portal web site can be used to configure just about anything. In fact, the example captive portal web site sends the device the current time, timezone, and daylight savings offset, along with the user's preferred language.

The captive portal web site communicates with the device over a standard WebSocket connection which you can use to send any information back to the device. Your captive portal instance on the device receives the message in its `onInfo()` callback (see [Get Information from Captive Portal](./info.md)).

```js
socket.send(JSON.stringify({
	event: "favoriteColor",
	color: "#0000ff"
}));
```
