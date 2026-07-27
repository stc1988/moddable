import { ReadableStream, WritableStream } from "web/streams";

import Timer from "timer"
import URL from "url";

class WebSocketStream {
	#protocol = "";
	#extensions = "";
	#url = "";

	#client = null;
	#state = 0;
	#closed = null;
	#opened = null;
	
	#readable = null;
	#readableBuffer = null;
	#readableController = null;
	#readableLength = 0;
	#readableOptions = null;
	
	#writable = null;
	#writableController = null;
	#writableBuffers = [];
	#writableLength = 0;

	constructor(href, options) {
		let keepalive, protocol, signal;
		let url = new URL(href);
		let scheme = url.protocol;
		let port, config;
		if (scheme == "ws:") {
			port = url.port || 80;
			config = {...(options?.ws ?? device.network.ws)};
		}
		else if (scheme == "wss:") {
			port = url.port || 443;
			config = {...(options?.wss ?? device.network.wss)};
		}
		else
			throw new URIError("only ws or wss");
			
		let host = url.hostname;
		let path = url.pathname;
		let query = url.search;
		if (query)
			path += query;
		this.#url = href;
		
		protocol = options?.protocols;
		signal = options?.signal;
			
		this.#closed = Promise.withResolvers();
		this.#opened = Promise.withResolvers();
		
		options = { ...config, host, port, path, protocol }
		this.#client = new device.network.ws.io({
			...options,
			onControl: (opcode, data) => {
				switch (opcode) {
					case this.#client.constructor.close:
						if (this.#state < 3) {
							this.#state = 3;
							this.#readableController.close();
							data = new Uint8Array(data);
							this.#closed.resolve({
								closeCode: (data[0] << 8) | data[1],
								reason: String.fromArrayBuffer(data.buffer.slice(2)),
							});
						}
						break;
					case this.#client.constructor.ping:
						break;
					case this.#client.constructor.pong:
// 						if (this.#keepalive)
// 							this.#keepalive.pong = true;
						break;
				}
			},
			onReadable: (count, options) => {
// 				trace(`onReadable ${count} binary ${options.binary} more ${options.more}\n`);
				if (!count)
					return;
				if (this.#state == 0) {
					return;
				}
				if (this.#readableController.desiredSize > 0) {
					this.#read(count, options);
				}
				else {
					this.#readableLength = count;
					this.#readableOptions = options;
				}
			},
			onWritable: (count) => {
// 				trace(`onWritable ${count}\n`);
				this.#writableLength = count;
				if (this.#state == 0) {
					this.#state = 1;
    				this.#opened.resolve({
    					readable: this.#readable,
    					writable: this.#writable,
    					protocol: this.#protocol,
    					extensions: this.#extensions,
    				});
					return;
				}
				let buffers = this.#writableBuffers;
				while (buffers.length) {
					let buffer = buffers[0];
					let options = buffer.options;
					let data = buffer.data;
					const dataLength = data.byteLength;
					const writableLength = this.#writableLength;
					if (dataLength <= writableLength) {
						this.#writableLength = this.#clientWrite(data, options);
						buffer.result?.resolve();
						buffers.shift();
					}
					else if (0 < writableLength) {
						let moreOptions = { ...options, more:true };
						this.#writableLength = this.#clientWrite(data.slice(0, writableLength), moreOptions);
						buffer.data = data.slice(writableLength);
						break;
					}
					else
						break;
				}
			},
			onClose: () => {
// 				trace(`onClose\n`);
			},
			onError: () => {
// 				trace(`onError\n`);
				const error = new Error("WebSocket error");
				if (this.#state == 0)
    				this.#opened.reject(error);
				if (this.#state < 3) {
 					this.#state = 3;
 					this.#readableController.error(error);
					this.#writableController.error(error);
  					this.#closed.reject(error);
				}
			}
		});
		
		this.#readable = new ReadableStream({
			start: (controller) => {
				this.#readableController = controller;
			},
			pull: (controller) => {
// 				if (this.#state == 2)
// 					throw new Error("WebSocket closing");
				if (this.#state == 3)
					throw new Error("WebSocket closed");
				const count = this.#readableLength;
				const options = this.#readableOptions;
				if (count && options) {
					this.#readableLength = 0;
					this.#readableOptions = null;
					this.#read(count, options);
				}
			},
			cancel: (reason = "ReadableStream canceled") => {
				if (this.#state == 1)
					this.close({ closeCode:4000, reason });
			}
		});
		this.#writable = new WritableStream({
			start: (controller) => {
				this.#writableController = controller;
			},
			write: (data) => {
				if (this.#state == 2)
					throw new Error("WebSocket closing");
				if (this.#state == 3)
					throw new Error("WebSocket closed");
				let binary = false;
				if (data instanceof ArrayBuffer)
					binary = true;
				else if ((data instanceof DataView) || (data instanceof TypedArray)) {
					binary = true;
					data = data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength);
				}
				else
					data = ArrayBuffer.fromString(data);
				const buffer = this.#write(data, { binary });
				if (buffer) {
					buffer.result = Promise.withResolvers();
					return buffer.result.promise;
				}
			},
			close: () => {
				if (this.#state == 1)
					this.close({ closeCode:4001, reason: "WritableStream closed" });
			},
			abort: (reason = "WritableStream aborted") => {
				if (this.#state == 1)
					this.close({ closeCode:4002, reason });
			}
		});
	}
	get opened() {
		return this.#opened.promise;
	}
	get closed() {
		return this.#closed.promise;
	}
	get url() {
		return this.#url;
	}
	close(options) {
// 		Timer.clear(this.#keepalive);
// 		this.#keepalive = undefined;
		let code = options?.closeCode;
		let reason = options?.reason;
		if (code === undefined) {
			code = 1000;
			reason = "";
		}
		else {
			if ((code != 1000) && ((code < 3000) || (4999 < code)))
				throw new Error("invalid code: " + code);
			if (reason === undefined)
				throw new Error("code but no reason");
		}
		reason = ArrayBuffer.fromString(reason);
		if (reason.byteLength > 123)
			throw new Error("too long reason");
		if (this.#state == 1) {
			let data = new Uint8Array(2);
			data[0] = code >> 8;
			data[1] = code & 0xFF;
			data = data.buffer.concat(reason);
			this.#write(data, { opcode: this.#client.constructor.close });	
			this.#state = 2;
		}
	}
	#read(count, options) {
		let data = this.#clientRead(count);
		let buffer = this.#readableBuffer;
		if (buffer)
			buffer = buffer.concat(data);
		else
			buffer = data;
		if (!options.more) {
			if (options.binary)
				data = new Uint8Array(buffer);
			else
				data = String.fromArrayBuffer(buffer);
			this.#readableBuffer = null;
			this.#readableController.enqueue(data);
		}
		else {
			this.#readableBuffer = buffer;
		}
	}
	#write(data, options) {
		let buffers = this.#writableBuffers;
		let buffer = null;
		if (buffers.length) 
			buffer = { data, options };
		else {
			const dataLength = data.byteLength;
			const writableLength = this.#writableLength;
			if (dataLength <= writableLength) {
				this.#writableLength = this.#clientWrite(data, options);
			}
			else if (0 < writableLength) {
				let moreOptions = { ...options, more:true };
				this.#writableLength = this.#clientWrite(data.slice(0, writableLength), moreOptions);
				buffer = { data: data.slice(writableLength), options };
			}
			else {
				buffer = { data, options };
			}
		}
		if (buffer)
			buffers.push(buffer);
		return buffer;
	}
	#clientRead(count) {
// 		trace(`#client.read ${ count }\n`);
		return this.#client.read(count);
	}
	#clientWrite(buffer, options) {
// 		trace(`#client.write ${ buffer.byteLength }\n`);
		return this.#client.write(buffer, options);
	}
}

export default WebSocketStream;
