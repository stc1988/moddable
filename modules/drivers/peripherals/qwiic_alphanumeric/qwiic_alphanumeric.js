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
	SparkFun Qwiic Alphanumeric Display (HT16K33 based 14-segment)

	Supports one or more daisy-chained displays (up to 4) via unique I2C addresses
	(0x70–0x73 set by A0/A1 jumpers).

	https://www.sparkfun.com/products/16919 (and color variants)

	write(string) displays the string. When the string is longer than the
	available digits (4 × number of displays) it automatically scrolls at a
	configurable rate (default 250 ms per step).
*/

import Timer from "timer";

const DEFAULT_ADDRESS = 0x70;
const CMD_SYSTEM_SETUP   = 0b0010_0000;
const CMD_DISPLAY_SETUP  = 0b1000_0000;
const CMD_DIMMING_SETUP  = 0b1110_0000;

// 14-segment font for printable ASCII (space .. ~) + unknown
// Bit order: n m l k j i h g f e d c b a   (bit 0 = segment A)
const FONT = Object.freeze([
	0b00000000000000, // ' '
	0b00001000001000, // '!'
	0b00001000000010, // '"'
	0b01001101001110, // '#'
	0b01001101101101, // '$'
	0b10010000100100, // '%'
	0b00110011011001, // '&'
	0b00001000000000, // '''
	0b00000000111001, // '('
	0b00000000001111, // ')'
	0b11111010000000, // '*'
	0b01001101000000, // '+'
	0b10000000000000, // ','
	0b00000101000000, // '-'
	0b00000000000000, // '.'
	0b10010000000000, // '/'
	0b00000000111111, // '0'
	0b00010000000110, // '1'
	0b00000101011011, // '2'
	0b00000101001111, // '3'
	0b00000101100110, // '4'
	0b00000101101101, // '5'
	0b00000101111101, // '6'
	0b01010000000001, // '7'
	0b00000101111111, // '8'
	0b00000101100111, // '9'
	0b00000000000000, // ':'
	0b10001000000000, // ';'
	0b00110000000000, // '<'
	0b00000101001000, // '='
	0b01000010000000, // '>'
	0b01000100000011, // '?'
	0b00001100111011, // '@'
	0b00000101110111, // 'A'
	0b01001100001111, // 'B'
	0b00000000111001, // 'C'
	0b01001000001111, // 'D'
	0b00000101111001, // 'E'
	0b00000101110001, // 'F'
	0b00000100111101, // 'G'
	0b00000101110110, // 'H'
	0b01001000001001, // 'I'
	0b00000000011110, // 'J'
	0b00110001110000, // 'K'
	0b00000000111000, // 'L'
	0b00010010110110, // 'M'
	0b00100010110110, // 'N'
	0b00000000111111, // 'O'
	0b00000101110011, // 'P'
	0b00100000111111, // 'Q'
	0b00100101110011, // 'R'
	0b00000110001101, // 'S'
	0b01001000000001, // 'T'
	0b00000000111110, // 'U'
	0b10010000110000, // 'V'
	0b10100000110110, // 'W'
	0b10110010000000, // 'X'
	0b01010010000000, // 'Y'
	0b10010000001001, // 'Z'
	0b00000000111001, // '['
	0b00100010000000, // '\'
	0b00000000001111, // ']'
	0b10100000000000, // '^'
	0b00000000001000, // '_'
	0b00000010000000, // '`'
	0b00000101011111, // 'a'
	0b00100001111000, // 'b'
	0b00000101011000, // 'c'
	0b10000100001110, // 'd'
	0b00000001111001, // 'e'
	0b00000001110001, // 'f'
	0b00000110001111, // 'g'
	0b00000101110100, // 'h'
	0b01000000000000, // 'i'
	0b00000000001110, // 'j'
	0b01111000000000, // 'k'
	0b01001000000000, // 'l'
	0b01000101010100, // 'm'
	0b00100001010000, // 'n'
	0b00000101011100, // 'o'
	0b00010001110001, // 'p'
	0b00100101100011, // 'q'
	0b00000001010000, // 'r'
	0b00000110001101, // 's'
	0b00000001111000, // 't'
	0b00000000011100, // 'u'
	0b10000000010000, // 'v'
	0b10100000010100, // 'w'
	0b10110010000000, // 'x'
	0b00001100001110, // 'y'
	0b10010000001001, // 'z'
	0b10000011001001, // '{'
	0b01001000000000, // '|'
	0b00110100001001, // '}'
	0b00000101010010, // '~'
	0b11111111111111, // unknown
]);

class QwiicAlphanumericDisplay {
	#ios;
	#ram;					// Uint8Array, 16 bytes per display
	#digitCount;			// 4 * number of displays
	#rate = 250;			// ms per scroll step
	#direction = 1;			// or -1
	#timer;
	#content = "";
	#offset = 0;
	#onError;

	/**
	 * options:
	 *  addresses:	[0x70] - I²C addresses of the chained displays
	 *      in left-to-right physical order
	 * 	rate:		250 - milliseconds between scroll steps
		direction:	1 - scroll direction (-1 scroll right)
	 */
	constructor(options) {
		const addresses = options.addresses ?? [DEFAULT_ADDRESS];
		if (!Array.isArray(addresses) || addresses.length < 1 || addresses.length > 4)
			throw new Error("addresses must be an array of 1–4 I2C addresses");

		this.#onError = options.onError;

		this.#ios = addresses.map(addr => {
			const io = new options.sensor.io({
				hz: 400_000,
				address: addr,
				...options.sensor
			});
			return io;
		});

		this.#digitCount = this.#ios.length * 4;
		this.#ram = new Uint8Array(16 * this.#ios.length);

		try {
			for (const io of this.#ios) {
				io.write(Uint8Array.of(CMD_SYSTEM_SETUP | 1));
				Timer.delay(1);
				io.write(Uint8Array.of(CMD_DIMMING_SETUP | 15));
				io.write(Uint8Array.of(CMD_DISPLAY_SETUP | 1));
			}
			this.clear();
		}
		catch (e) {
			this.close();
			throw e;
		}
	}

	close() {
		this.#stopScroll();
		if (this.#ios) {
			for (const io of this.#ios)
				io.close();
			this.#ios = undefined;
		}
		this.#ram = undefined;
	}

	/**
	 * Change the scroll speed (milliseconds per character step).
	 * Takes effect on the next write() that needs scrolling.
	 */
	configure(options = {}) {
		if (undefined !== options.rate) {
			this.#rate = Math.max(50, options.rate | 0);
			if (this.#timer)
				this.#restartScroll();
		}
		if (undefined !== options.direction) {
			this.#direction = (options.direction == -1) ? -1 : 1;
			if (this.#timer)
				this.#restartScroll();
		}
		if (undefined !== options.brightness) {
			const duty = Math.max(0, Math.min(15, options.brightness | 0));
			for (const io of this.#ios)
				io.write(Uint8Array.of(CMD_DIMMING_SETUP | duty));
		}
	}

	/**
	 * Write a string to the display(s).
	 * If the string fits in the available digits it is shown statically
	 * (right-padded with spaces).  Otherwise continuous left-scrolling
	 * begins at the configured rate.
	 *
	 * The characters '.' and ':' are treated as modifiers: they light the
	 * decimal point / colon of the preceding digit and do not consume a
	 * digit position (matching the SparkFun Arduino library behaviour).
	 */
	write(string) {
		string = String(string);

		this.#stopScroll();
		this.#content = string;
		this.#offset = 0;

		const visible = this.#buildVisible(string, 0);
		this.#render(visible);

		if (this.#contentNeedsScroll(string))
			this.#startScroll();
	}

	clear() {
		this.#stopScroll();
		this.#content = "";
		this.#ram.fill(0);
		this.#update();
	}

	/** Turn the display(s) on or off without losing the current content. */
	setDisplay(on = true) {
		const cmd = CMD_DISPLAY_SETUP | (on ? 1 : 0);
		for (const io of this.#ios)
			io.write(Uint8Array.of(cmd));
	}

	// ------------------------------------------------------------------
	// internal helpers
	// ------------------------------------------------------------------

	#contentNeedsScroll(str) {
		// Count only characters that consume a digit position
		let digits = 0;
		for (let i = 0; i < str.length; i++) {
			const c = str.charCodeAt(i);
			if (c !== 0x2E && c !== 0x3A)		// '.' ':'
				digits++;
		}
		return digits > this.#digitCount;
	}

	#buildVisible(str, startOffset) {
		// Produce a string of exactly #digitCount displayable characters
		// (ignoring '.' and ':' for the length count, but preserving them)
		const out = [];
		let i = startOffset % Math.max(str.length, 1);
		let placed = 0;

		// Add a trailing space so the scroll has a clean gap
		const src = str + "    ";
		const len = src.length;

		while (placed < this.#digitCount) {
			const c = src.charAt(i % len);
			out.push(c);
			if (c !== "." && c !== ":")
				placed++;
			i++;
		}
		return out.join("");
	}

	#render(str) {
		this.#ram.fill(0);
		let digit = 0;

		for (let i = 0; i < str.length && digit < this.#digitCount; i++) {
			const ch = str.charCodeAt(i);

			if (ch === 0x2E) {					// '.'
				this.#setDecimal(digit ? digit - 1 : 0);
				continue;
			}
			if (ch === 0x3A) {					// ':'
				this.#setColon(digit ? digit - 1 : 0);
				continue;
			}

			const segs = this.#lookup(ch);
			this.#illuminateChar(segs, digit);
			digit++;
		}
		this.#update();
	}

	#lookup(ch) {
		if (ch === 0x20) return FONT[0];					// space
		if (ch >= 0x21 && ch <= 0x7E) return FONT[ch - 0x21 + 1];
		return FONT[95];									// unknown → all on
	}

	#illuminateChar(segments, digit) {
		for (let bit = 0; bit < 14; bit++) {
			if (segments & (1 << bit))
				this.#illuminateSegment(bit, digit);
		}
	}

	/**
	 * Map a logical segment (0 = A … 13 = N) onto the HT16K33 RAM layout
	 * used by the SparkFun Qwiic board.  Logic mirrors the Arduino library.
	 */
	#illuminateSegment(bitIndex, digit) {
		let com = bitIndex;
		if (com > 6)
			com -= 7;
		// special cases from the original library
		if (bitIndex === 8)			// 'I'
			com = 0;
		else if (bitIndex === 7)	// 'H'
			com = 1;

		let row = digit % 4;
		if (bitIndex > 6)
			row += 4;

		const offset = (digit / 4 | 0) * 16;
		let adr = com * 2 + offset;
		if (row > 7) {
			adr++;
			row -= 8;
		}
		this.#ram[adr] |= 1 << row;
	}

	#setDecimal(digit) {
		const disp = (digit / 4 | 0);
		const adr = 0x03 + disp * 16;
		this.#ram[adr] |= 0x01;
	}

	#setColon(digit) {
		const disp = (digit / 4 | 0);
		const adr = 0x01 + disp * 16;
		this.#ram[adr] |= 0x01;
	}

	#update() {
		for (let i = 0; i < this.#ios.length; i++) {
			const io = this.#ios[i];
			const slice = this.#ram.subarray(i * 16, (i + 1) * 16);
			const buf = new Uint8Array(17);
			buf[0] = 0;					// RAM start address
			buf.set(slice, 1);
			io.write(buf);
		}
	}

	#startScroll() {
		this.#timer = Timer.repeat(() => {
			this.#offset = (this.#offset + this.#direction) % (this.#content.length + 1);
			if (this.#offset < 0)
				this.#offset = this.#content.length;
			const visible = this.#buildVisible(this.#content, this.#offset);
			this.#render(visible);
		}, this.#rate);
	}

	#restartScroll() {
		this.#stopScroll();
		if (this.#contentNeedsScroll(this.#content))
			this.#startScroll();
	}

	#stopScroll() {
		Timer.clear(this.#timer);
		this.#timer = undefined;
	}

    static { 
        this.prototype[Symbol.dispose] = this.prototype.close;
    } 
}

export default QwiicAlphanumericDisplay;
