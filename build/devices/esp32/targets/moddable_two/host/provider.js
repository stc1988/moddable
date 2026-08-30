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

import Analog from "embedded:io/analog";
import Digital from "embedded:io/digital";
import DigitalBank from "embedded:io/digitalbank";
import I2C from "embedded:io/i2c";
import PulseCount from "embedded:io/pulsecount";
import PWM from "embedded:io/pwm";
import Serial from "embedded:io/serial";
import SMBus from "embedded:io/smbus";
import SPI from "embedded:io/spi";

import Backlight from "backlight";
import Button from "button";
import LED from "led/pwm";
import Touch from "embedded:sensor/Touch/FT6x06";

class ButtonA {
	constructor(options) {
		return new Button({
			...options,
			io: device.io.Digital,
			pin: device.pin.buttonA,
			mode: Digital.InputPullUp,
			activeLow: true
		});
	}
}

const device = {
	I2C: {
		default: {
			io: I2C,
			data: 21,
			clock: 22
		}
	},
	Serial: {
		default: {
			io: Serial,
			port: 1,
			receive: 3,
			transmit: 1
		}
	},
	SPI: {
		default: {
			io: SPI,
			clock: 14,
			in: 12,
			out: 13,
			port: 1
		}
	},
	Analog: {
		default: {
			io: Analog,
			pin: 33
		}
	},
	io: {Analog, Digital, DigitalBank, I2C, PulseCount, PWM, Serial, SMBus, SPI},
	pin: {
		button: 0,
		buttonA: 0,
		led: 2,
		backlight: 18,
		displayDC: 2,
		displaySelect: 15
	},
	peripheral: {
		button: {
			Default: ButtonA,
			A: ButtonA
		},
		Backlight: class {
			constructor() {
				return new Backlight({
					io: device.io.PWM,
					pin: device.pin.backlight,
					invert: true
				});
			}
		},
		led: {
			Default: class {
				constructor() {
					return new LED({
						io: PWM,
						pin: device.pin.led,
						invert: true
					});
				}
			}
		}
	},
	sensor: {
		Touch: class {
			constructor(options) {
				const result = new Touch({
					sensor: {
						...device.I2C.default,
						io: device.io.SMBus
					},
					...options
				});
				result.configure({threshold: 20});
				return result;
			}
		}
	}
};

export default device;
