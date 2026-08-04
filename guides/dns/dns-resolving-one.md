---
name: Resolve Name
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

To resolve a DNS name to its address, use a DNS resolver class. You provide the constructor a list of the DNS servers to use to resolve the address.

Once resolution completes, either successfully or with an error, close the DNS resolver instance to release any resources it holds.

```js
import Resolver from "embedded:network/dns/resolver/udp";
import UDP from "embedded:io/socket/udp";

(new Resolver({
	socket: { io: UDP },
	servers: ["8.8.8.8", "1.1.1.1"]
})).resolve({
	host: "moddable.com",
	onResolved(host, address) {
		trace(`Resolved ${host} to ${address}\n`);
		this.close();
	},
	onError(host) {
		trace(`Unable to resolve ${host}\n`);
		this.close();
	}
});
```

---

The DNS resolver has several special cases:
- `"localhost"` resolves to `127.0.0.1`.
- Names ending in `.local` as used by DNS-SD are resolved using mDNS.
- If the host name is an IP address, that address is passed to `onResolved()` for both the `host` and `address` arguments.
