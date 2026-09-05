---
name: Captive Portal
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-09-02
---

The [Captive Portal](https://en.wikipedia.org/wiki/Captive_portal) module makes provisioning Wi-Fi easy. One of the biggest obstacles faced by embedded developers is the process of having a user select a Wi-Fi network for the device and enter the Wi-Fi password. The Captive Portal module allows the user to provision their Wi-Fi using a phone or computer. It relies only on a web browser, eliminating the need for the user to install another app.

This module provides a stable API that works across microcontrollers and RTOSs including ESP32, Raspberry Pi Pico, ESP8266, and Zephyr. It integrates many disparate modules to reliably provision Wi-Fi:

- Wi-Fi access point - for the user to connect to from their phone or computer
- Wi-Fi station mode - to verify the Wi-Fi password the user provides
- HTTP Server - to serve web pages to the user
- WebSocket Server - for low-latency communication
- DNS Server - to redirect captive portal requests

The Captive Portal module doesn't require your device to have a display, but users benefit if it does. That's because the display can jump-start provisioning by presenting a [Wi-Fi QR code](https://dev.to/mycko22/the-technical-guide-to-wifi-qr-codes-implementation-security-and-best-practices-2f2n) with the Wi-Fi access point's SSID and password.

The Captive Portal module takes care of many subtle details.

- Ensures the SSID broadcast used for the Wi-Fi access point has a unique name (e.g. doesn't collide with nearby devices)
- Limits the access point to a single user to avoid complexity caused by multiple users provisioning Wi-Fi simultaneously
- Announces the captive portal web page using [DHCP](https://www.rfc-editor.org/info/rfc8910/) on devices with that capability (ESP32 family)

The Captive Portal module is available in the simulator to assist in developing and testing your application. The simulator performs the Wi-Fi provisioning as a ghost user that progresses through the phases of provisioning. This simulator does not exercise every path, so it is still essential to test on a real device. 

While the Captive Portal module is not part of the ECMA-419 standard, it follows ECMA-419 conventions including the [Base Class Pattern](https://419.ecma-international.org/#base-class-pattern).

## Using Captive Portal

- [Create Captive Portal](./create.md)
- [Close Captive Portal](./close.md)
- [Provide Web Pages for Captive Portal](./webpages.md)
- [Get Information from Captive Portal](./info.md)

## Building with mcconfig

Include the Captive Portal's manifest in your project's `manifest.json`:

	$(MODDABLE)/examples/io/wifiaccesspoint/captiveportal/manifest_captiveportal.json

Then, import the module in your JavaScript source code:

```js
import CaptivePortal from "captiveportal";
```

## Building with mcpack

Import the module in your JavaScript source code. `mcpack` automatically includes the manifest.

```js
import CaptivePortal from "captiveportal";
```

## Learn More

- Example
	- [Captive Portal example](../../examples/io/wifiaccesspoint/captiveportal/main.js) in Moddable SDK
- Implementation
	- [Captive Portal module](../../examples/io/wifiaccesspoint/captiveportal/captiveportal.js) in Moddable SDK
