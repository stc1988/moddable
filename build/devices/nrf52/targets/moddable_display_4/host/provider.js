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

import Button from "button";
import LED from "LED";
import JogDial from "jogdial";
import LIS3DH from "embedded:sensor/Accelerometer/LIS3DH";

const device = {
	I2C: {
		default: {
			io: I2C,
			data: 26,
			clock: 27
		}
	},
	Serial: {
		default: {
			io: Serial,
			port: 1,
			receive: 31,
			transmit: 30
		}
	},
	io: {Analog, Digital, DigitalBank, I2C, PulseCount, PWM, Serial, SMBus},
	pin: {
		button: 13,
		buttonA: 13,
		lcdPower: 23,
		led: 7,
		jogdial: {
			signal: 6,
			control: 8,
			button: 11
		},
		imu_int1: 14,
		imu_int2: 16
	},
	peripheral: {
		Button,
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
		},
		JogDial: class extends JogDial {
			#onTurn;
			#onPushAndTurn;
			constructor(options) {
				super({
					...options,
					jogdial: device.pin.jogdial,
					onTurn: delta => {
//						if (180 === screen?.rotation)
//							delta = -delta;
						this.#onTurn?.(delta);
					},
					onPushAndTurn: delta => {
//						if (180 === screen?.rotation)
//							delta = -delta;
						this.#onPushAndTurn?.(delta);
					}
				});
				this.#onTurn = options.onTurn ?? this.onTurn;
				this.#onPushAndTurn = options.onPushAndTurn ?? this.onPushAndTurn ?? this.#onTurn;
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
	},
	sensor: {
		IMU: class {
			constructor(options) {
				return new LIS3DH({
					...options,
					sensor: {
						...device.I2C.default,
						io: SMBus,
						address: 0x19
					}
				});
			}
			sample() {
				const result = super.sample();
				if (180 === screen?.rotation) {
					result.x = -result.x;
					result.y = -result.y;
				}
				return result;
			}
		}
	}
};

export default device;
