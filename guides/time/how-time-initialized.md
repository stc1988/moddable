---
name: How Time and Timezone are Initialized
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-19
---

The system time needs to be set when the microcontroller starts up. In addition, many projects require the system's timezone and daylight saving time to be set. These are done in a variety of different ways, depending on the capabilities of the device hardware, the device firmware, and your project. In some cases, the host firmware takes care of them; in others, your project must initialize these values.

When developing with the Moddable SDK, the JavaScript debugger connection automatically initializes the microcontroller's system time, timezone, and daylight saving time offset from the corresponding values on the development computer. This behavior is convenient. It can, however, lead developers to believe they don't need to manage these values in their own projects. Try running an instrumented build on the device to test without the debugger (run an instrumented build by replacing `-d` with `-i` when running `mcconfig`).

If your project uses the default Wi-Fi set-up by passing `SSID` and `password` to `mcconfig`, the Wi-Fi set-up code attempts to [initialize the system time using NTP](./get-time-from-ntp.md) before your project's `main` runs. This ensures an accurate system time, which is essential for validating certificates when using secure network communication with TLS. NTP cannot, however, initialize the timezone and daylight saving time. If your project manages the Wi-Fi connection itself, you may also want to initialize the system time using NTP. 

If your development board includes a battery-backed real-time clock (RTC), a host may [initialize the system time from the real-time clock](./get-time-from-rtc.md) at start-up. If the host does not do that, you can set the [system time from the RTC](./get-time-from-rtc.md). Your code may still need to perform a [one-time initialization of the RTC](./set-rtc-time.md), if it has not yet been set. An RTC does not usually provide the timezone or daylight saving time offset.

Note that when running the simulator, the system time, timezone, and daylight saving time offset are initialized from the computer running the simulator and may not be changed. Consequently, it is necessary to test time initialization code on the device, preferably with an instrumented build.

---

A common way to determine if the system time has been initialized is to compare the system time to the time your code was developed. This guide was written at Unix time 1787160049 (in seconds).

```js
if (Date.now() >= 1787160049_000)
	trace(`Time initialized: ${new Date()}\n`);
else
	trace(`Time uninitialized.\n`);
```
---

The timezone and daylight saving time offset are unnecessary for many microcontroller projects. An accurate system time is enough. However, for projects which display a date or time value to the user, such as those with a display, they are essential. Fortunately, such devices typically have an interactive user interface where the user can set the timezone and daylight savings offset.

These may be set explicitly by the user or implicitly. For example, during provisioning, the timezone and daylight saving time offset might be transferred from the phone or computer used to provision.

Once the timezone and daylight saving time offset are known, they can be set for the runtime:

```js
import Time from "time";

Time.timezone = -8 * 60 * 60;		// seconds
Time.dst = 60 * 60;		// seconds
```

---

The Time module changes the runtime values for the timezone and daylight saving offset, but does not restore them after the device restarts. To do that, use [Key-Value Storage](../key-value/index.md). This example saves the values.

```js
import Time from "time";

using timeSettings = device.keyValue.open({
	path: "time",
	format: "int32"
});

timeSettings.write("timezone", Time.timezone);
timeSettings.write("dst", Time.dst);
```
---

You can restore the timezone and daylight saving offset from key-value storage. This example applies the values only if they have been set.

```js
import Time from "time";

using timeSettings = device.keyValue.open({
	path: "time",
	format: "int32"
});

let value = timeSettings.read("timezone");
if (undefined !== value)
	Time.timezone = value;
value = timeSettings.read("dst");
if (undefined !== value)
	Time.dst = value;
```
