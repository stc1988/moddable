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
	RFID UI - open the reader first (same as log), then the panel.

	Do not include examples/manifest_commodetto.json: that runs
	setup/commodetto and constructs ili9341 + AXP backlight before main.

	Green = present, amber = last UID / NDEF, gray = idle, red = error,
	blue = wrote Type 2 / Classic URI (button A on hosts that have one).
*/

import Timer from "timer";
import Poco from "commodetto/Poco";
import parseBMF from "commodetto/parseBMF";
import Resource from "Resource";
import config from "mc/config";

const HOLD_MS = 2500;
const POLL_MS = 250;
const WRITE_URI = "https://www.moddable.com";
const canWrite = !!(device.peripheral?.button?.A || (typeof device.pin?.buttonA === "number"));

let mode = "idle";
let lastLine = "-";
let ndefLine = "";
let holdTimer;
let cardPresent = false;
let render, white, green, amber, gray, blue, red, font, titleFont;

const rfid = new device.sensor.RFID;
lastLine = rfid.identification.model;
trace(rfid.identification.model, "  version=0x", rfid.version.toString(16), "\n");

try {
	if (config.Screen && !globalThis.screen) {
		globalThis.screen = new config.Screen({});
		if (config.driverRotation) {
			if (config.rotation)
				screen.rotation = (config.driverRotation + config.rotation) % 360;
			else
				screen.rotation = config.driverRotation;
		}
	}
	render = new Poco(screen, {displayListLength: 4096, pixels: screen.width * 8});

	if (device.peripheral?.Backlight) {
		globalThis.backlight = new device.peripheral.Backlight;
		globalThis.backlight.brightness = 0.5;
	}

	white = render.makeColor(255, 255, 255);
	green = render.makeColor(40, 180, 80);
	amber = render.makeColor(200, 140, 40);
	gray = render.makeColor(80, 80, 80);
	blue = render.makeColor(40, 100, 200);
	red = render.makeColor(180, 50, 50);
	titleFont = parseBMF(new Resource("OpenSans-Semibold-28.bf4"));
	font = parseBMF(new Resource("OpenSans-Regular-24.bf4"));
}
catch (e) {
	trace("screen: ", e, "\n");
}

// LCD bring-up can step the 3V3 rail; start polling after the screen is up.
rfid.configure({antenna: true});
Timer.repeat(() => {
	const s = rfid.sample();
	if (!s)
		return;
	if (s.present) {
		if (holdTimer) {
			Timer.clear(holdTimer);
			holdTimer = undefined;
		}
		cardPresent = true;
		mode = "live";
		lastLine = s.uidHex;
		ndefLine = s.type ?? (canWrite ? "A=write" : "");
		trace(s.uidHex, "\n");
		draw();
		try {
			const ndef = rfid.readNDEF();
			if (ndef?.text) {
				ndefLine = ndef.text;
				trace(s.uidHex, "  ", ndefLine, "\n");
				draw();
			}
		}
		catch {
			// no NDEF on this tag
		}
	}
	else {
		cardPresent = false;
		trace("removed\n");
		mode = "hold";
		draw();
		holdTimer = Timer.set(() => {
			holdTimer = undefined;
			if (mode === "hold") {
				mode = "idle";
				lastLine = "-";
				ndefLine = "";
				draw();
			}
		}, HOLD_MS);
	}
}, POLL_MS);

function fit(s, maxW) {
	if (!s)
		return "";
	if (render.getTextWidth(s, font) <= maxW)
		return s;
	while (s.length && (render.getTextWidth(s + "...", font) > maxW))
		s = s.slice(0, -1);
	return s + "...";
}

function draw() {
	if (!render || !titleFont || !font)
		return;
	try {
		const w = render.width;
		const h = render.height;
		const bg = (mode === "live") ? green
			: (mode === "hold") ? amber
			: (mode === "wrote") ? blue
			: (mode === "error") ? red
			: gray;
		const title = (mode === "hold") ? "last"
			: (mode === "wrote") ? "wrote"
			: (mode === "error") ? "fail"
			: "RFID";
		const pad = 10;
		const gap = 10;
		const block = titleFont.height + gap + font.height
			+ (ndefLine ? (gap + font.height) : 0);
		let y = (h - block) >> 1;
		render.begin();
			render.fillRectangle(bg, 0, 0, w, h);
			render.drawText(title, titleFont, white,
				(w - render.getTextWidth(title, titleFont)) >> 1, y);
			y += titleFont.height + gap;
			const uid = fit(lastLine, w - pad * 2);
			render.drawText(uid, font, white,
				(w - render.getTextWidth(uid, font)) >> 1, y);
			if (ndefLine) {
				y += font.height + gap;
				const ndef = fit(ndefLine, w - pad * 2);
				render.drawText(ndef, font, white,
					(w - render.getTextWidth(ndef, font)) >> 1, y);
			}
		render.end();
	}
	catch (e) {
		trace("draw: ", e, "\n");
	}
}

function writeDemoUri() {
	if (!cardPresent)
		return;
	try {
		const ndef = rfid.writeNDEF({uri: WRITE_URI});
		mode = "wrote";
		ndefLine = ndef?.text ?? WRITE_URI;
		trace("NDEF write: ", ndefLine, "\n");
		draw();
	}
	catch (e) {
		mode = "error";
		ndefLine = String(e);
		trace("NDEF write failed: ", e, "\n");
		draw();
	}
}

if (device.peripheral?.button?.A) {
	const buttonA = new device.peripheral.button.A({
		onPush() {
			if (buttonA.pressed)
				writeDemoUri();
		}
	});
}
else if ((typeof device.pin?.buttonA === "number") && device.peripheral?.Button) {
	const Digital = device.io.Digital;
	const buttonA = new device.peripheral.Button({
		io: Digital,
		pin: device.pin.buttonA,
		mode: Digital.InputPullUp,
		invert: true,
		onPush() {
			if (buttonA.pressed)
				writeDemoUri();
		}
	});
}

draw();
