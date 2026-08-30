/*
 * Copyright (c) 2022  Moddable Tech, Inc.
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
import DigitalBank from "embedded:io/digitalbank";
import PWM from "embedded:io/pwm";

import Backlight from "backlight";
import Button from "button";
import LEDrgb from "led/rgb";

const device = {
	io: { Digital, DigitalBank, PWM },
	pin: {
		button: 18,
		buttonA: 18,
		buttonB: 19,
		buttonX: 17,
		buttonY: 16,
		buttonUP: 23,
		buttonDOWN: 20,
		buttonLEFT: 22,
		buttonRIGHT: 21,
		led: 14,
		led_r: 14,
		led_g: 13,
		led_b: 15,
		backlight: 12
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
					return new LEDrgb({
						...options,
						io: device.io.PWM,
						pin: { r: device.pin.led_r, g: device.pin.led_g, b: device.pin.led_b },
						invert: true
					});
				}
			}
		}
	}
};

export default device;

