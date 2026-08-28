/*
 * Copyright (c) 2016-2026  Moddable Tech, Inc.
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

/*
	xml
	Contributed by Mark Wharton.
*/

// the native parser calls these at run time (see modXML.rl); reference them
// here so the linker does not dead-strip them
void [String.fromCodePoint, "".concat, "".trim];

function escape(string, mode) {
	return native("xs_xml_escape").call(this, string, mode);
}

export class XML {
	static escape = escape;
	static parse(string, compact) {
		return native("xs_xml_parse").call(this, string, compact);
	}
	// with a prefix, match "prefix:name" or bare "name" (as before); without one,
	// also match any "anyprefix:name" so lookups do not depend on the namespace
	// prefix a server chooses (namespace URIs are not resolved)
	static search(items, name, prefix) {
		const qualified = prefix ? `${prefix}:${name}` : null;
		return items?.find(item => {
			const itemName = item.name;
			if ((itemName === name) || (itemName === qualified))
				return true;
			if (prefix)
				return false;
			const colon = itemName.indexOf(":");
			return (colon >= 0) && (itemName.slice(colon + 1) === name);
		});
	}
	static searchAttributes(element, name, prefix) {
		return this.search(element.attributes, name, prefix);
	}
	static searchElements(element, name, prefix) {
		return this.search(element.elements, name, prefix);
	}
	static serialize(item, declaration = true) {
		const parts = declaration ? [`<?xml version="1.0" encoding="utf-8"?>`] : [];
		serializeItem(item, parts);
		return parts.join("");
	}
}

// build into a shared array of parts, joined once by serialize (see the
// "Build a String" optimization guide)
function serializeItem(item, parts) {
	parts.push(`<${item.name}`);
	if (Array.isArray(item.attributes)) {
		for (const attribute of item.attributes) {
			const name = attribute.name;
			if ("value" in attribute)
				parts.push(` ${name}="`, escape(attribute.value, 1), `"`);
			else
				parts.push(` ${name}`);
		}
	}
	else if (item.attributes instanceof Object) {
		for (const name of Object.keys(item.attributes)) {
			const value = item.attributes[name];
			if (typeof value === "boolean") {
				if (value)
					parts.push(` ${name}`);
			}
			else
				parts.push(` ${name}="`, escape(value, 1), `"`);
		}
	}
	const elements = item.elements ?? [];
	if (elements.length === 0 && item.text === undefined && item.TEXT === undefined) {
		parts.push(`/>`);
		return;
	}
	parts.push(`>`);
	for (const element of elements) {
		serializeItem(element, parts);
	}
	if (item.text !== undefined) {
		parts.push(escape(item.text));
	}
	if (item.TEXT !== undefined) {
		parts.push(`<![CDATA[`, escape(item.TEXT, 2), `]]>`);
	}
	parts.push(`</${item.name}>`);
}

export default XML;
