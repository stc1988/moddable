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
	Single channel LED on a PWM output (see led/digital for an on/off LED)

	Options:
		io			PWM constructor
		pin, ...	passed through to the PWM constructor
		invert		true if the LED is active low
		target		optional, stored on the instance

	Properties:
		on			0 to 1 (luminance)
		color		{r, g, b}, 0 to 255 each. Setting reduces to luminance; getting returns a gray.
*/

class LED {
	#io;

	constructor(options) {
		const {target, invert, ...io} = options;
		if (undefined !== target)
			this.target = target;

		this.#io = new io.io(io);
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
		io.value = value = clamp(value, 1);
		const range = (1 << io.resolution) - 1;
		value = (value * range) | 0;
		io.write(io.invert ? range - value : value);
	}
	get color() {
		const value = this.#io.value * 255;
		return {r: value, g: value, b: value};
	}
	set color(value) {
		const {r, g, b} = value;
		this.on = (((r << 1) + r + (g << 2) + b) >> 3) / 255;		// integer luma, same as toGray in commodettoConvert.c
	}
}

function clamp(value, max) {
	value = Number(value);
	return (value > 0) ? ((value < max) ? value : max) : 0;
}

export default LED;
