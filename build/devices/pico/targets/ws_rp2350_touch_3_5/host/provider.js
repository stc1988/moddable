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
import SMBus from "embedded:io/smbus";
import SPI from "embedded:io/spi";
import Touch from "embedded:sensor/Touch/FT6x36";
import RTC from "embedded:RTC/PCF85063";
import QMI8658 from "embedded:sensor/Accelerometer-Gyroscope/QMI8658";

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
		if (value <= 0)
			value = 0;
		else if (value >= 1)
			value = 1023;
		else 
			value *= 1023;
		this.#io.write(value);
	}
	write(value) {		// compatibility
		this.brightness = value / 100;
	}
}

const device = {
	I2C: {
		default: {
			io: I2C,
			data: 34,
			clock: 35,
			port: 1
		}
	},
	SPI: {
		default: {
			io: SPI,
			clock: 18,
			out: 19,
			port: 0
		}
	},
	io: { Analog, Digital, DigitalBank, I2C, PulseCount, PWM, SMBus, SPI },
	pin: {
		backlight: 22
	} ,
	peripheral: {
		Backlight: class {
			constructor() {
				return new Backlight({pin: device.pin.backlight });
			}
		},
		RTC: class {
			constructor(options) {
				return new RTC({
					...options,
					clock: {
						...device.I2C.default,
						io: device.io.SMBus
					}
				});
			}
		}
	},
	sensor: {
		Touch: class {
			constructor(options) {
				const result = new Touch({
					...options,
					sensor: {
						...device.I2C.default,
						io: device.io.SMBus
					},
					reset: {
						io: Digital,
						mode: Digital.Output,
						pin: 24
					},
					interrupt: {
						io: Digital,
						mode: Digital.Input,
						pin: 25
					}
				});
				result.configure({active: true, threshold: 64, timeout: 10});
				return result;
			}
		},
		IMU: class extends QMI8658 {
			constructor(options) {
				super({
					...options,
					sensor: {
						...device.I2C.default,
						address: 0x6b,
						io: device.io.SMBus
					}
				});
			}
			sample() {
				const sample = super.sample();
				sample.accelerometer.z *= -1;
				[ sample.accelerometer.x, sample.accelerometer.y ] = [ sample.accelerometer.y * -1, sample.accelerometer.x * -1 ];
				return sample;
			}
		}
	}
};

export default device;

