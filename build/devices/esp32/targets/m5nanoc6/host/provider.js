/*
 * Copyright (c) 2024-2026  Moddable Tech, Inc.
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
import I2C from "embedded:io/i2c";
import PWM from "embedded:io/pwm";
import SMBus from "embedded:io/smbus";

import Button from "button";
import LED from "LED";
import LEDneopixel from "LEDneopixel";

const device = {
	I2C: {
		default: {
			io: I2C,
			data: 2,
			clock: 1
		}
	},
	io: {Digital, I2C, PWM, SMBus},
	pin: {
		button: 9,
		buttonA: 9,
		led: 20,
		ledPower: 19,
		ledBlue: 7,
		IRTX: 3
	},
	peripheral: {
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
		Power: {
			LED: class {
				constructor() {
					return new Digital({
						io: Digital,
						pin: device.pin.ledPower,
						mode: Digital.Output
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
						order: "GRB",
						power: new device.peripheral.Power.LED()
					});
					led.brightness = 32;
					return led;
				}
			},
			RGB: class {
				constructor(options) {
					return new device.peripheral.led.Default(options);
				}
			},
			Blue: class {
				constructor(options) {
					return new LED({
						...options,
						io: PWM,
						pin: device.pin.ledBlue
					});
				}
			}
		}
	}
};

export default device;
