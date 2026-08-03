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

const Digital = device.io.Digital;
import config from "mc/config";
import { TransformStream } from "web/streams";
import { IOReadableStreamMixin, IOWritableStreamMixin } from "iostreams";

const DigitalReadableStream = IOReadableStreamMixin(Digital);
const DigitalWritableStream = IOWritableStreamMixin(Digital);

if (undefined === device?.pin?.button)
	throw new Error("no button pin provided by device");
if (undefined === device?.pin?.led)
	throw new Error("no led pin provided by device");

const writable = new DigitalWritableStream({
	pin: device.pin.led,
	mode: Digital.Output,
});

const readable = new DigitalReadableStream({
	pin: device.pin.button,
	mode: Digital.InputPullUp,
	edge: Digital.Rising | Digital.Falling,
});

const invert = new TransformStream({
	transform(state, controller) {
		controller.enqueue(state ^ 1);
	}
});

readable.pipeThrough(invert).pipeTo(writable);