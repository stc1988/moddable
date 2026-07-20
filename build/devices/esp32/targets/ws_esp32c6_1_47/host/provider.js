/*
 * Copyright (c) 2023-2026  Moddable Tech, Inc.
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

import Analog from "embedded:io/analog";
import Digital from "embedded:io/digital";
import DigitalBank from "embedded:io/digitalbank";
import I2C from "embedded:io/i2c";
// import PulseCount from "embedded:io/pulsecount";
import PWM from "embedded:io/pwm";
import Serial from "embedded:io/serial";
import SMBus from "embedded:io/smbus";
import SPI from "embedded:io/spi";

class Backlight {
	#io;

	constructor(options) {
		this.#io = new PWM(options);
	}
	close() {
		this.#io?.close();
		this.#io = undefined;
	}
	set brightness(value) {
		value = 1 - value;
		if (value <= 0)
			value = 0;
		else if (value >= 1)
			value = 1023;
		else
			value *= 1023;
		this.#io.write(value);
	}
	write(value) {
		this.brightness = value / 100;
	}
}

const device = {
	I2C: {
		default: {
			io: I2C,
			data: 4,
			clock: 5
		}
	},
	Serial: {
		default: {
			io: Serial,
			port: 0,
			receive: 17,
			transmit: 16
		}
	},
	SPI: {
		default: {
			io: SPI,
			clock: 18,
			in: 14,
			out: 15,
			port: 2
		}
	},
	Analog: {
		default: {
			io: Analog,
			pin: 0
		}
	},
	io: {Analog, Digital, DigitalBank, I2C, PWM, Serial, SMBus, SPI},
	pin: {
		button: 9,
		buttonA: 9,
		backlight: 22,
		displayDC: 15,
		displaySelect: 14
	},
	peripheral: {
		Backlight: class {
			constructor() {
				return new Backlight({pin: device.pin.backlight});
			}
		}
	}
};

export default device;
