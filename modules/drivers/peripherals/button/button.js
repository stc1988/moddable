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

class Button {
	#io;
	#onChanged;
	#debounce = 25;
	#debounceTimer;

	constructor(options) {
		options = {...options};
		if (options.onReadable || options.onWritable || options.onError)
			throw new Error;

		if (options.target)
			this.target = options.target;

		const Digital = options.io;
		if (options.onPush) {
			this.#onChanged = options.onPush;
			options.edge = Digital.Rising | Digital.Falling;
		}
		options.onReadable = () => this.#onEdge();

		this.#io = new Digital(options);
		this.#io.pressed = options.invert ? 0 : 1;
	}
	close() {
		Timer.clear(this.#debounceTimer);
		this.#debounceTimer = undefined;
		this.#io?.close();
		this.#io = undefined;
	}
	configure(options) {
		if (undefined !== options.debounce)
			this.#debounce = options.debounce;
	}
	get pressed() {
		return this.#io.read() == this.#io.pressed;
	}
	set onChanged(c) {
		this.#onChanged = c;
	}
	#onEdge() {
		if (undefined !== this.#debounceTimer)
			Timer.schedule(this.#debounceTimer, this.#debounce);
		else
			this.#debounceTimer = Timer.set(() => {
				this.#debounceTimer = undefined;
				if (this.#onChanged)
					this.#onChanged();
			}, this.#debounce);
	}
}

export default Button;
