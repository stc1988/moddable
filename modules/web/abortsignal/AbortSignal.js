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

import { DOMException } from "web/domexception";

const AbortSignalSymbol = Symbol("AbortSignal");
class AbortSignal {
	#aborted = false;
	#dependents = null;
	#eventListeners = { abort:[] };
	#reason = undefined;
	constructor(symbol) {
		if (symbol != AbortSignalSymbol)
			throw new TypeError("new AbortSignal");
	}
	get aborted() {
		return this.#aborted;
	}
	get reason() {
		return this.#reason;
	}
	addEventListener(event, listener) {
		let listeners = this.#eventListeners[event];
		if (!listeners)
			throw new Error("no such event");
		listeners.push(listener);
	}
	removeEventListener(event, listener) {
		let listeners = this.#eventListeners[event];
		if (!listeners)
			throw new Error("no such event");
		let index = listeners.find(item => item === listener);
		if (index >= 0)
			listeners.splice(index, 1);
	}
	trigger(reason) {
		if (this.#aborted)
			return;
		if (reason === undefined) {
			reason = new DOMException("", "AbortError");
			reason.code = 20;
		}
		this.#aborted = true;
		this.#reason = reason;
		const event = {
			signal:this,
		}
		this.#eventListeners.abort.forEach(eventListener => eventListener.call(null, event));
		this.#dependents?.forEach(dependent => triggerAbortSignal(dependent, reason));
		this.#dependents = null;
	}
	
	
	static abort(reason) {
		const signal = new AbortSignal(AbortSignalSymbol);
		signal.trigger(reason);
		return signal;
	}
	static any(iterable) {
		const signal = new AbortSignal(AbortSignalSymbol);
		const sources = [];
		for (let source of iterable) {
			if (sources.indexOf(source) < 0)
				source.push(source);
		}
		for (let source of sources) {
			if (source.aborted) {
				signal.trigger(source.reason);
				return signal;
			}
		}
		for (let source of sources) {
			if (source.dependents)
				source.dependents.push(signal);
			else
				source.dependents = [ signal ];
		}
		return signal;
	}
// 	static timeout(duration) {
// 		const signal = new AbortSignal(AbortSignalSymbol);
// 		setTimeout(() => {
// 			triggerAbortSignal(signal, new Error());
// 		}, duration);
// 		return signal;
// 	}
}

class AbortController {
	#signal;
	constructor() {
		this.#signal = new AbortSignal(AbortSignalSymbol);
	}
	get signal() {
		return this.#signal;
	}
	abort(reason) {
		this.#signal.trigger(reason);
	}
}

export { 
	AbortSignal,
	AbortController,
} 
