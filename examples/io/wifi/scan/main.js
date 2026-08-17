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

const accessPoints = new Set;
let count = 0;

const wifi = new WiFi({});
function scan() {
	trace(`** start scan ${++count} **\n`);
	wifi.scan({
		onFound(ap) {
			if (accessPoints.has(ap.SSID))
				return;

			accessPoints.add(ap.SSID);

			trace(`Found ${ap.SSID}\n`);
			trace(`  channel: ${ap.channel}\n`);
			trace(`  security: ${ap.security}\n`);
			trace(`  RSSI: ${ap.RSSI}\n`);
		},
		onComplete() {
			if (count < 20)
				scan();
		}
	});
}
scan();
