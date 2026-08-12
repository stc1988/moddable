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

import Timer from "timer";

const phases = Object.freeze([
    // red, purple, blue, cyan, green, orange, white, black
    [1, 0, -1, 0, 0, 1, 0, -1],
    [0, 0, 0, 1, 0, 0, 0, -1],
    [0, 1, 0, 0, -1, 0, 1, -1]
], true);

class LEDrgb {
	#led_r;
	#led_g;
	#led_b;
	#timer;
	#invert;
	#color = {r:0,g:0,b:0};

	constructor(options) {
		let io;
		options = {...options};

		this.#led_r = new options.io({options, pin: options.pin.r});
		this.#led_g = new options.io({options, pin: options.pin.g});
		this.#led_b = new options.io({options, pin: options.pin.b});
		this.#invert = options.invert ?? false;
		this.on = 0;
	}
	close() {
		this.#led_r?.close();
		this.#led_g?.close();
		this.#led_b?.close();
		this.#led_r = undefined;
		this.#led_g = undefined;
		this.#led_b = undefined;
	}
	set on(value) {
		this.color = { r: 255 * value, g: 255 * value, b: 255 * value };
	}
	get on() {
		return ((this.#color.r + this.#color.g + this.#color.b) / 3) / 255;
	}
	get color() {
		return this.#color;
	}
	set color(value) {
		let range = (1 << this.#led_r.resolution) - 1;
		this.#color.r = Number(value.r);
		value.r = ((this.#color.r * range) / 255) | 0;
		this.#led_r.write(this.#invert ? range - value.r : value.r);
		range = (1 << this.#led_g.resolution) - 1;
		this.#color.g = Number(value.g);
		value.g = ((this.#color.g * range) / 255) | 0;
		this.#led_g.write(this.#invert ? range - value.g : value.g);
		range = (1 << this.#led_b.resolution) - 1;
		this.#color.b = Number(value.b);
		value.b = ((this.#color.b * range) / 255) | 0;
		this.#led_b.write(this.#invert ? range - value.b : value.b);
	}
	rainbow(value) {
		if (this.#timer) {
			this.#timer?.close();
			this.#timer = undefined;
		}

		const step = value?.step ?? 3;
		let rgb = [0, 0, 0];
		let phase = 0;

		this.#timer = Timer.repeat(() => {
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

			this.color = { r:rgb[0], g:rgb[1], b:rgb[2]};
		}, 33);
	}
}

export default LEDrgb;
