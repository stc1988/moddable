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

import { TransformStream }  from "web/streams";
import TextDecoder from "text/decoder";

class TextDecoderStream extends TransformStream {
	#decoder;
	constructor(label="utf-8", options={}) {
		const decoder = new TextDecoder(label, options);
		const decodeOptions = { stream:true };
		super({
			transform(chunk, controller) {
				if (!(ArrayBuffer.isView(chunk) || (chunk instanceof ArrayBuffer) || (chunk instanceof SharedArrayBuffer)))
					throw new TypeError("invalid chunk");
				if (chunk.byteLength > 0) {
					const string = decoder.decode(chunk, decodeOptions);
					if (string.length)
    					controller.enqueue(string);
    			}
			},
			flush(controller) {
				const string = decoder.decode();
				if (string.length)
					controller.enqueue(string);
			}
		});
		this.#decoder = decoder;
	}
	get encoding() {
		return this.#decoder.encoding;
	}
	get fatal() {
		return this.#decoder.fatal;
	}
	get ignoreBOM() {
		return this.#decoder.ignoreBOM;
	}
};

export default TextDecoderStream;