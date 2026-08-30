/*
 * Copyright (c) 2026  Moddable Tech, Inc.
 *
 *   This file is part of the Moddable SDK Runtime.
 *
 *   The Moddable SDK Runtime is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   The Moddable SDK Runtime is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with the Moddable SDK Runtime.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
	On/off LED on a Digital output (see led/pwm for a dimmable LED)

	Options:
		io			Digital constructor
		pin, ...	passed through to the Digital constructor (mode defaults to Output)
		invert		true if the LED is active low
		target		optional, stored on the instance

	Properties:
		on			0 or 1. Setting any non-zero luminance turns the LED on.
		color		{r, g, b}, 0 to 255 each. Setting a color with non-zero luma turns the LED on; getting returns black or white.
*/

class LED {
	#io;

	constructor(options) {
		const {target, invert, ...io} = options;
		if (undefined !== target)
			this.target = target;

		this.#io = new io.io({mode: io.io.Output, ...io});
		if (invert)
			this.#io.invert = true;
		this.on = 0;
	}
	close() {
		this.#io?.close();
		this.#io = undefined;
	}
	get on() {
		return this.#io.value;
	}
	set on(value) {
		const io = this.#io;
		io.value = value = (clamp(value, 1) > 0) ? 1 : 0;
		io.write(io.invert ? value ^ 1 : value);
	}
	get color() {
		const value = this.#io.value * 255;
		return {r: value, g: value, b: value};
	}
	set color(value) {
		const r = clamp(value.r, 255), g = clamp(value.g, 255), b = clamp(value.b, 255);
		this.on = ((r << 1) + r + (g << 2) + b) >> 3;		// integer luma, same as toGray in commodettoConvert.c
	}
}

function clamp(value, max) {
	value = Number(value);
	return (value > 0) ? ((value < max) ? value : max) : 0;
}

export default LED;
