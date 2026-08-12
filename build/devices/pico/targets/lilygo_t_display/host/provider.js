/*
 * Copyright (c) 2022-2026  Moddable Tech, Inc.
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
import LED from "LED";

const device = {
	I2C: {
		default: {
			io: I2C,
			data: 4,
			clock: 5,
			port: 0
		}
	},
	Serial: {
		default: {
			io: Serial,
			receive: 13,
			transmit: 12
		}
	},
	SPI: {
		default: {
			io: SPI,
			clock: 14,
			in: 12,
			out: 15,
			port: 1
		}
	},
	Analog: {
		default: {
			io: Analog,
			pin: 26
		}
	},
	io: { Analog, Digital, DigitalBank, I2C, PulseCount, PWM, Serial, SMBus, SPI },
	pin: {
		button: 6,
		buttonA: 6,
		buttonB: 7,
		led: 25,
		backlight: 4,
		lcdPower: 22
	},
	peripheral: {
		Backlight: class {
			constructor() {
				return new Backlight({
					io: device.io.PWM,
					pin: device.pin.backlight
				});
			}
		},
		Button,
		button: {
			Default: class {
				constructor(options) {
					return new Button({
						...options,
						io: Digital,
						pin: device.pin.button,
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
						pin: device.pin.button,
						mode: Digital.InputPullUp,
						invert: true
					});
				}
			}
		},
		led: {
			Default: class {
				constructor(options) {
					return new LED({
						...options,
						io: device.io.PWM,
						pin: device.pin.led
					});
				}
			}
		},
		Power: {
			LCD: class {
				constructor() {
					return new Digital({
						io: Digital,
						pin: device.pin.lcdPower,
						mode: Digital.Output });
					}
			}
		}
	}
};

export default device;

