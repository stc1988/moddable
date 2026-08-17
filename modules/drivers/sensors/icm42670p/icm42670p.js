/*
 * Copyright (c) 2026  Moddable Tech, Inc.
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
/*
	ICM-42670-P -- TDK InvenSense 6-axis IMU (3-axis accelerometer + 3-axis gyroscope + temperature)

	Datasheet: DS-000451, Rev 1.1 (05/23/2022)
	I2C address: 0x68 (AP_AD0 = GND) or 0x69 (AP_AD0 = VDDIO)

	Implements the ECMA-419 Sensor Class Pattern:
		constructor(options)
			options.sensor        - dictionary passed to the I2C/SMBus peripheral constructor
			options.target        - optional caller context
			options.accelerometer - optional {scale, sampleRate, mode} to enable/configure the accelerometer
			options.gyroscope     - optional {scale, sampleRate, mode} to enable/configure the gyroscope
		configure(options)   - same shape as constructor options (minus options.sensor)
		sample()             - returns {accelerometer: {x,y,z}, gyroscope: {x,y,z}, thermometer: {temperature}}
		close()
*/

import Time from "time";
import Timer from "timer";

const Register = Object.freeze({
	SIGNAL_PATH_RESET: 0x02,
	TEMP_DATA1: 0x09,			// TEMP_DATA1:TEMP_DATA0, ACCEL_DATA_*, GYRO_DATA_* are contiguous
	PWR_MGMT0: 0x1F,
	GYRO_CONFIG0: 0x20,
	ACCEL_CONFIG0: 0x21,
	WHO_AM_I: 0x75
});

const WHO_AM_I_VALUE = 0x67;

const RESET_MS = 2;				// soft-reset complete
const WRITE_QUIET_MS = 1;		// 200 µs after OFF → other; Timer.delay is 1 ms resolution
const GYRO_VALID_MS = 40;		// start-up time to valid gyro data (30 typ / 40 max)
const GYRO_KEEP_ON_MS = 45;		// must remain ON this long after leaving OFF before turning OFF

// ACCEL_UI_FS_SEL / GYRO_UI_FS_SEL field encodings (bits 6:5), indexed 0..3
const AccelFullScale = Object.freeze([16, 8, 4, 2]);			// g
const GyroFullScale  = Object.freeze([2000, 1000, 500, 250]);	// degrees/second

// ACCEL_ODR / GYRO_ODR field encodings (bits 3:0), shared table.
// Rates below 12.5 Hz (6.25, 3.125, 1.5625) are valid for the accelerometer only.
const ODR = Object.freeze({
	1600: 0b0101,
	800:  0b0110,
	400:  0b0111,
	200:  0b1000,
	100:  0b1001,
	50:   0b1010,
	25:   0b1011,
	12.5: 0b1100,
	6.25:   0b1101,
	3.125:  0b1110,
	1.5625: 0b1111
});

const GyroMode = Object.freeze({
	"off": 0b00,
	"standby": 0b01,
	"low noise": 0b11
});
const AccelMode = Object.freeze({
	"off": 0b00,
	"low power": 0b10,
	"low noise": 0b11
});

const GRAVITY = 9.80665;		// m/s^2 per g
const DEG2RAD = Math.PI / 180;	// radians per degree

function awaitSince(startedAt, ms) {
	if (undefined === startedAt)
		return;
	const elapsed = Time.delta(startedAt);
	if (elapsed < ms)
		Timer.delay(ms - elapsed);
}

class ICM42670P {
	#io;
	#accelScale;		// LSB per m/s^2
	#gyroScale;			// LSB per radian/second
	#accelEnabled = false;
	#gyroEnabled = false;
	#gyroOn = false;	// drive on (low noise or standby)
	#powerMgmt0 = 0;
	#buffer = new Uint8Array(14);
	#view;
	#resetAt;
	#writeQuietAt;
	#gyroOnAt;
	#accelFSSel;
	#accelODR;
	#accelMode;
	#gyroFSSel;
	#gyroODR;
	#gyroMode;

	constructor(options) {
		if ("target" in options)
			this.target = options.target;

		try {
			const io = this.#io = new options.sensor.io({
				hz: 400_000,
				address: 0x68,
				...options.sensor
			});

			this.#view = new DataView(this.#buffer.buffer);

			const id = io.readUint8(Register.WHO_AM_I);
			if (WHO_AM_I_VALUE !== id)
				throw new Error("unexpected sensor id");

			// software reset -- device always powers up in sleep mode (PWR_MGMT0 == 0)
			io.writeUint8(Register.SIGNAL_PATH_RESET, 0b0001_0000);		// SOFT_RESET_DEVICE_CONFIG
			this.#resetAt = Time.ticks;

			this.configure(options);
		}
		catch (e) {
			this.close();
			throw e;
		}
	}
	configure(options) {
		if ("accelerometer" in options)
			this.#configureAccelerometer(options.accelerometer);

		if ("gyroscope" in options)
			this.#configureGyroscope(options.gyroscope);
	}
	#configureAccelerometer(accelerometer) {
		const first = undefined === this.#accelMode;

		if ("scale" in accelerometer) {
			const fsSel = AccelFullScale.indexOf(accelerometer.scale);
			if (fsSel < 0)
				throw new RangeError("invalid accelerometer scale -- use 2, 4, 8, or 16 (g)");
			this.#accelFSSel = fsSel;
			this.#accelScale = (32768 / accelerometer.scale) / GRAVITY;
		}
		else if (first) {
			this.#accelFSSel = AccelFullScale.indexOf(2);
			this.#accelScale = (32768 / 2) / GRAVITY;
		}

		if ("sampleRate" in accelerometer) {
			const odr = ODR[accelerometer.sampleRate];
			if (undefined === odr)
				throw new RangeError("invalid accelerometer sampleRate");
			this.#accelODR = odr;
		}
		else if (first) {
			this.#accelODR = ODR[100];
		}

		if ("mode" in accelerometer) {
			if (!(accelerometer.mode in AccelMode))
				throw new RangeError("invalid accelerometer mode");
			this.#accelMode = accelerometer.mode;
		}
		else if (first) {
			this.#accelMode = "low noise";
		}

		if (first || ("scale" in accelerometer) || ("sampleRate" in accelerometer))
			this.#writeUint8(Register.ACCEL_CONFIG0, (this.#accelFSSel << 5) | this.#accelODR);

		if (first || ("mode" in accelerometer)) {
			this.#setPowerMode({accel: AccelMode[this.#accelMode]});
			this.#accelEnabled = "off" !== this.#accelMode;
		}
	}
	#configureGyroscope(gyroscope) {
		const first = undefined === this.#gyroMode;

		if ("scale" in gyroscope) {
			const fsSel = GyroFullScale.indexOf(gyroscope.scale);
			if (fsSel < 0)
				throw new RangeError("invalid gyroscope scale -- use 250, 500, 1000, or 2000 (degrees/second)");
			this.#gyroFSSel = fsSel;
			this.#gyroScale = (32768 / gyroscope.scale) / DEG2RAD;
		}
		else if (first) {
			this.#gyroFSSel = GyroFullScale.indexOf(250);
			this.#gyroScale = (32768 / 250) / DEG2RAD;
		}

		if ("sampleRate" in gyroscope) {
			const odr = ODR[gyroscope.sampleRate];
			if (undefined === odr || gyroscope.sampleRate < 12.5)
				throw new RangeError("invalid gyroscope sampleRate");
			this.#gyroODR = odr;
		}
		else if (first) {
			this.#gyroODR = ODR[100];
		}

		if ("mode" in gyroscope) {
			if (!(gyroscope.mode in GyroMode))
				throw new RangeError("invalid gyroscope mode");
			this.#gyroMode = gyroscope.mode;
		}
		else if (first) {
			this.#gyroMode = "low noise";
		}

		const nextOn = "off" !== this.#gyroMode;
		if (this.#gyroOn && !nextOn)
			awaitSince(this.#gyroOnAt, GYRO_KEEP_ON_MS);

		if (first || ("scale" in gyroscope) || ("sampleRate" in gyroscope))
			this.#writeUint8(Register.GYRO_CONFIG0, (this.#gyroFSSel << 5) | this.#gyroODR);

		if (first || ("mode" in gyroscope)) {
			this.#setPowerMode({gyro: GyroMode[this.#gyroMode]});
			if (nextOn && !this.#gyroOn)
				this.#gyroOnAt = Time.ticks;
			else if (!nextOn)
				this.#gyroOnAt = undefined;
			this.#gyroOn = nextOn;
			this.#gyroEnabled = "low noise" === this.#gyroMode;
		}
	}
	#setPowerMode({accel, gyro} = {}) {
		let value = this.#powerMgmt0;
		if (undefined !== gyro)
			value = (value & ~0b1100) | (gyro << 2);
		if (undefined !== accel)
			value = (value & ~0b0011) | accel;
		if (value === this.#powerMgmt0)
			return;

		const prev = this.#powerMgmt0;
		this.#writeUint8(Register.PWR_MGMT0, value);
		this.#powerMgmt0 = value;

		const leftOff = ((0 === (prev & 0b1100)) && (0 !== (value & 0b1100)))
			|| ((0 === (prev & 0b0011)) && (0 !== (value & 0b0011)));
		if (leftOff)
			this.#writeQuietAt = Time.ticks;
	}
	#writeUint8(register, value) {
		awaitSince(this.#resetAt, RESET_MS);
		this.#resetAt = undefined;
		awaitSince(this.#writeQuietAt, WRITE_QUIET_MS);
		this.#writeQuietAt = undefined;
		this.#io.writeUint8(register, value);
	}
	sample() {
		awaitSince(this.#resetAt, RESET_MS);
		this.#resetAt = undefined;

		if (this.#gyroEnabled)
			awaitSince(this.#gyroOnAt, GYRO_VALID_MS);

		const result = {};
		if (!this.#accelEnabled && !this.#gyroEnabled)
			return result;

		const view = this.#view;
		this.#io.readBuffer(Register.TEMP_DATA1, this.#buffer.buffer);

		result.thermometer = {
			temperature: (view.getInt16(0, false) / 128) + 25
		};

		if (this.#accelEnabled) {
			result.accelerometer = {
				x: view.getInt16(2, false) / this.#accelScale,
				y: view.getInt16(4, false) / this.#accelScale,
				z: view.getInt16(6, false) / this.#accelScale
			};
		}

		if (this.#gyroEnabled) {
			result.gyroscope = {
				x: view.getInt16(8, false) / this.#gyroScale,
				y: view.getInt16(10, false) / this.#gyroScale,
				z: view.getInt16(12, false) / this.#gyroScale
			};
		}

		return result;
	}
	close() {
		const io = this.#io;
		if (!io)
			return;
		this.#io = undefined;
		try {
			if (this.#powerMgmt0 && (undefined === this.#gyroOnAt || Time.delta(this.#gyroOnAt) >= GYRO_KEEP_ON_MS)) {
				awaitSince(this.#resetAt, RESET_MS);
				awaitSince(this.#writeQuietAt, WRITE_QUIET_MS);
				io.writeUint8(Register.PWR_MGMT0, 0);
			}
		}
		catch {
		}
		io.close();
	}

	static {
		this.prototype[Symbol.dispose] = this.prototype.close;
	}
}

export default ICM42670P;
