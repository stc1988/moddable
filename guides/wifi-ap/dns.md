---
name: DNS Redirect
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-07
---

A common use of a Wi-Fi Access Point is to provide a Captive Portal. To implement a captive portal, a small DNS server is needed to redirect requests, usually to the access point itself.

The Moddable SDK provides a simple DNS server that can be used for this purpose. It is implemented using the ECMA-419 UDP IO class together with modules in the Moddable SDK for parsing and serializing DNS.

---

This example uses the DNS server to redirect all DNS requests from connected devices to the access point itself by having `onResolve()` always return the IP address of the access point.

```js
import WiFiAccessPoint from
	"embedded:network/interface/wifi/accesspoint";
import DNSServer from "embedded:network/dns/server/udp";
import UDP from "embedded:io/socket/udp";

const ap = new WiFiAccessPoint({
	SSID: "captive",
	onChanged() {
		if (this.connection < 400)
			return;

		this.dnsServer ??= new DNSServer({
			socket: {io: UDP},
			onResolve: () => ap.address
		});
	}
});
```
