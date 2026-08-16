/*
 * Copyright (c) 2026  Moddable Tech, Inc.
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

import { ReadableStream } from "web/streams";
import Timer from "timer";

// TBD: interrupt

export function SensorStreamMixin(Base) {
	return class extends ReadableStream {  
		constructor(dictionary) {
	 		super({
				start(controller) {
					const sensor = new Base({
						...dictionary
					});
					Timer.repeat(() => {
						const samples = sensor.sample();
						if (samples) {
							if (Array.isArray(samples))
								samples.forEach(sample => controller.enqueue(sample));
							else
								controller.enqueue(samples);
						}
					}, dictionary.interval);
				}
			})
		}
	};
}
