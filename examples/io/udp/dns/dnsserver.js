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

import DNS from "dns";
import Parser from "dns/parser";
import Serializer from "dns/serializer";

class Server {
	#socket;
	#onResolve;
	#ttl;

	constructor(options) {
		this.#ttl = options.ttl ?? 60;
		this.#onResolve = options.onResolve;
		if (!this.#onResolve)
			throw new Error("onResolve required");

		this.#socket = new (options.socket.io)({
			target: this,
			port: options.port ?? 53,
			onReadable(count) {
				while (count--) {
					const buffer = this.read();
					this.target.#receive(buffer);
				}
			}
		});
	}
	close() {
		this.#socket?.close();
		this.#socket = this.#onResolve = undefined;
	}
	#receive(buffer) {
		const packet = new Parser(buffer);
		const question = packet.question(0);
		if (!question || (DNS.CLASS.IN !== question.qclass) || (DNS.RR.A !== question.qtype))
			return;

		const name = question.qname.join(".");
		const resolved = this.#onResolve?.(name);
		if (!resolved)
			return;

		const response = new Serializer({query: false, authoritative: true, id: packet.id});
		response.add(DNS.SECTION.QUESTION, name, DNS.RR.A, DNS.CLASS.IN);
		response.add(DNS.SECTION.ANSWER, name, DNS.RR.A, DNS.CLASS.IN, this.#ttl, resolved);
		this.#socket.write(response.build(), buffer.address, buffer.port);
	}

	static {
		this.prototype[Symbol.dispose] = this.prototype.close;
	}
}

export default Server;
