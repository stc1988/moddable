---
name: Set Real-Time Clock Time
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

On devices with Real-Time Clock hardware, you can set the time stored on the Real-Time Clock using Unix Time in milliseconds.

```js
const rtc = new device.rtc.io(device.rtc);
rtc.time = 1760390224000;
rtc.close();
```
