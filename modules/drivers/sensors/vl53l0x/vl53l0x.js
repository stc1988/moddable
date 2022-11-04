/*
 * Copyright (c) 2016-2022  Moddable Tech, Inc.
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
  Time-of-Flight ranging sensor
    Data sheet for controller: https://www.st.com/resource/en/datasheet/vl53l0x.pdf
    Reference implementation: https://github.com/m5stack/M5Stack/blob/master/examples/Unit/ToF_VL53L0X/ToF_VL53L0X.ino
*/

import Timer from "timer";

const REGISTERS = Object.freeze({
  SYSRANGE_START: 0x00,

  //   SYSTEM_THRESH_HIGH: 0x0c,
  //   SYSTEM_THRESH_LOW: 0x0e,

  //   SYSTEM_SEQUENCE_CONFIG: 0x01,
  //   SYSTEM_RANGE_CONFIG: 0x09,
  //   SYSTEM_INTERMEASUREMENT_PERIOD: 0x04,

  //   SYSTEM_INTERRUPT_CONFIG_GPIO: 0x0a,

  //   GPIO_HV_MUX_ACTIVE_HIGH: 0x84,

  //   SYSTEM_INTERRUPT_CLEAR: 0x0b,

  //   RESULT_INTERRUPT_STATUS: 0x13,
  RESULT_RANGE_STATUS: 0x14,

  //   RESULT_CORE_AMBIENT_WINDOW_EVENTS_RTN: 0xbc,
  //   RESULT_CORE_RANGING_TOTAL_EVENTS_RTN: 0xc0,
  //   RESULT_CORE_AMBIENT_WINDOW_EVENTS_REF: 0xd0,
  //   RESULT_CORE_RANGING_TOTAL_EVENTS_REF: 0xd4,
  //   RESULT_PEAK_SIGNAL_RATE_REF: 0xb6,

  //   ALGO_PART_TO_PART_RANGE_OFFSET_MM: 0x28,

  //   I2C_SLAVE_DEVICE_ADDRESS: 0x8a,

  //   MSRC_CONFIG_CONTROL: 0x60,

  //   PRE_RANGE_CONFIG_MIN_SNR: 0x27,
  //   PRE_RANGE_CONFIG_VALID_PHASE_LOW: 0x56,
  //   PRE_RANGE_CONFIG_VALID_PHASE_HIGH: 0x57,
  //   PRE_RANGE_MIN_COUNT_RATE_RTN_LIMIT: 0x64,

  //   FINAL_RANGE_CONFIG_MIN_SNR: 0x67,
  //   FINAL_RANGE_CONFIG_VALID_PHASE_LOW: 0x47,
  //   FINAL_RANGE_CONFIG_VALID_PHASE_HIGH: 0x48,
  //   FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT: 0x44,

  //   PRE_RANGE_CONFIG_SIGMA_THRESH_HI: 0x61,
  //   PRE_RANGE_CONFIG_SIGMA_THRESH_LO: 0x62,

  //   PRE_RANGE_CONFIG_VCSEL_PERIOD: 0x50,
  //   PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI: 0x51,
  //   PRE_RANGE_CONFIG_TIMEOUT_MACROP_LO: 0x52,

  //   SYSTEM_HISTOGRAM_BIN: 0x81,
  //   HISTOGRAM_CONFIG_INITIAL_PHASE_SELECT: 0x33,
  //   HISTOGRAM_CONFIG_READOUT_CTRL: 0x55,

  //   FINAL_RANGE_CONFIG_VCSEL_PERIOD: 0x70,
  //   FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI: 0x71,
  //   FINAL_RANGE_CONFIG_TIMEOUT_MACROP_LO: 0x72,
  //   CROSSTALK_COMPENSATION_PEAK_RATE_MCPS: 0x20,

  //   MSRC_CONFIG_TIMEOUT_MACROP: 0x46,

  //   SOFT_RESET_GO2_SOFT_RESET_N: 0xbf,
  IDENTIFICATION_MODEL_ID: 0xc0,
  //   IDENTIFICATION_REVISION_ID: 0xc2,

  //   OSC_CALIBRATE_VAL: 0xf8,

  //   GLOBAL_CONFIG_VCSEL_WIDTH: 0x32,
  //   GLOBAL_CONFIG_SPAD_ENABLES_REF_0: 0xb0,
  //   GLOBAL_CONFIG_SPAD_ENABLES_REF_1: 0xb1,
  //   GLOBAL_CONFIG_SPAD_ENABLES_REF_2: 0xb2,
  //   GLOBAL_CONFIG_SPAD_ENABLES_REF_3: 0xb3,
  //   GLOBAL_CONFIG_SPAD_ENABLES_REF_4: 0xb4,
  //   GLOBAL_CONFIG_SPAD_ENABLES_REF_5: 0xb5,

  //   GLOBAL_CONFIG_REF_EN_START_SELECT: 0xb6,
  //   DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD: 0x4e,
  //   DYNAMIC_SPAD_REF_EN_START_OFFSET: 0x4f,
  //   POWER_MANAGEMENT_GO1_POWER_FORCE: 0x80,

  //   VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV: 0x89,

  //   ALGO_PHASECAL_LIM: 0x30,
  //   ALGO_PHASECAL_CONFIG_TIMEOUT: 0x30,
});

class VL53L0X {
  #io;
  #onError;

  #buf = new ArrayBuffer(12);
  #view = new DataView(this.#buf);

  constructor(options) {
    const io = (this.#io = new options.sensor.io({
      address: 0x29,
      hz: 400_000,
      ...options.sensor,
    }));

    this.#onError = options.onError;

    if (
      io.readByte(0xc0) != 0xee ||
      io.readByte(0xc1) != 0xaa ||
      io.readByte(0xc2) != 0x10
    )
      throw new Error("ERR_REGISTER_ID");

    this.configure({});
  }

  configure(options) {}

  close() {
    this.#io?.close();
    this.#io = undefined;
  }

  sample() {
    const io = this.#io;

    io.writeByte(REGISTERS.SYSRANGE_START, 0x01);
    io.readBlock(REGISTERS.RESULT_RANGE_STATUS, this.#buf);

    return this.#view.getUint16(10, false);
  }
}

export default VL53L0X;
