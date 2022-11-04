/*
 * Copyright (c) 2022  Moddable Tech, Inc.
 *
 *   This file is part of the Moddable SDK.
 *
 *   This work is licensed under the
 *       Creative Commons Attribution 4.0 International License.
 *   To view a copy of this license, visit
 *       <http://creativecommons.org/licenses/by/4.0>.
 *   or send a letter to Creative Commons, PO Box 1866,
 *   Mountain View, CA 94042, USA.
 *
 */

import Distance from "embedded:sensor/Time-of-Flight-Ranging/VL53L0X";
import Timer from "timer";

const sensor = new Distance({
  sensor: {
    ...device.I2C.default,
    io: device.io.SMBus
  }
});

Timer.repeat(function () {
  const sample = sensor.sample();
  if (sample) {
    trace(`Distance:${sample}\n`);
  }
}, 100);
