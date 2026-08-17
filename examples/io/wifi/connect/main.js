/*
 * Copyright (c) 2026  Moddable Tech, Inc.
 *
 *   This file is part of the Moddable SDK.
 * 
 *   This work is licensed under the
 *       Creative Commons Attribution 4.0 International License.
 *   To view a copy of this license, visit
 *       <http://creativecommons.org/licenses/by/4.0>.
 *   or send a letter to Creative Commons, PO Box 1866,
 *   Mountain View, CA 94042, USA.
 *
 */

import WiFi from "embedded:network/interface/wifi";

const wifi = new WiFi({
	onChanged(property) {
		trace(`Wi-Fi ${property} changed to ${this[property]}\n`);

		if ("connection" === property) {
			if (this.connection >= 500)
				trace(`Connection ready @ ${this.address}.\n`);
			else if (this.connection >= 300)
				trace(`Connected to Wi-Fi.\n`);
			else if (this.connection <= 200)
				trace(`Wi-Fi connection failed.\n`);
		}
	}
});

wifi.connect({
	SSID: "Moddable Guest",
	password: "javascript"
});
