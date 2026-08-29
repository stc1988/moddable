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
class Backlight {
	#io;
	#brightness = 1.0;

	constructor(options) {
		this.#io = new options.io(options);
		if (options.invert)
			this.#io.invert = true;
	}
	close() {
		this.#io?.close();
		this.#io = undefined;
	}
	set brightness(value) {
		if (value <= 0)
			value = 0;
		else if (value >= 1)
			value = 1023;
		else
			value *= 1023;
		if (this.#io.invert)
			value = 1023 - value;
		this.#io.write(value);
		this.#brightness = value / 1024;
	}
	get brightness() {
		return this.#brightness;
	}
	write(value) {
		this.brightness = value / 100;
	}
}

export default Backlight;
