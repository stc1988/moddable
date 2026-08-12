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

import NeoPixel from "neopixel";
import Timer from "timer";

const phases = Object.freeze([
	// red, purple, blue, cyan, green, orange, white, black
	[1, 0, -1, 0, 0, 1, 0, -1],
	[0, 0, 0, 1, 0, 0, 0, -1],
	[0, 1, 0, 0, -1, 0, 1, -1]
], true);

class LEDNeoPixel extends NeoPixel {
	#timer;
	#value;
	#power;
	#color = {r:0,g:0,b:0};

	constructor(options) {
		super({...options});
		this.#power = options.power;
	}
	get on() {
		return this.#value;
	}
	set on(value) {
		this.#value = Number(value);
		value = (this.#value * 255) | 0;
		if (value) {
			this.#power?.write(1);
			super.setPixel(0, super.makeRGB(value, value, value));
		}
		else {
			this.#power?.write(0);
			super.setPixel(0, super.makeRGB(0, 0, 0));
		}
		super.update();
	}
	update() {
		this.#power?.write(1);
		super.update();
	}
	close() {
		if (this.#timer) {
			this.#power?.write(0);
			this.#power?.close();
			this.#timer?.close();
			this.#timer = undefined;
		}
		super.close();
	}
	get color() {
		return this.#color;
	}
	set color(value) {
		this.#color = value;
		this.#power?.write((value?.r + value?.g + value?.b) > 0);
		super.setPixel(0, super.makeRGB(value.r, value.g, value.b));
	}
	rainbow(value) {
		if (this.#timer) {
			this.#timer?.close();
			this.#timer = undefined;
		}

		if (value === 0) {
			this.off();
			return;
		}
		this.#power?.write(1);

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

			super.setPixel(0, super.makeRGB(rgb[0], rgb[1], rgb[2]));
			super.update();
		}, 33);
	}
}

export default LEDNeoPixel;
