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
	#debounce = 25;
	#debounceTimer;

	constructor(options) {
		options = {...options};
		if (options.onReadable || options.onWritable || options.onError)
			throw new Error;

		const {target, onPush, io: Digital} = options;
		if (target)
			this.target = target;

		options.edge = Digital.Rising | Digital.Falling;
		options.onReadable = () => {
			this.#debounceTimer ??= Timer.set(() => {
				this.#debounceTimer = undefined;
				this.#io.onChanged?.();
			}, this.#debounce);
		};

		this.#io = new Digital(options);
		if (onPush)
			this.#io.onChanged = onPush;
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
		return this.#io.read();
	}
	set onChanged(c) {
		this.#io.onChanged = c;
	}
}

export default Button;
