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
import PulseCount from "embedded:io/pulsecount";
import PulseWidth from "embedded:io/pulsewidth";
import PWM from "embedded:io/pwm";
import Serial from "embedded:io/serial";
import SMBus from "embedded:io/smbus";
import SPI from "embedded:io/spi";

import Button from "button";
import LEDneopixel from "LEDneopixel";

const device = {
	I2C: {
		default: {
			io: I2C,
			data: 2,
			clock: 1
		},
		internal: {
			io: I2C,
			data: 38,
			clock: 39
		}
	},
	SPI: {
		default: {
			io: SPI,
			port: 3,
			clock: 17,
			out: 21
		}
	},
	Analog: {
		default: {
			io: Analog,
			pin: 8
		}
	},
	io: {Analog, Digital, DigitalBank, I2C, PulseCount, PulseWidth, PWM, Serial, SMBus, SPI},
	pin: {
		button: 41,
		buttonA: 41,
		buttonFlash: 0,
		led: 35
	},
	peripheral: {
		Button,
		button: {
			A: class {
				constructor(options) {
					return new Button({
						...options,
						io: Digital,
						pin: device.pin.buttonA,
						mode: Digital.InputPullUp,
						invert: true
					});
				}
			},
			Default: class {
				constructor(options) {
					return new Button({
						...options,
						io: Digital,
						pin: device.pin.buttonFlash,
						mode: Digital.InputPullUp,
						invert: true
					});
				}
			},
			Flash: class {
				constructor(options) {
					return new Button({
						...options,
						io: Digital,
						pin: device.pin.buttonFlash,
						mode: Digital.InputPullUp,
						invert: true
					});
				}
			}
		},
		led: {
			Default: class {
				constructor(options) {
					const led = new LEDneopixel({
						...options,
						length: 1,
						pin: device.pin.led,
						order: "GRB"
					});
					led.brightness = 32;
					return led;
				}
			}
		}
	}
};

export default device;
