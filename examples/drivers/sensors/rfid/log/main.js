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

/*
	RFID log - print UID and NDEF (if any) to the debugger.

	mcconfig -dl -m -p <platform>

	Requires device.sensor.RFID (see README).
*/

import Timer from "timer";

if (!globalThis.device?.sensor?.RFID)
	throw new Error("Host does not provide RFID. See ../readme.md for info on installing a driver.");

const rfid = new device.sensor.RFID;
Timer.repeat(() => {
	const s = rfid.sample();
	if (!s)
		return;
	if (s.present) {
		let extra = "";
		try {
			const ndef = rfid.readNDEF();
			if (ndef?.text)
				extra = `  ${ndef.text}`;
		}
		catch {
			// no NDEF on this tag
		}
		trace(s.uidHex, extra, "\n");
	}
	else
		trace("removed\n");
}, 250);

trace(rfid.identification.model, "  version=0x", rfid.identification.version.toString(16), "\n");
