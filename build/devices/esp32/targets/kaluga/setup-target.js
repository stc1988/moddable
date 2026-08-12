/*
 * Copyright (c) 2021-2026  Moddable Tech, Inc.
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

import config from "mc/config";
import Timer from "timer";

const BUTTON_TOLERANCE = 10;
const BUTTON_VALUES = Object.freeze([
	750 + BUTTON_TOLERANCE,
	615 + BUTTON_TOLERANCE,
	515 + BUTTON_TOLERANCE,
	347 + BUTTON_TOLERANCE,
	255 + BUTTON_TOLERANCE,
	119 + BUTTON_TOLERANCE
]);

// Analog resistor-ladder buttons on a single ADC pin
class LadderButton {
	static #state = {
		active: {},
		pushed: undefined,
		timer: undefined,
		analog: undefined
	};

	#button;
	#onPush;

	constructor(options) {
		this.#button = options.button;
		this.#onPush = options.onPush;

		if (LadderButton.#state.active[this.#button])
			throw new Error("in use");

		LadderButton.#state.active[this.#button] = this;

		if (LadderButton.#state.timer)
			return;

		const pin = config.buttonArray?.pin ?? device.pin.buttonArray;
		const analog = LadderButton.#state.analog = new device.io.Analog({ pin });
		const scale = 1023 / ((1 << analog.resolution) - 1);

		LadderButton.#state.timer = Timer.repeat(() => {
			const value = analog.read() * scale;
			if (value > BUTTON_VALUES[0]) {
				if (LadderButton.#state.pushed === undefined)
					return;
				LadderButton.#state.active[LadderButton.#state.pushed]?.#onPush?.(0);
				LadderButton.#state.pushed = undefined;
			}
			for (let i = 5; i >= 0; i--) {
				if (value < BUTTON_VALUES[i]) {
					if (i !== LadderButton.#state.pushed) {
						if (LadderButton.#state.pushed !== undefined)
							LadderButton.#state.active[LadderButton.#state.pushed]?.#onPush?.(0);
						LadderButton.#state.pushed = i;
						LadderButton.#state.active[i]?.#onPush?.(1);
					}
					break;
				}
			}
		}, config.buttonArray?.delay ?? 50);
	}

	close() {
		if (undefined === this.#button)
			return;

		delete LadderButton.#state.active[this.#button];
		this.#button = undefined;

		if (Object.keys(LadderButton.#state.active).length)
			return;

		Timer.clear(LadderButton.#state.timer);
		LadderButton.#state.timer = undefined;
		LadderButton.#state.analog?.close();
		LadderButton.#state.analog = undefined;
	}

	read() {
		return (LadderButton.#state.pushed === this.#button) ? 1 : 0;
	}

	get pressed() {
		return (LadderButton.#state.pushed === this.#button);
	}
}

function createLadder(button) {
	const i = button;
	return class {
		constructor(options) {
			return new LadderButton({
				...options,
				button: i
			});
		}
	};
}

// Expose ladder buttons as device.peripheral.button.A–F
device.peripheral.button.A = createLadder(0);
device.peripheral.button.B = createLadder(1);
device.peripheral.button.C = createLadder(2);
device.peripheral.button.D = createLadder(3);
device.peripheral.button.E = createLadder(4);
device.peripheral.button.F = createLadder(5);

if (config.touchpad?.pins) {
	device.peripheral.Touchpad = {};
	for (let x in config.touchpad.pins) {
		const pin = config.touchpad.pins[x];
		// touchpad pins map to ladder indices in original createTouch — keep if present
		device.peripheral.Touchpad[x] = createLadder(pin);
	}
}

export default function (done) {
	if (config.led?.rainbow) {
		const led = new device.peripheral.led.Default({});
		led.rainbow(3);
	}

	done?.();
}
