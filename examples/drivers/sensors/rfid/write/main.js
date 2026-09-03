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
	RFID write - write one URI to each Type 2 or MIFARE Classic tag presented.

	mcconfig -dl -m -p <platform>

	Type 2: disposable NDEF-formatted sticker.
	Classic 1K: factory or NFC Forum keys; overwrites sector 1 (blocks 4-6).
	Remove the tag, then present another to write again.
*/

import Timer from "timer";

const URI = "http://www.moddable.com";
//const URI = "http://www.intlweb.com/oops1.png";

const rfid = new device.sensor.RFID;
Timer.repeat(() => {
	const s = rfid.sample();
	if (!s?.present)
		return;

	try {
		const ndef = rfid.writeNDEF({uri: URI});
		trace("wrote ", ndef?.text ?? URI, "\n");
	}
	catch (e) {
		trace("write failed: ", e, "\n");
	}
}, 250);

trace(rfid.identification.model, " - hold a Type 2 or Classic 1K tag to write\n");
