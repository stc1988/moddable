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
	RGB LED on three PWM outputs

	Options:
		io			PWM constructor, used for all three channels
		pin			{r, g, b} pin numbers
		...			remaining options passed through to each PWM constructor
		invert		true if the LED is active low
		target		optional, stored on the instance

	Properties:
		on			0 to 1 (luminance). Setting produces a gray.
		color		{r, g, b}, 0 to 255 each

	Methods:
		rainbow(options)	cycle through colors. options.step (default 3) sets the speed. rainbow(0) stops and turns the LED off.
*/

import Timer from "timer";

const phases = Object.freeze([
	// red, purple, blue, cyan, green, orange, white, black
	[1, 0, -1, 0, 0, 1, 0, -1],
	[0, 0, 0, 1, 0, 0, 0, -1],
	[0, 1, 0, 0, -1, 0, 1, -1]
], true);

class LEDrgb {
	#r;
	#g;
	#b;
	#invert;
	#color = {r: 0, g: 0, b: 0};

	constructor(options) {
		const {target, invert, pin, ...io} = options;
		if (undefined !== target)
			this.target = target;
		this.#invert = invert ?? false;

		try {
			this.#r = new io.io({...io, pin: pin.r});
			this.#g = new io.io({...io, pin: pin.g});
			this.#b = new io.io({...io, pin: pin.b});
		}
		catch (e) {
			this.close();
			throw e;
		}
		this.on = 0;
	}
	close() {
		Timer.clear(this.#r?.timer);
		this.#r?.close();
		this.#g?.close();
		this.#b?.close();
		this.#r = this.#g = this.#b = undefined;
	}
	get on() {
		const {r, g, b} = this.#color;
		return (((r << 1) + r + (g << 2) + b) >> 3) / 255;		// integer luma, same as toGray in commodettoConvert.c
	}
	set on(value) {
		const color = this.#color;
		color.r = color.g = color.b = clamp(value, 1) * 255;
		this.color = color;
	}
	get color() {
		return {...this.#color};
	}
	set color(value) {
		const color = this.#color;
		const {r, g, b} = value;
		color.r = r;
		color.g = g;
		color.b = b;
		this.#write(this.#r, r);
		this.#write(this.#g, g);
		this.#write(this.#b, b);
	}
	#write(io, value) {
		const range = (1 << io.resolution) - 1;
		value = ((value * range) / 255) | 0;
		io.write(this.#invert ? range - value : value);
	}
	rainbow(value) {
		const r = this.#r;
		Timer.clear(r.timer);
		r.timer = undefined;

		if (0 === value) {
			this.on = 0;
			return;
		}

		const step = value?.step ?? 3;
		const rgb = [0, 0, 0], color = {r: 0, g: 0, b: 0};
		let phase = 0;

		r.timer = Timer.repeat(() => {
			let advance;
			for (let i = 0; i < 3; i++) {
				const direction = phases[i][phase];

				rgb[i] += direction * step;
				if (direction) {
					if (rgb[i] >= 255) {
						rgb[i] = 255;
						advance = true;
					}
					else if (rgb[i] <= 0) {
						rgb[i] = 0;
						advance = true;
					}
				}
			}
			if (advance)
				if (++phase >= phases[0].length) phase = 0;

			color.r = rgb[0], color.g = rgb[1], color.b = rgb[2];
			this.color = color;
		}, 33);
	}
}

function clamp(value, max) {
	value = Number(value);
	return (value > 0) ? ((value < max) ? value : max) : 0;
}

export default LEDrgb;
