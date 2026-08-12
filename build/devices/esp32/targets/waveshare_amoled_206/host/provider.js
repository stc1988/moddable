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

import Analog from 'embedded:io/analog'
import Digital from 'embedded:io/digital'
import DigitalBank from 'embedded:io/digitalbank'
import I2C from 'embedded:io/i2c'
import PulseCount from 'embedded:io/pulsecount'
import PWM from 'embedded:io/pwm'
import Serial from 'embedded:io/serial'
import SMBus from 'embedded:io/smbus'
import SPI from 'embedded:io/spi'
import Touch from 'embedded:sensor/Touch/FT3168'
import RTC from 'embedded:RTC/PCF85063'
import QMI8658 from 'embedded:sensor/Accelerometer-Gyroscope/QMI8658'

import Button from "button";
import AXP2101 from 'embedded:peripheral/Power/axp2101'

class Backlight {
	constructor(brightness = 100) {
		this.write(brightness);
	}
	write(value) {
		if (globalThis.screen)
			globalThis.screen.brightness = value;
	}
	close() {}
};

const device = {
  I2C: {
    default: {
      io: I2C,
      data: 15,
      clock: 14
    }
  },
  SPI: {
    default: {
      io: SPI,
      port: 2,
      clock: 11,
      out: 4
    }
  },
  io: { Analog, Digital, DigitalBank, I2C, PulseCount, PWM, Serial, SMBus, SPI },
  pin: {
    button: 0,
	buttonA: 0,
    buttonB: 10,
	amplifier: 46
  },
  peripheral: {
	Backlight,
    Button,
    Power: {
      PMIC: class {
        constructor() {
          return new AXP2101({
            sensor: { ...device.I2C.default, io: SMBus },
            address: 0x34
          });
        }
      },
      Amplifier: class {
        constructor() {
          return new Digital({
            io: Digital,
            pin: device.pin.amplifier,
            mode: Digital.Output,
            initialValue: 1
          });
		}
      }
    },
    RTC: class {
      constructor(options) {
        return new RTC({
          ...options,
          clock: {
            ...device.I2C.default,
            io: SMBus
          }
        })
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
            pin: 9
          },
          interrupt: {
            io: Digital,
            mode: Digital.Input,
            pin: 38
          }
        })
        result.configure({ length: 1 })
        return result
      }
    },
    IMU: class extends QMI8658 {
      constructor(options) {
        super({
          ...options,
          sensor: {
            ...device.I2C.default,
            io: device.io.SMBus
          }
        });
      }
	  sample() {
		const sample = super.sample();
		[sample.accelerometer.x, sample.accelerometer.y, sample.accelerometer.z] =
		  [sample.accelerometer.y, sample.accelerometer.x, sample.accelerometer.z * -1];
        [sample.gyroscope.x, sample.gyroscope.y, sample.gyroscope.z] =
		  [sample.gyroscope.y, sample.gyroscope.x, sample.gyroscope.z * -1];
		return sample;
	  }
    }
  },
  xperipheral: {
    button: {
      A: class {
        constructor(options) {
          return new Digital({
            ...options,
            pin: device.pin.button,
            mode: Digital.InputPullUp
          })
        }
      }
    }
  }
}

export default device
