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
import TextEncoder from "text/encoder";

const replacement = String.fromCharCode(0xFFFD);
class TextEncoderStream extends TransformStream {
	constructor() {
		const encoder = new TextEncoder();
		let pendingSurrogate = null;
		super({
			transform(string, controller) {
				string = String(string);
				let length = string.length;
				let result = "";
				for (let i = 0; i < length; i++) {
					const code = string.charCodeAt(i);
					if ((0xD800 <= code) && (code <= 0xDBFF)) {
						if (pendingSurrogate) {
							result += replacement;
							pendingSurrogate = null;
						}
						pendingSurrogate = string[i];
					}
					else if ((0xDC00 <= code) && (code <= 0xDFFF)) {
						if (pendingSurrogate) {
							result += pendingSurrogate + string[i];
							pendingSurrogate = null;
						}
						else {
							result += replacement;
						}
					}
					else {
						if (pendingSurrogate) {
							result += replacement;
							pendingSurrogate = null;
						}
						result += string[i];
					}
				}	
				if (result.length > 0) {
					const chunk = encoder.encode(result);
    				controller.enqueue(chunk);
				}	
			},
			flush(controller) {
				if (pendingSurrogate) {
					const chunk = encoder.encode(replacement);
    				controller.enqueue(chunk);
				}	
			}
		});
	}
};

export default TextEncoderStream;