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
  RCWL-9620 ultrasonic distance sensor
  https://github.com/m5stack/M5Unit-Sonic
*/

import Timer from "timer";

const MAX_RANGE = 4500.0;

class RCWL9620 {
  #io;
  #valueBuffer = new Uint8Array(3);
  #byteBuffer = new Uint8Array(1);
  #onError;

  constructor(options) {
    const io = (this.#io = new options.sensor.io({
      address: 0x57,
      hz: 200_000,
      ...options.sensor,
    }));

    this.#onError = options.onError;

    this.configure({});
  }

  configure(options) {}

  #getDistance() {
    const io = this.#io;
    const vBuf = this.#valueBuffer;
    const bBuf = this.#byteBuffer;

    bBuf[0] = 0x01;
    io.write(bBuf, false);
    Timer.delay(150);

    io.read(vBuf);
    let distance = ((vBuf[0] << 16) | (vBuf[1] << 8) | vBuf[2]) / 1000;

    if (distance > MAX_RANGE) {
      return MAX_RANGE;
    } else {
      return distance;
    }
  }

  close() {
    this.#io?.close();
    this.#io = undefined;
  }

  sample() {
    return this.#getDistance();
  }
}

export default RCWL9620;
