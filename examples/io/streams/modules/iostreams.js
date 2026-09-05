import { ReadableStream, WritableStream } from "streams";

export function IOReadableStreamMixin(Base) {
	return class extends ReadableStream {  
		constructor(options) {
	 		super({
				start(controller) {
					trace(`start readable\n`);
					this.io = new Base({
						...options,
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
		constructor(options) {
	 		super({
				start(/* controller */) {
					trace(`start writable\n`);
					this.io = new Base({
						...options
					});
				},
				write(chunk) {
					this.io.write(chunk);
				}
			})
		}
	};
}