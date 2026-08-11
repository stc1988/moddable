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
	Titan Micro Electronics TM1637 LED display driver

	Supports displays with up to 6 digits. Most commonly available modules have
	4 digits, with the colon connected to bit 7 of the second digit.

	write(string) displays the string. When the string is longer than the
	available digits it automatically scrolls at a configurable rate
	(default 250 ms per step).
*/

import Timer from "timer";

const CMD_DATA = 0x40;
const CMD_ADDRESS = 0xC0;
const CMD_DISPLAY = 0x80;
const DISPLAY_ON = 0x08;
const MAX_DIGITS = 6;

// 7-segment font. Bit order: dp g f e d c b a (bit 0 = segment A)
const FONT = Object.freeze({
	0x20: 0b00000000, // space
	0x2D: 0b01000000, // -
	0x30: 0b00111111, // 0
	0x31: 0b00000110, // 1
	0x32: 0b01011011, // 2
	0x33: 0b01001111, // 3
	0x34: 0b01100110, // 4
	0x35: 0b01101101, // 5
	0x36: 0b01111101, // 6
	0x37: 0b00000111, // 7
	0x38: 0b01111111, // 8
	0x39: 0b01101111, // 9
	0x3D: 0b01001000, // =
	0x41: 0b01110111, // A
	0x42: 0b01111100, // B
	0x43: 0b00111001, // C
	0x44: 0b01011110, // D
	0x45: 0b01111001, // E
	0x46: 0b01110001, // F
	0x47: 0b00111101, // G
	0x48: 0b01110110, // H
	0x49: 0b00000110, // I
	0x4A: 0b00011110, // J
	0x4B: 0b01110101, // K
	0x4C: 0b00111000, // L
	0x4D: 0b00110111, // M
	0x4E: 0b01010100, // N
	0x4F: 0b00111111, // O
	0x50: 0b01110011, // P
	0x51: 0b01100111, // Q
	0x52: 0b01010000, // R
	0x53: 0b01101101, // S
	0x54: 0b01111000, // T
	0x55: 0b00111110, // U
	0x56: 0b00111110, // V
	0x57: 0b00101010, // W
	0x58: 0b01110110, // X
	0x59: 0b01101110, // Y
	0x5A: 0b01011011, // Z
	0x5F: 0b00001000  // _
});

class TM1637Display {
	#clock;
	#data;
	#ram;
	#digitCount;
	#brightness = 7;
	#displayOn = true;
	#rate = 250;
	#direction = 1;
	#timer;
	#content = [];
	#offset = 0;
	#onError;

	/**
	 * options:
	 *  sensor:    Digital IO options with clock and data pins
	 *  digits:    4 - number of display digits (1–6)
	 *  rate:      250 - milliseconds between scroll steps
	 *  direction: 1 - scroll direction (-1 scroll right)
	 *  brightness: 7 - display brightness (0–7)
	 */
	constructor(options) {
		const sensor = options?.sensor;
		if (!sensor?.io || (undefined === sensor.clock) || (undefined === sensor.data))
			throw new Error("sensor with io, clock, and data is required");

		const digits = options.digits ?? 4;
		if (!Number.isInteger(digits) || (digits < 1) || (digits > MAX_DIGITS))
			throw new RangeError("digits must be an integer from 1 to 6");

		this.#onError = options.onError;
		if (undefined !== options.rate)
			this.#rate = Math.max(50, options.rate | 0);
		if (undefined !== options.direction)
			this.#direction = (options.direction === -1) ? -1 : 1;
		if (undefined !== options.brightness)
			this.#brightness = Math.max(0, Math.min(7, options.brightness | 0));

		try {
			const {io: Digital, clock, data, ...ioOptions} = sensor;
			this.#clock = new Digital({
				...ioOptions,
				pin: clock,
				mode: Digital.OutputOpenDrain,
				initialValue: 1
			});

			this.#data = new Digital({
				...ioOptions,
				pin: data,
				mode: Digital.OutputOpenDrain,
				initialValue: 1
			});

			this.#digitCount = digits;
			this.#ram = new Uint8Array(digits);

			this.#updateDisplayControl();
			this.clear();
		}
		catch (e) {
			this.close();
			throw e;
		}
	}

	close() {
		this.#stopScroll();
		this.#clock?.close();
		this.#clock = undefined;
		this.#data?.close();
		this.#data = undefined;
		this.#ram = undefined;
		this.#content = undefined;
	}

	/**
	 * Change the scroll speed (milliseconds per character step), direction,
	 * or display brightness. Scroll changes take effect immediately.
	 */
	configure(options = {}) {
		const restartScroll = !!this.#timer && ((undefined !== options.rate) || (undefined !== options.direction));

		if (undefined !== options.rate) {
			this.#rate = Math.max(50, options.rate | 0);
		}
		if (undefined !== options.direction) {
			this.#direction = (options.direction === -1) ? -1 : 1;
		}
		if (restartScroll) {
			this.#stopScroll();
			if (this.#contentNeedsScroll())
				this.#startScroll();
		}
		if (undefined !== options.brightness) {
			this.#brightness = Math.max(0, Math.min(7, options.brightness | 0));
			this.#updateDisplayControl();
		}
	}

	/**
	 * Write a string to the display. If the string fits it is shown statically
	 * (right-padded with spaces). Otherwise continuous scrolling begins.
	 *
	 * The characters '.' and ':' light bit 7 of the preceding digit and do not
	 * consume a digit position. On common clock modules, "12:34" lights the
	 * colon because bit 7 of the second digit is connected to it.
	 */
	write(string) {
		if (typeof string !== "string")
			string = String(string);

		this.#stopScroll();

		const content = [];
		let leadingModifier = false;
		for (let i = 0; i < string.length; i++) {
			let character = string.charCodeAt(i);
			if ((character === 0x2E) || (character === 0x3A)) { // '.' ':'
				if (content.length)
					content[content.length - 1] |= 0x80;
				else
					leadingModifier = true;
				continue;
			}

			if ((character >= 0x61) && (character <= 0x7A))
				character -= 0x20;
			let segments = FONT[character] ?? 0;
			if (leadingModifier) {
				segments |= 0x80;
				leadingModifier = false;
			}
			content.push(segments);
		}

		this.#content = content;
		this.#offset = 0;
		this.#render();

		if (this.#contentNeedsScroll())
			this.#startScroll();
	}

	clear() {
		this.#stopScroll();
		this.#content = [];
		this.#ram.fill(0);
		this.#update();
	}

	/** Turn the display on or off without losing the current content. */
	setDisplay(on = true) {
		this.#displayOn = !!on;
		this.#updateDisplayControl();
	}

	// ------------------------------------------------------------------
	// internal helpers
	// ------------------------------------------------------------------

	#contentNeedsScroll() {
		return this.#content.length > this.#digitCount;
	}

	#render() {
		this.#ram.fill(0);

		if (this.#contentNeedsScroll()) {
			const cycleLength = this.#content.length + this.#digitCount;
			for (let digit = 0; digit < this.#digitCount; digit++) {
				const index = (this.#offset + digit) % cycleLength;
				if (index < this.#content.length)
					this.#ram[digit] = this.#content[index];
			}
		}
		else {
			for (let digit = 0; digit < this.#content.length; digit++)
				this.#ram[digit] = this.#content[digit];
		}

		this.#update();
	}

	#update() {
		try {
			this.#command(CMD_DATA);
			this.#start();
			try {
				this.#writeByte(CMD_ADDRESS);
				for (let i = 0; i < this.#ram.length; i++)
					this.#writeByte(this.#ram[i]);
			}
			finally {
				this.#stop();
			}
		}
		catch (e) {
			this.#onError?.(e);
		}
	}

	#updateDisplayControl() {
		this.#command(CMD_DISPLAY | (this.#displayOn ? (DISPLAY_ON | this.#brightness) : 0));
	}

	#startScroll() {
		this.#timer = Timer.repeat(() => {
			const cycleLength = this.#content.length + this.#digitCount;
			this.#offset = (this.#offset + this.#direction) % cycleLength;
			if (this.#offset < 0)
				this.#offset += cycleLength;
			this.#render();
		}, this.#rate);
	}

	#stopScroll() {
		if (this.#timer) {
			Timer.clear(this.#timer);
			this.#timer = undefined;
		}
	}

	#command(command) {
		this.#start();
		try {
			this.#writeByte(command);
		}
		finally {
			this.#stop();
		}
	}

	#start() {
		this.#clock.write(1);
		this.#data.write(1);
		this.#data.write(0);
		this.#clock.write(0);
	}

	#stop() {
		this.#clock.write(0);
		this.#data.write(0);
		this.#clock.write(1);
		this.#data.write(1);
	}

	#writeByte(value) {
		for (let bit = 0; bit < 8; bit++) {
			this.#clock.write(0);
			this.#data.write((value >> bit) & 1);
			this.#clock.write(1);
		}

		this.#clock.write(0);
		this.#data.write(1);
		this.#clock.write(1);
		this.#clock.write(0);
	}

	static {
		TM1637Display.prototype[Symbol.dispose] = TM1637Display.prototype.close;
	}
}

export default TM1637Display;
