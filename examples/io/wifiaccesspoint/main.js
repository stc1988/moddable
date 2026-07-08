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

import WiFiAccessPoint from "embedded:network/interface/wifi/accesspoint";

new WiFiAccessPoint({
	SSID: "Moddable-AP",
	password: "moddable",
	channel: 6,
	onChanged(name) {
		if ("connection" === name) {
			if (400 === this.connection)
				trace(`AP up as ${this.SSID} at ${this.address} (${this.MAC}) on channel ${this.channel}\n`);
			else
				trace(`AP stopped\n`);
		}
	},
	onConnect(station) {
		trace(`joined: ${station.MAC}. ${stations.size} connections.\n`);
	},
	onDisconnect(station) {
		trace(`left: ${station.MAC}. ${stations.size} connections.\n`);
	}
});
