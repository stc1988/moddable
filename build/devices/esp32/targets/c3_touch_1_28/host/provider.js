/*
 * Copyright (c) 2026  Satoshi Tanaka
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
import Serial from "embedded:io/serial";
import SMBus from "embedded:io/smbus";
import Touch from "embedded:sensor/Touch/CST816";

const device = {
	I2C: {
		internal: {
			io: I2C,
			port: "I2C_NUM_0",
			data: 4,
			clock: 5
		}
	},
	Serial: {
		default: {
			io: Serial,
			port: 1,
			receive: 20,
			transmit: 21
		}
	},
	io: { Analog, Digital, DigitalBank, I2C, Serial, SMBus },
	pin: {},
	sensor: {
		Touch: class {
			constructor(options) {
				const result = new Touch({
					...options,
					sensor: {
						...device.I2C.internal,
						io: device.io.SMBus
					},
					reset: {
						io: Digital,
						mode: Digital.Output,
						pin: 1
					},
					interrupt: {
						io: Digital,
						mode: Digital.Input,
						pin: 0
					}
				});
				result.configure({});
				return result;
			}
		}
	}
};

export default device;
