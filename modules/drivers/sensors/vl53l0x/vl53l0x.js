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
    Reference implementation: https://github.com/pololu/vl53l0x-arduino/
*/

import Timer from "timer";

const REGISTERS = Object.freeze(
  {
    SYSRANGE_START: 0x00,
    //   SYSTEM_THRESH_HIGH: 0x0c,
    //   SYSTEM_THRESH_LOW: 0x0e,
    SYSTEM_SEQUENCE_CONFIG: 0x01,
    //   SYSTEM_RANGE_CONFIG: 0x09,
    //   SYSTEM_INTERMEASUREMENT_PERIOD: 0x04,
    SYSTEM_INTERRUPT_CONFIG_GPIO: 0x0a,
    GPIO_HV_MUX_ACTIVE_HIGH: 0x84,
    SYSTEM_INTERRUPT_CLEAR: 0x0b,
    //   RESULT_INTERRUPT_STATUS: 0x13,
    RESULT_RANGE_STATUS: 0x14,
    //   RESULT_CORE_AMBIENT_WINDOW_EVENTS_RTN: 0xbc,
    //   RESULT_CORE_RANGING_TOTAL_EVENTS_RTN: 0xc0,
    //   RESULT_CORE_AMBIENT_WINDOW_EVENTS_REF: 0xd0,
    //   RESULT_CORE_RANGING_TOTAL_EVENTS_REF: 0xd4,
    //   RESULT_PEAK_SIGNAL_RATE_REF: 0xb6,
    //   ALGO_PART_TO_PART_RANGE_OFFSET_MM: 0x28,
    //   I2C_SLAVE_DEVICE_ADDRESS: 0x8a,
    MSRC_CONFIG_CONTROL: 0x60,
    //   PRE_RANGE_CONFIG_MIN_SNR: 0x27,
    //   PRE_RANGE_CONFIG_VALID_PHASE_LOW: 0x56,
    //   PRE_RANGE_CONFIG_VALID_PHASE_HIGH: 0x57,
    //   PRE_RANGE_MIN_COUNT_RATE_RTN_LIMIT: 0x64,
    //   FINAL_RANGE_CONFIG_MIN_SNR: 0x67,
    //   FINAL_RANGE_CONFIG_VALID_PHASE_LOW: 0x47,
    //   FINAL_RANGE_CONFIG_VALID_PHASE_HIGH: 0x48,
    FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT: 0x44,
    //   PRE_RANGE_CONFIG_SIGMA_THRESH_HI: 0x61,
    //   PRE_RANGE_CONFIG_SIGMA_THRESH_LO: 0x62,
    //   PRE_RANGE_CONFIG_VCSEL_PERIOD: 0x50,
    //   PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI: 0x51,
    //   PRE_RANGE_CONFIG_TIMEOUT_MACROP_LO: 0x52,
    //   SYSTEM_HISTOGRAM_BIN: 0x81,
    //   HISTOGRAM_CONFIG_INITIAL_PHASE_SELECT: 0x33,
    //   HISTOGRAM_CONFIG_READOUT_CTRL: 0x55,
    //   FINAL_RANGE_CONFIG_VCSEL_PERIOD: 0x70,
    FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI: 0x71,
    //   FINAL_RANGE_CONFIG_TIMEOUT_MACROP_LO: 0x72,
    //   CROSSTALK_COMPENSATION_PEAK_RATE_MCPS: 0x20,
    MSRC_CONFIG_TIMEOUT_MACROP: 0x46,
    //   SOFT_RESET_GO2_SOFT_RESET_N: 0xbf,
    //   IDENTIFICATION_MODEL_ID: 0xc0,
    //   IDENTIFICATION_REVISION_ID: 0xc2,
    //   OSC_CALIBRATE_VAL: 0xf8,
    //   GLOBAL_CONFIG_VCSEL_WIDTH: 0x32,
    GLOBAL_CONFIG_SPAD_ENABLES_REF_0: 0xb0,
    //   GLOBAL_CONFIG_SPAD_ENABLES_REF_1: 0xb1,
    //   GLOBAL_CONFIG_SPAD_ENABLES_REF_2: 0xb2,
    //   GLOBAL_CONFIG_SPAD_ENABLES_REF_3: 0xb3,
    //   GLOBAL_CONFIG_SPAD_ENABLES_REF_4: 0xb4,
    //   GLOBAL_CONFIG_SPAD_ENABLES_REF_5: 0xb5,
    GLOBAL_CONFIG_REF_EN_START_SELECT: 0xb6,
    DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD: 0x4e,
    DYNAMIC_SPAD_REF_EN_START_OFFSET: 0x4f,
    //   POWER_MANAGEMENT_GO1_POWER_FORCE: 0x80,
    //   VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV: 0x89,
    //   ALGO_PHASECAL_LIM: 0x30,
    //   ALGO_PHASECAL_CONFIG_TIMEOUT: 0x30,
  },
  true
);

const OVERHEAD = Object.freeze(
  {
    SATRT: 1910,
    END: 960,
    MSRC: 660,
    TCC: 590,
    DSS: 690,
    PRE_RANGE: 660,
    FINAL_RANGE: 550,
  },
  true
);

class VL53L0X {
  #io;
  #onError;

  #stop_variable;
  #measurement_timing_budget_us;
  #timeout;

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

    // Set I2C standard mode.
    this.#writeRegisters([
      [0x88, 0x00],
      [0x80, 0x01],
      [0xff, 0x01],
      [0x00, 0x00],
    ]);
    this.#stop_variable = io.readByte(0x91);

    this.#writeRegisters([
      [0x00, 0x01],
      [0xff, 0x00],
      [0x80, 0x00],
    ]);

    var config_control = io.readByte(REGISTERS._MSRC_CONFIG_CONTROL);
    io.writeByte(REGISTERS._MSRC_CONFIG_CONTROL, config_control | 0x12);

    this.#setSignalRateLimit(0.25);

    io.writeByte(REGISTERS.SYSTEM_SEQUENCE_CONFIG, 0xff);

    var spadInfo = this.#getSpadInfo();
    var spadMap = io.readBlock(REGISTERS.GLOBAL_CONFIG_SPAD_ENABLES_REF_0, 6);

    io.writeByte(0xff, 0x01);
    io.writeByte(REGISTERS.DYNAMIC_SPAD_REF_EN_START_OFFSET, 0x00);
    io.writeByte(REGISTERS.DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD, 0x2c);
    io.writeByte(0xff, 0x00);
    io.writeByte(REGISTERS.GLOBAL_CONFIG_REF_EN_START_SELECT, 0xb4);

    var first_spad_to_enable = spadInfo.type_is_aperture ? 12 : 0; // 12 is the first aperture spad
    var spads_enabled = 0;

    for (var i = 0; i < 48; i++) {
      if (i < first_spad_to_enable || spads_enabled == spadInfo.count) {
        spadMap[i / 8] &= ~(1 << i % 8);
      } else if ((spadMap[i / 8] >> i % 8) & 0x1) {
        spads_enabled++;
      }
    }
    io.writeBlock(GLOBAL_CONFIG_SPAD_ENABLES_REF_0, spadMap, 6);

    this.#writeRegisters([
      [0xff, 0x01],
      [0x00, 0x00],
      [0xff, 0x00],
      [0x09, 0x00],
      [0x10, 0x00],
      [0x11, 0x00],
      [0x24, 0x01],
      [0x25, 0xff],
      [0x75, 0x00],
      [0xff, 0x01],
      [0x4e, 0x2c],
      [0x48, 0x00],
      [0x30, 0x20],
      [0xff, 0x00],
      [0x30, 0x09],
      [0x54, 0x00],
      [0x31, 0x04],
      [0x32, 0x03],
      [0x40, 0x83],
      [0x46, 0x25],
      [0x60, 0x00],
      [0x27, 0x00],
      [0x50, 0x06],
      [0x51, 0x00],
      [0x52, 0x96],
      [0x56, 0x08],
      [0x57, 0x30],
      [0x61, 0x00],
      [0x62, 0x00],
      [0x64, 0x00],
      [0x65, 0x00],
      [0x66, 0xa0],
      [0xff, 0x01],
      [0x22, 0x32],
      [0x47, 0x14],
      [0x49, 0xff],
      [0x4a, 0x00],
      [0xff, 0x00],
      [0x7a, 0x0a],
      [0x7b, 0x00],
      [0x78, 0x21],
      [0xff, 0x01],
      [0x23, 0x34],
      [0x42, 0x00],
      [0x44, 0xff],
      [0x45, 0x26],
      [0x46, 0x05],
      [0x40, 0x40],
      [0x0e, 0x06],
      [0x20, 0x1a],
      [0x43, 0x40],
      [0xff, 0x00],
      [0x34, 0x03],
      [0x35, 0x44],
      [0xff, 0x01],
      [0x31, 0x04],
      [0x4b, 0x09],
      [0x4c, 0x05],
      [0x4d, 0x04],
      [0xff, 0x00],
      [0x44, 0x00],
      [0x45, 0x20],
      [0x47, 0x08],
      [0x48, 0x28],
      [0x67, 0x00],
      [0x70, 0x04],
      [0x71, 0x01],
      [0x72, 0xfe],
      [0x76, 0x00],
      [0x77, 0x00],
      [0xff, 0x01],
      [0x0d, 0x01],
      [0xff, 0x00],
      [0x80, 0x01],
      [0x01, 0xf8],
      [0xff, 0x01],
      [0x8e, 0x01],
      [0x00, 0x01],
      [0xff, 0x00],
      [0x80, 0x00],
    ]);

    io.writeByte(REGISTERS.SYSTEM_INTERRUPT_CONFIG_GPIO, 0x04);
    io.writeByte(
      REGISTERS.GPIO_HV_MUX_ACTIVE_HIGH,
      io.readByte(REGISTERS.GPIO_HV_MUX_ACTIVE_HIGH) & ~0x10
    );
    io.writeByte(REGISTERS.SYSTEM_INTERRUPT_CLEAR, 0x01);

    var measurement_timing_budget_us = this.#getMeasurementTimingBudget();
    io.writeByte(REGISTERS.SYSTEM_SEQUENCE_CONFIG, 0xe8);
    this.#setMeasurementTimingBudget(measurement_timing_budget_us);

    io.writeByte(REGISTERS.SYSTEM_SEQUENCE_CONFIG, 0x01);
    this.#performSingleRefCalibration(0x40);

    io.writeByte(REGISTERS.SYSTEM_SEQUENCE_CONFIG, 0x02);
    this.#performSingleRefCalibration(0x00);

    io.writeByte(REGISTERS.SYSTEM_SEQUENCE_CONFIG, 0xe8);

    this.configure({});
  }

  configure(options) {
    const io = this.#io;

    if ("timeout" in options) {
      this.#timeout = options.timeout;
    }

    if ("rateLimit" in options) {
      this.#setSignalRateLimit(options.rateLimit);
    }

    // setVcselPulsePeriod

    if ("timingBudget" in options) {
      this.#setMeasurementTimingBudget(options.timingBudget);
    }

    // if ("invert" in options) {
    //   delete io.invert;
    //   if (options.invert)
    //     io.invert = true;
    // }
  }

  close() {
    this.#io?.close();
    this.#io = undefined;
  }

  sample() {
    const io = this.#io;

    this.#writeRegisters([
      [0x80, 0x01],
      [0xff, 0x01],
      [0x00, 0x00],
      [0x91, this.#stop_variable],
      [0x00, 0x01],
      [0xff, 0x00],
      [0x80, 0x00],
      [REGISTERS.SYSRANGE_START, 0x01],
    ]);

    io.readBlock(REGISTERS.RESULT_RANGE_STATUS, this.#buf);

    return this.#view.getUint16(10, false);
  }

  #setSignalRateLimit(limit_Mcps) {
    if (limit_Mcps < 0 || limit_Mcps > 511.99) {
      return false;
    }

    // Q9.7 fixed point format (9 integer bits, 7 fractional bits)
    this.#io.writeByte(
      REGISTERS.FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT,
      limit_Mcps * (1 << 7)
    );
    return true;
  }

  #getSpadInfo() {
    const io = this.#io;
    this.#writeRegisters([
      [0x80, 0x01],
      [0xff, 0x01],
      [0x00, 0x00],
      [0xff, 0x06],
    ]);

    io.writeByte(0x83, io.readByte(0x83) | 0x04);

    this.#writeRegisters([
      [0xff, 0x07],
      [0x81, 0x01],
      [0x80, 0x01],
      [0x94, 0x6b],
      [0x83, 0x00],
    ]);

    io.writeByte(0x83, 0x01);

    this.#writeRegisters([
      [0x81, 0x00],
      [0xff, 0x06],
    ]);

    io.writeByte(0x83, io.readByte(0x83) & ~0x04);

    this.#writeRegisters([
      [0xff, 0x01],
      [0x00, 0x01],
      [0xff, 0x00],
      [0x80, 0x00],
    ]);

    var tmp = io.readByte(0x92);
    return { count: tmp & 0x7f, type_is_aperture: (tmp >> 7) & 0x01 };
  }

  #writeRegisters(pairs) {
    for (var pair of pair) {
      this.#io.writeByte(pair[0], pair[1]);
    }
  }

  #performSingleRefCalibration(vhv_init_byte) {
    const io = this.#io;

    io.writeByte(REGISTERS.SYSRANGE_START, 0x01 | vhv_init_byte);
    io.writeByte(REGISTERS.SYSTEM_INTERRUPT_CLEAR, 0x01);
    io.writeByte(REGISTERS.SYSRANGE_START, 0x00);
  }

  #getSequenceStepEnables() {
    var enables = {};
    var sequence_config = this.#io.readReg(REGISTERS.SYSTEM_SEQUENCE_CONFIG);

    enables.tcc = (sequence_config >> 4) & 0x1;
    enables.dss = (sequence_config >> 3) & 0x1;
    enables.msrc = (sequence_config >> 2) & 0x1;
    enables.pre_range = (sequence_config >> 6) & 0x1;
    enables.final_range = (sequence_config >> 7) & 0x1;

    return enables;
  }

  #getSequenceStepTimeouts(enables) {
    const io = this.#io;
    var timeouts = {};

    timeouts.pre_range_vcsel_period_pclks =
      getVcselPulsePeriod(VcselPeriodPreRange);
    timeouts.msrc_dss_tcc_mclks =
      io.readByte(REGISTERS.MSRC_CONFIG_TIMEOUT_MACROP) + 1;
    // timeouts.msrc_dss_tcc_us =
    //   timeoutMclksToMicroseconds(timeouts->msrc_dss_tcc_mclks,
    //   timeouts->pre_range_vcsel_period_pclks);

    // timeouts->pre_range_mclks =
    //   decodeTimeout(readReg16Bit(PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI));
    // timeouts->pre_range_us =
    //   timeoutMclksToMicroseconds(timeouts->pre_range_mclks,
    //                              timeouts->pre_range_vcsel_period_pclks);

    // timeouts.final_range_vcsel_period_pclks = getVcselPulsePeriod(VcselPeriodFinalRange);

    // timeouts.final_range_mclks =
    //   decodeTimeout(readReg16Bit(FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI));

    if (enables.pre_range) {
      timeouts.final_range_mclks -= timeouts.pre_range_mclks;
    }

    // timeouts.final_range_us =
    //   timeoutMclksToMicroseconds(timeouts.final_range_mclks,
    //                              timeouts.final_range_vcsel_period_pclks);

    return timeouts;
  }

  // Get the measurement timing budget in microseconds
  #getMeasurementTimingBudget() {
    var enables;
    var timeouts;

    // Start and end overhead times always present"
    var budget_us = StartOverhead + EndOverhead;

    var enables = this.#getSequenceStepEnables();
    var timeouts = his.#getSequenceStepTimeouts(enables);

    if (enables.tcc) {
      budget_us += timeouts.msrc_dss_tcc_us + OVERHEAD.TCC;
    }

    if (enables.dss) {
      budget_us += 2 * (timeouts.msrc_dss_tcc_us + OVERHEAD.DSS);
    } else if (enables.msrc) {
      budget_us += timeouts.msrc_dss_tcc_us + OVERHEAD.MSRC;
    }

    if (enables.pre_range) {
      budget_us += timeouts.pre_range_us + OVERHEAD.PRE_RANGE;
    }

    if (enables.final_range) {
      budget_us += timeouts.final_range_us + OVERHEAD.FINAL_RANGE;
    }

    this.#measurement_timing_budget_us = budget_us;
    return budget_us;
  }

  #setMeasurementTimingBudget(budget_us) {
    var used_budget_us = OVERHEAD.StartOverhead + OVERHEAD.EndOverhead;

    var enables = this.#getSequenceStepEnables();
    var timeouts = this.#getSequenceStepTimeouts(enables);

    if (enables.tcc) {
      used_budget_us += timeouts.msrc_dss_tcc_us + OVERHEAD.TccOverhead;
    }

    if (enables.dss) {
      used_budget_us += 2 * (timeouts.msrc_dss_tcc_us + OVERHEAD.DssOverhead);
    } else if (enables.msrc) {
      used_budget_us += timeouts.msrc_dss_tcc_us + OVERHEAD.MsrcOverhead;
    }

    if (enables.pre_range) {
      used_budget_us += timeouts.pre_range_us + OVERHEAD.PreRangeOverhead;
    }

    if (enables.final_range) {
      used_budget_us += OVERHEAD.FinalRangeOverhead;

      if (used_budget_us > budget_us) {
        return false;
      }

      var final_range_timeout_us = budget_us - used_budget_us;

      // var final_range_timeout_mclks =
      // timeoutMicrosecondsToMclks(final_range_timeout_us,
      //                            timeouts.final_range_vcsel_period_pclks);

      if (enables.pre_range) {
        final_range_timeout_mclks += timeouts.pre_range_mclks;
      }

      // this.#io(
      //   REGISTERS.FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI,
      //   encodeTimeout(final_range_timeout_mclks)
      // );
    }
    return true;
  }
}

export default VL53L0X;
