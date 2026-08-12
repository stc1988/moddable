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

class LED {
	#io;
	#invert;

	constructor(options) {
		let io;
		options = {...options};
		if (options.target)
			this.target = options.target;

		this.#io = io = new options.io(options);
		this.#invert = options.invert ?? false;
		this.on = 0;
	}
	close() {
		this.#io?.close();
		this.#io = undefined;
	}
	set on(value) {
		const range = (1 << this.#io.resolution) - 1;
		this.#io.value = Number(value);
		value = (this.#io.value * range) | 0;
		this.#io.write(this.#invert ? range - value : value);
	}
	get on() {
		return this.#io.value;
	}
}

export default LED;
