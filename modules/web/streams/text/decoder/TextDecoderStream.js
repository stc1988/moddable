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