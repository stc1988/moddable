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

import Digital from "embedded:io/digital";
import PulseCount from "embedded:io/pulsecount";

class JogDial {
	#pulse;
	#button;
	#onPush;
	#onTurn;
	#onPushAndTurn;

	constructor(options = {}) {
		if (options.target)
			this.target = options.target;
		this.#onPush = options.onPush ?? this.onPush;
		this.#onTurn = options.onTurn ?? this.onTurn;
		this.#onPushAndTurn = options.onPushAndTurn ?? this.onPushAndTurn ?? this.#onTurn;

		this.#pulse = new PulseCount({
			signal: options.jogdial.signal,
			control: options.jogdial.control,
			filter: 1000,
			target: this,
			onReadable() {
				const target = this.target, pulse = target.#pulse;
				const value = -pulse.read();
				const delta = value - pulse.previous;
				pulse.previous = value;

				if (target.#button.previous)
					target.#onTurn(delta);
				else
					target.#onPushAndTurn(delta);
			}
		});
		this.#pulse.previous = -this.#pulse.read();

		this.#button = new Digital({
			pin: options.jogdial.button,
			mode: Digital.InputPullUp,
			edge: Digital.Rising | Digital.Falling,
			target: this,
			onReadable() {
				const target = this.target;
				const value = target.#button.read();
				if (value === target.#button.previous)
					return;
				target.#button.previous = value;
				target.#onPush(value);
			}
		});
		this.#button.previous = this.#button.read();
	}
	close() {
		this.#button?.close();
		this.#pulse?.close();
		this.#button =
		this.#pulse = undefined;
	}
}

export default JogDial;
