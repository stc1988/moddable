/*
 * Copyright (c) 2021-2026  Moddable Tech, Inc.
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
 
import Timer from "timer";

const more = Object.freeze({more: true});

class Connection {
	#socket;
	#from;
	#readable;
	#writable = 0;
	#writePosition;
	#pendingWrite;
	#line = "";
	#state = "receiveRequest";
	#remaining;
	#chunk;
	#options = {};
	#route;
	#timer;

	constructor(from, done) {
		this.#from = from;
		this.#options.done = done;
	}
	close() {
		this.#options?.done?.(this);
		this.#options = undefined;
		this.#socket?.close();
		this.#socket = undefined; 
		this.#from?.close();
		this.#from = undefined;
		Timer.clear(this.#timer); 
		this.#timer = undefined;
	}
	accept(options) {
		const from = this.#from;
		this.#options.onRequest = options.onRequest; 
		this.#options.onReadable = options.onReadable; 
		this.#options.onResponse = options.onResponse; 
		this.#options.onWritable = options.onWritable; 
		this.#options.onDone = options.onDone;
		this.#options.onError = options.onError; 

		this.#socket = new from.constructor({
			from,
			onReadable: count => this.#onReadable(count),
			onWritable: count => this.#onWritable(count),
			onError: () => this.#onError("socket error")
		});
		this.#from = undefined;
	}
	detach() {
		const result = this.#socket ?? this.#from;
		if (!result)
			throw new Error;
		this.#socket = this.#from = undefined;

		this.#options?.done(this);
		delete this.#options.done;

		return new result.constructor({from: result});
	}
	read(count) {
		if ("receiveBody" !== this.#state)
			throw new Error("bad state");

		const available = Math.min(this.#readable, (undefined === this.#chunk) ? this.#remaining : this.#chunk);
		if ((count > available) || (undefined === count))
			count = available;
		if (!count)
			return;

		this.#readable -= count;
		if (undefined === this.#chunk)
			this.#remaining -= count;
		else
			this.#chunk -= count;

		const result = this.#socket.read(count);

		if (0 === this.#chunk) {
			this.#line = "";
			if (this.#readable) {
				this.#timer = Timer.set(() => {
					this.#timer = undefined;
					this.#onReadable(this.#readable);
				});
			}
		}
		else if (0 === this.#remaining)
			this.#timer = Timer.set(() => this.#reply());

		return result;
	}
	write(data) {
		if ("sendResponseBody" !== this.#state)
			throw new Error("bad state");

		if (!data) {
			if ("number" === typeof this.#remaining)
				throw new Error("bad data");

			this.#pendingWrite = ArrayBuffer.fromString("0\r\n\r\n");
			this.#writePosition = 0;
			this.#remaining = 0;
			if (this.#writable >= this.#pendingWrite.byteLength) {
				this.#writable = this.#socket.write(this.#pendingWrite);
				this.#pendingWrite = undefined;
				this.#done();
			}
			else {
				this.#timer = Timer.set(() => {
					this.#timer = undefined;
					if (this.#writable)
						this.#onWritable(this.#writable);
				});
			}
			return 0;
		}

		const byteLength = data.byteLength;
		if (true === this.#remaining) {
			if ((byteLength + 8) > this.#writable)
				throw new Error("would block");

			this.#socket.write(ArrayBuffer.fromString(byteLength.toString(16) + "\r\n"), more);
			this.#socket.write(data, more);
			this.#writable = this.#socket.write(ArrayBuffer.fromString("\r\n"));

			return (this.#writable > 8) ? (this.#writable - 8) : 0;
		}
		else {
			if ((byteLength > this.#writable) || (byteLength > this.#remaining))
				throw new Error("would block");

			this.#writable = this.#socket.write(data);

			this.#remaining -= byteLength;
			if (0 === this.#remaining) {
				this.#done();
				this.#writable = 0;
			}

			return this.#writable;
		}
	}
	#onReadable(count) {
		this.#readable = count;

		while (this.#readable) {
			if (undefined !== this.#line) {
				this.#socket.format = "number";
				while (this.#readable--) {
					const c = this.#socket.read();
					this.#line += String.fromCharCode(c);
					if (10 === c)
						break;
				}
				this.#socket.format = "buffer";

				if (!this.#line.endsWith("\r\n")) {
					this.#readable = 0;
					return;
				}
			}

			switch (this.#state) {
				case "receiveRequest": {
					const status = this.#line.trim().split(" ");
					if ((status.length < 3) || ("HTTP/1.1" !== status[2])) {
						this.#onError("badly formed");
						return;
					}

					this.#options.request = {
						method: status[0],
						headers: new Map
					};
					const query = status[1].indexOf("?");
					if (query < 0) {
						this.#options.request.path = status[1];
						this.#options.request.query = "";
					}
					else {
						this.#options.request.path = status[1].slice(0, query);
						this.#options.request.query = status[1].slice(query + 1);
					}
					this.#line = "";
					this.#state = "receiveHeader";
					} break;

				case "receiveHeader":
					if ("\r\n" !== this.#line) {
						const position = this.#line.indexOf(":");
						const name = this.#line.substring(0, position).trim().toLowerCase();
						let data = this.#line.substring(position + 1).trim();
						this.#options.request.headers.set(name, data);

						if ("content-length" === name) {
							this.#remaining = parseInt(data);
							if (!(this.#remaining >= 0))
								return void this.#onError("badly formed");
						}
						else if ("transfer-encoding" === name) {
							data = data.toLowerCase();
							if ("chunked" === data)
								this.#chunk = 0;
						}

						this.#line = "";
					}
					else {
						if (undefined !== this.#chunk)
							this.#remaining = undefined;		// ignore content-length if chunked

						this.#options.onRequest?.call(this, this.#options.request);
						if (!this.#socket)
							return;
						delete this.#options.request;

						if (!this.#remaining && (undefined == this.#chunk)) {
							this.#line = "";
							this.#reply();
							return;
						}

						this.#state = "receiveBody";
						this.#line = (undefined == this.#chunk) ? undefined : "";
					}
					break;

				case "receiveBody": {
					let count = this.#remaining;
					if (undefined !== this.#chunk) {
						if (0 === this.#chunk) {
							if ("\r\n" === this.#line)
								continue;
							this.#chunk = parseInt(this.#line.trim(), 16);
							if (!(this.#chunk >= 0))
								return void this.#onError("badly formed");
							this.#line = undefined;

							if (0 === this.#chunk) {
								this.#state = "receiveChunkTrailer";
								this.#line = "";
								continue;
							}
						}
						count = this.#chunk;
					}

					if (this.#options.onReadable)
						this.#options.onReadable.call(this, Math.min(this.#readable, count));
					else
						this.read();
					}
					return;

				case "receiveChunkTrailer":
					if ("\r\n" !== this.#line)
						this.#onError("badly formed");
					else
						this.#reply();
					return;

				default:
					this.#onError("bad state");
					return;
			}
		}
	}
	#onWritable(count) {
		this.#writable = count;

		if (this.#state.startsWith("receive"))
			return;		// initial on-writable

		while (this.#writable) {
			if (this.#pendingWrite) {
				const use = Math.min(this.#pendingWrite.byteLength - this.#writePosition, this.#writable);
				this.#writable = this.#socket.write(new Uint8Array(this.#pendingWrite, this.#writePosition, use));
				this.#writePosition += use;
				if (this.#writePosition !== this.#pendingWrite.byteLength)
					return;

				this.#pendingWrite = undefined;
			}

			switch (this.#state) {
				case "sendResponseHeader": {
					const item = this.#options.headers.next();
					if (item.done) {
						this.#pendingWrite = "\r\n";
						this.#state = "sendResponseBody";
						if (101 === this.#options.status)	// 101 Switching Protocols "...the empty line which terminates the 101 response"
							this.#remaining = 0;
						delete this.#options.headers;
					}
					else {
						const name = item.value[0];
						this.#pendingWrite = name + ": " + item.value[1] + "\r\n";
						if ("content-length" === name) {
							this.#remaining = parseInt(item.value[1]);
							if (!(this.#remaining >= 0))
								return void this.#onError("badly formed");
						}
						else if ("transfer-encoding" === name) {
							if ("chunked" === item.value[1])
								this.#remaining = true;
						}
					}
					this.#pendingWrite = ArrayBuffer.fromString(this.#pendingWrite);
					this.#writePosition = 0;
					} break;

				case "sendResponseBody": {
					if (0 === this.#remaining) {
						this.#done();
						return;
					}

					let writable = this.#writable;
					if (true === this.#remaining) {
						writable -= 8;
						if (writable <= 0)
							return;
					}
					else if ((undefined !== this.#remaining) && (writable > this.#remaining))
						writable = this.#remaining;
					this.#options.onWritable?.call(this, writable);
					return;
					}

				case "waitResponse":
				case "done":
					return;

				default:
					this.#onError("bad state");
					return;
			}
		}
	}
	#onError(msg) {
		const onError = this.#options.onError; 
		this.#state = "error";
		this.close();
		onError?.call(this, msg);
	}
	#done() {
		this.#state = "done";
		try {
			this.#options.onDone?.call(this);
		}
		catch {
			/* this space intentionally left blank */
		}
		this.close();
	}
	#reply() {		// request headers & request body received. time to reply.
		this.#timer = undefined;
		this.#state = "waitResponse";

		const response = {
			headers: new Map,
			status: 200
		};
		if (this.#options.onResponse)
			this.#options.onResponse.call(this, response);
		else
			this.respond(response);
	}
	respond(response) {
		this.#state = "sendResponseHeader";

		let pendingWrite = `HTTP/1.1 ${response.status} ${reason(response.status)}\r\n`;
		if (!response.headers.get("connection"))
			pendingWrite += "connection: close\r\n";		// only one request per connection
		this.#pendingWrite = ArrayBuffer.fromString(pendingWrite);
		this.#writePosition = 0;

		this.#options.status = response.status;
		this.#options.headers = response.headers.entries();
		this.#remaining = undefined;

		this.#onWritable(this.#writable);
	}
	get route() {
		return this.#route;
	}
	set route(route) {
		if ("receiveHeader" !== this.#state)
			throw new Error("bad state");

		this.#route = route;
		this.#options.onRequest = route.onRequest;
		this.#options.onReadable = route.onReadable;
		this.#options.onResponse = route.onResponse;
		this.#options.onWritable = route.onWritable;
		this.#options.onDone = route.onDone;
		this.#options.onError = route.onError;

		this.#options.onRequest?.call(this, this.#options.request);
		delete this.#options?.request;
	}

	static {
		this.prototype[Symbol.dispose] = this.prototype.close;
	}
}

class HTTPServer {
	#onConnect;
	#listener;
	#connections = new Set;
	#onRoute;

	constructor(options) {
		this.#onConnect = options.onConnect;
		this.#onRoute = options.onRoute;
		if (!this.#onConnect === !this.#onRoute)
			throw new Error("invalid");

		this.#listener = new options.socket.io({
			...options.socket,
			port: options.port ?? 80,
			target: this,
			onReadable(count) {
				while (count--) {
					const connection = new Connection(this.read(), connection => this.target.#connections?.delete(connection));
					this.target.#connections.add(connection);
					if (this.target.#onRoute) {
						connection.accept({
							onRequest: request => {
								try {
									connection.route = this.target.#onRoute.call(this.target, request) || {
										onResponse(response) {
											response.status = 404;
											response.headers.set("content-length", 0);
											this.respond(response);
										}
									};
								}
								catch {
									connection.close();
								}
							}
						});
					}
					else {
						try {
							this.target.#onConnect(connection);
						}
						catch {
							connection.close();
						}
					}
				}
			}
		});
	}
	close() {
		this.#connections?.forEach(connection => connection.close());
		this.#connections = undefined;
		this.#listener?.close();
		this.#listener = undefined;
	}
	get port() {
		return this.#listener.port;
	}

	static {
		this.prototype[Symbol.dispose] = this.prototype.close;
	}
}

const message = `
100 Continue
101 Switching Protocols
200 OK
201 Created
202 Accepted
203 Non-Authoritative Information
204 No Content
205 Reset Content
206 Partial Content
207 Multi-Status
300 Multiple Choices
301 Moved Permanently
302 Found
303 See Other
304 Not Modified
305 Use Proxy
307 Temporary Redirect
400 Bad Request
401 Unauthorized
402 Payment Required
403 Forbidden
404 Not Found
405 Method Not Allowed
406 Not Acceptable
407 Proxy Authentication Required
408 Request Timeout
409 Conflict
410 Gone
411 Length Required
412 Precondition Failed
413 Request Entity Too Large
414 Request-URI Too Long
415 Unsupported Media Type
416 Requested Range Not Satisfiable
417 Expectation Failed
500 Internal Server Error
501 Not Implemented
502 Bad Gateway
503 Service Unavailable
504 Gateway Timeout
505 HTTP Version Not Supported
`;

function reason(status)
{
	const index = message.indexOf(`\n${status} `);
	if (index < 0) return "";
	return message.slice(index + 5, message.indexOf("\n", index + 1));
}

export default HTTPServer;
