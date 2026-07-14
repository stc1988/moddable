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