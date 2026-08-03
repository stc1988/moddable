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

import { ReadableStream, WritableStream } from "web/streams";

export function IOReadableStreamMixin(Base) {
	return class extends ReadableStream {  
		constructor(dictionary) {
	 		super({
				start(controller) {
					trace(`start readable\n`);
					this.io = new Base({
						...dictionary,
						onReadable() {
							controller.enqueue(this.read());
						}
					});
				}
			})
		}
	};
}

export function IOWritableStreamMixin(Base) {
	return class extends WritableStream {  
		constructor(dictionary) {
	 		super({
				start(controller) {
					trace(`start readable\n`);
					this.io = new Base({
						...dictionary
					});
				},
				write(chunk) {
					this.io.write(chunk);
				}
			})
		}
	};
}