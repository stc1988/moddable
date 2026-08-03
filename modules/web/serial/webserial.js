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

import {ReadableStream, WritableStream, ByteLengthQueuingStrategy} from "web/streams"
import Serial from "embedded:io/serial";

class WebSerial {
	static onconnect() {
		debugger;
	}
	static ondisconnect() {
		debugger;
	}
	static async getPorts() {
		debugger;
	}
	static async requestPort() {
		return new SerialPort("/dev/cu.usbserial-0001");
	}
}

class SerialPort {
	#state = "closed";
	#bufferSize;
	#readable;
	#writable;
	#fatal = false;		// combines readFatal and writeFatal
	#pendingClosePromise;

	#serial;
	#device;
	#readableBytes = 0;
	#writableBytes = 0;
	#readController;
	#writeController;
	#pull;
	#pendingWrite;			//@@ should be an array / list?

	constructor(device) {
		this.#device = device; 
	}
	static onconnect() {
		debugger;
	}
	static ondisconnect() {
		debugger;
	}
	get connected() {
		return (this.#serial && !this.#serial.first) ? true : false;
	}
	get readable() {
		if (this.#readable)
			return this.#readable;

		if (("opened" !== this.#state) || this.#fatal)
			return null;

		this.#readable = new ReadableStream({
			type: "bytes",
			start: controller => {
				this.#readController = controller;
			},
			pull: ( /* controller */ ) => {
				const use = this.#read();
				if (use <= 0) {
					trace(`pull -> promise\n`);
					this.#pull = Promise.withResolvers();
					return this.#pull.promise;
				}
				trace(`pull read ${use}\n`);
			},
			cancel: () => {
				this.#readController = undefined;
				this.#pull = undefined;
				this.#readable = null;
				if (!this.#writable && this.#pendingClosePromise)
					this.#pendingClosePromise.resolve(undefined);
					
			}
		}, { highWaterMark: this.#bufferSize });

		return this.#readable;
	}
	get writable() {
		if (this.#writable)
			return this.#writable;

		if (("opened" !== this.#state) || this.#fatal)
			return null;

		this.#writable = new WritableStream({
			start: controller => {
				this.#writeController = controller;
			},
			write: async (chunk /* , controller */) => {
				if (chunk instanceof ArrayBuffer)
					chunk = new Uint8Array(chunk);
				else if (chunk.BYTES_PER_ELEMENT > 1)		// allows ArrayBuffer, SharedArrayBuffer, Uint8Array, Int8Array, DataView. disallows multi-byte element arrays.
					throw new Error("invalid buffer");

				const result = Promise.withResolvers();
				this.#pendingWrite = result;
				result.chunk = chunk;
				result.position = 0;
				try {
					this.#write();
				}
				catch {
					result.reject("NetworkError");
				}
				return result.promise;
			},
			abort: () => {
				this.#writable = null;
				if (!this.#readable && this.#pendingClosePromise)
					this.#pendingClosePromise.resolve(undefined);
			}
			
		}, new ByteLengthQueuingStrategy({highWaterMark: this.#bufferSize}));
		
		return this.#writable;
	}	
	getInfo() {
		// could fill in usbVendorId && usbProductId 
		return {};
	}
	async open(options = {}) {
		return new Promise((resolve, reject) => {
			if ("closed" !== this.#state)
				reject(new Error("InvalidStateError"));
			const dataBits = options.dataBits ?? 8;
			const stopBits = options.stopBits ?? 1;
			const parity = options.parity ?? "none";
			if ((8 !== dataBits) || (1 !== stopBits) || ("none" !== parity))
				return void reject(new TypeError("invalid configurations"));

			const bufferSize = options.bufferSize ?? 255;
			if ((bufferSize <= 0) || (bufferSize > 16384))
				return void reject(new TypeError("invalid bufferSize"));

			this.#state = "opening";
			this.#bufferSize = bufferSize;

			this.#serial = new Serial({
				device: this.#device,
				baud: options.baudRate,
				target: this,
				onReadable(count) {
					if (this.first)
						throw new Error("onReadable before expected onWritable");
					
					const target = this.target;
					if (!target.#readController)
						return void this.read();					// ignore data before readable stream created (can't get to desiredSize)

					trace(`onReadable ${count}\n`);
					target.#readableBytes = count;
					if (target.#pull) {
						const use = target.#read();
						if (use > 0) {
							trace(`promise read ${use}\n`);
							const pull = target.#pull;
							target.#pull = undefined;
							pull.resolve();
						}
					}					
				},
				onWritable(count) {
					const target = this.target;

					if (this.first) {
						delete this.first;
						target.#state = "opened";
						resolve();
					}

					target.#writableBytes = count;
					if (target.#pendingWrite)
						target.#write();
				},
				onError(e) {
					const target = this.target;

					if (target.first)
						return void reject(e);

					target.#fatal = true;
					target.#readController?.error(new Error("NetworkError"));
					target.#writeController?.error(new Error("NetworkError"));
				}
			});
			this.#serial.first = true;
		});
	}
	async setSignals(options) {
		if (options.break)
			throw new Error("break unsupported");

		const {dataTerminalReady, requestToSend} = options;
		if ((undefined === dataTerminalReady) && (undefined === requestToSend))
			return;

		options = {};
		if (undefined !== dataTerminalReady)
			options.DTR = dataTerminalReady;
		if (undefined !== requestToSend)
			options.RTS = requestToSend;
		this.#serial.set(options);
	}
	async getSignals() {
		throw new Error("getSignals unimplemented");
	}
	async close() {
		return new Promise((resolve, reject) => {
			if ("opened" !== this.#state)
				return void reject("InvalidStateError");

			const cancelPromise = this.#readable?.cancel();
			const abortPromise = this.#writable?.abort();
			this.#pendingClosePromise = Promise.withResolvers();
			if (!cancelPromise && !abortPromise)
				this.#pendingClosePromise.resolve(undefined);

			const promises = [this.#pendingClosePromise.promise];
			if (cancelPromise) promises.push(cancelPromise);
			if (abortPromise) promises.push(abortPromise);
			Promise.all(promises)
			.then(() => {
				this.#serial?.close();
				this.#serial = undefined;

				this.#state = "closed";
				this.#fatal = false;
				this.#pendingClosePromise = null;
				resolve();
			})
			.catch(e => {
				this.#pendingClosePromise = null;
				reject(e);
			});

			this.#state = "closing";
		});
	}
	async forget() {
		//@@ no meaning
	}
	
	#read() {
		const byobRequest = this.#readController.byobRequest;
		const desiredSize = byobRequest? byobRequest.view.byteLength : this.#bufferSize;
	
		let use = Math.min(this.#readableBytes, desiredSize);
		if (use > 0) {
			if (byobRequest) {
				trace(`read into ${use}\n`);
				use = this.#serial.read(byobRequest.view);
				byobRequest.respond(use);
			}
			else {
				trace(`enqueue ${use}\n`);
				let view = new Uint8Array(this.#serial.read(use));
				this.#readController.enqueue(view);
			}
			this.#readableBytes -= use;
		}
		return use;
	}
	#write() {
		const chunk = this.#pendingWrite.chunk;
		let position = this.#pendingWrite.position, byteLength = chunk.byteLength;
		
		let use = Math.min(this.#writableBytes, byteLength - position);
		this.#serial.write(chunk.subarray(position, position + use));
		this.#writableBytes -= use;
		position += use;
		if (position === byteLength) {
			this.#pendingWrite.resolve();
			this.#pendingWrite = undefined;
		}
		else
			this.#pendingWrite.position = position;
	}
}

export { WebSerial as serial };
