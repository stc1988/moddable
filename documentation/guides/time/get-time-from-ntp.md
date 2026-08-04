---
name: Get Time and Date from Network
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Use Network Time Protocol (NTP) to retrieve the current time and date from a network time server.

```js
const ntp = new device.network.ntp.client.io(
	device.network.ntp.client
);

ntp.getTime((error, time) => {
	trace(`NTP time ${time}\n`);
	ntp.close();
});
```

---

You can use the time value provided by NTP to set the time and date of the device. This is useful to initialize the time and date at start-up.

Note that the NTP `time` value is milliseconds but `Time.set()` expects seconds.

```js
const ntp = new device.network.ntp.client.io(
	device.network.ntp.client
);

ntp.getTime((error, time) => {
	if (!error)
		Time.set(time / 1000);
	ntp.close();
});
```

---

You can provide a list of time servers to override the system default.

```js
const ntp = new device.network.ntp.client.io({
	...device.network.ntp.client,
	servers: ["pool.ntp.org", "time.cloudflare.com"]
});
```
