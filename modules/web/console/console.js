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

const prefixes = Object.freeze({
	none: "",
	warn: "<warn>",
	error: "<error>",
	trace: "<info>",
});

const state = {
	indent: "",
	counts: undefined,
	timers: undefined,
};

function emit(prefix, args) {
	trace(state.indent, prefix);
	for (let i = 0; i < args.length; i++) {
		let value = args[i];
		switch (typeof value) {
			case "string": break;
			case "symbol": value = value.toString(); break;
			default: value = String(value); break;
		}
		if (i)
			trace(" ", value);
		else
			trace(value);
	}
	trace("\n");
}

globalThis.console = Object.freeze({
	log(...args) { return emit(prefixes.none, args); },
	info(...args) { return emit(prefixes.none, args); },
	debug(...args) { return emit(prefixes.none, args); },
	warn(...args) { return emit(prefixes.warn, args); },
	error(...args) { return emit(prefixes.error, args); },
	trace(...args) { return emit(prefixes.trace, args); },
	dir(item) { return emit(prefixes.none, [item]); },
	dirxml(...args) { return emit(prefixes.none, args); },
	assert(condition, ...args) {
		if (condition)
			return;
		return emit(prefixes.error, ["Assertion failed" + (args.length ? ":" : ""), ...args]);
	},
	table(tabularData) { return emit(prefixes.none, [tabularData]); },

	count(label = "default") {
		state.counts ??= new Map;
		const next = (state.counts.get(label) ?? 0) + 1;
		state.counts.set(label, next);
		return emit(prefixes.none, [`${label}: ${next}`]);
	},
	countReset(label = "default") {
		state.counts?.delete(label);
	},

	group(...args) {
		if (args.length)
			emit(prefixes.none, args);
		state.indent += "  ";
	},
	groupCollapsed(...args) {
		if (args.length)
			emit(prefixes.none, args);
		state.indent += "  ";
	},
	groupEnd() {
		if (state.indent.length)
			state.indent = state.indent.slice(0, -2);
	},

	time(label = "default") {
		state.timers ??= new Map;
		state.timers.set(label, Date.now());
	},
	timeLog(label = "default", ...args) {
		const started = state.timers?.get(label);
		if (undefined === started)
			return;
		return emit(prefixes.none, [`${label}: ${Date.now() - started} ms`, ...args]);
	},
	timeEnd(label = "default") {
		const started = state.timers?.get(label);
		if (undefined === started)
			return;
		state.timers.delete(label);
		return emit(prefixes.none, [`${label}: ${Date.now() - started} ms`]);
	},

	clear() {},
});
