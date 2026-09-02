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
	#timer;

	constructor(input) {
		const {target, onPush, io: Digital, ...options} = input;
		if (options.onReadable || options.onWritable || options.onError)
			throw new Error;

		if (target)
			this.target = target;

		options.edge = Digital.Rising | Digital.Falling;
		options.onReadable = () => {
			this.#timer ??= Timer.set(() => {
				this.#timer = undefined;
				this.#io.onChanged?.call(this);
			}, this.#io.debounce);
		};

		this.#io = new Digital(options);
		this.#io.debounce = 25;
		if (onPush)
			this.#io.onChanged = onPush;
	}
	close() {
		Timer.clear(this.#timer);
		this.#timer = undefined;
		this.#io?.close();
		this.#io = undefined;
	}
	configure(options) {
		if (undefined !== options.debounce)
			this.#io.debounce = options.debounce;
	}
	get pressed() {
		return this.#io.read();
	}
	set onChanged(c) {
		this.#io.onChanged = c;
	}
}

export default Button;
