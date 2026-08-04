---
name: Discover Services
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

These examples use a shared `dnssd` instance, as shown in [Creating a DNS-SD Instance](./index.md#creating-a-dns-sd-instance).

Use `discover()` to search for a particular type of DNS-SD service, such as HTTP servers or Apple AirPlay speakers. There may be several devices that provide the service. `onFound()` is called for each device when the requested service is first discovered. If the service is lost, because the device removed the service or went off-line for some reason, `onLost()` is called.

```js
dnssd.discover({
	serviceType: "_airplay._tcp",
	onFound(service) {
		trace(`Found: "${service.name}" @ ${service.address}\n`);
		trace(`   @ ${service.host}:${service.port}\n`);
	},
	onLost(service) {
		trace(`Lost: ${service.name}\n`);
	},
});
```
---

If you no longer want to search for the DNS-SD service, close the returned instance.

```js
const discoverAirPlay = dnssd.discover({
	serviceType: "_airplay._tcp",
	onFound(service) {
		trace(`Found: "${service.name}" @ ${service.address}\n`);
		trace(`   @ ${service.host}:${service.port}\n`);
	}
});
// some time later
discoverAirPlay.close();
```

---

Services may include additional information in their DNS `txt` record. The service's `txt` record is available on the `service.txt` [`Map`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Map).

```js
dnssd.discover({
	serviceType: "_airplay._tcp",
	onFound(service) {
		trace(`Found: "${service.name}" @ ${service.address}\n`);
		trace(`   @ ${service.host}:${service.port}\n`);
		if (!service.txt) return;
		for (const [key, value] of service.txt)
			trace(`  ${key}=${value}\n`);
	}
});
```
---

When a service updates its DNS `txt` record, `onUpdate()` is called. The `service.txt` [Map](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Map) contains all `txt` record entries, not only the entries that changed.

```js
dnssd.discover({
	serviceType: "_airplay._tcp",
	onUpdate(service) {
		trace(`Update: "${service.name}"\n`);
		if (!service.txt) return;
		for (const [key, value] of service.txt)
			trace(`  ${key}=${value}\n`);
	}
});
```