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

import TextEncoder from "text/encoder";
import TextDecoder from "text/decoder";

export const NDEF_MAX = 256;
export const NDEF_KEY = Object.freeze([0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7]);

const URI_PREFIX = Object.freeze([
	"", "http://www.", "https://www.", "http://", "https://",
	"tel:", "mailto:", "ftp://anonymous:anonymous@", "ftp://ftp.",
	"ftps://", "sftp://", "smb://", "nfs://", "ftp://", "dav://",
	"news:", "telnet://", "imap:", "rtsp://", "urn:", "pop:",
	"sip:", "sips:", "tftp:", "btspp://", "btl2cap://", "btgoep://",
	"tcpobex://", "irdaobex://", "file://", "urn:epc:id:",
	"urn:epc:tag:", "urn:epc:pat:", "urn:epc:raw:", "urn:epc:", "urn:nfc:",
]);

function encodeUtf8(s) {
	return (new TextEncoder).encode(s);
}

function decodeUtf8(bytes, start, end) {
	return (new TextDecoder).decode((start === undefined) ? bytes : bytes.subarray(start, end));
}

/**
 * Scan Type 2 / Classic TLV bytes.
 * @returns {Uint8Array|null|undefined} message, null if none, undefined if truncated
 */
export function extractNdefTlv(data, length) {
	let i = 0;
	while (i < length) {
		const t = data[i++];
		if (t === 0x00)
			continue;
		if (t === 0xFE)
			return null;
		if (i >= length)
			return;
		let len = data[i++];
		if (len === 0xFF) {
			if (i + 1 >= length)
				return;
			len = (data[i] << 8) | data[i + 1];
			i += 2;
		}
		if (i + len > length)
			return;
		if (t === 0x03)
			return len ? data.slice(i, i + len) : null;
		i += len;
	}
}

function decodeRecord(tnf, typeBytes, payload) {
	const rec = { tnf };
	const type = typeBytes.length ? decodeUtf8(typeBytes) : "";
	if (type)
		rec.type = type;
	if ((tnf === 1) && (type === "U") && payload.length) {
		const prefix = URI_PREFIX[payload[0]] ?? "";
		rec.uri = prefix + decodeUtf8(payload, 1);
	}
	else if ((tnf === 1) && (type === "T") && payload.length) {
		const langLen = payload[0] & 0x3F;
		rec.language = decodeUtf8(payload, 1, 1 + langLen);
		rec.text = decodeUtf8(payload, 1 + langLen);
	}
	else if (payload.length)
		rec.payload = payload.slice();
	return rec;
}

function parseNdef(message) {
	const records = [];
	let i = 0;
	while (i < message.length) {
		const flags = message[i++];
		const tnf = flags & 0x07;
		if (i >= message.length)
			break;
		const typeLen = message[i++];
		let payloadLen;
		if (flags & 0x10) {
			if (i >= message.length)
				break;
			payloadLen = message[i++];
		}
		else {
			if (i + 4 > message.length)
				break;
			payloadLen = ((message[i] << 24) | (message[i + 1] << 16) | (message[i + 2] << 8) | message[i + 3]) >>> 0;
			i += 4;
		}
		let idLen = 0;
		if (flags & 0x08) {
			if (i >= message.length)
				break;
			idLen = message[i++];
		}
		if ((payloadLen > NDEF_MAX) || (i + typeLen + idLen + payloadLen > message.length))
			break;
		const typeBytes = message.subarray(i, i + typeLen);
		i += typeLen + idLen;
		const payload = message.subarray(i, i + payloadLen);
		i += payloadLen;
		records.push(decodeRecord(tnf, typeBytes, payload));
		if (flags & 0x40)
			break;
	}
	return records;
}

function encodeNdefRecord(typeByte, payload) {
	if (payload.length > 255)
		throw new Error("NDEF too large");
	const rec = new Uint8Array(4 + payload.length);
	rec[0] = 0xD1;
	rec[1] = 1;
	rec[2] = payload.length;
	rec[3] = typeByte;
	for (let i = 0; i < payload.length; i++)
		rec[4 + i] = payload[i];
	return rec;
}

export function encodeUriRecord(uri) {
	let code = 0;
	for (let i = 1; i < URI_PREFIX.length; i++) {
		const p = URI_PREFIX[i];
		if (p && uri.startsWith(p) && (p.length > URI_PREFIX[code].length))
			code = i;
	}
	const rest = encodeUtf8(uri.slice(URI_PREFIX[code].length));
	const payload = new Uint8Array(1 + rest.length);
	payload[0] = code;
	for (let i = 0; i < rest.length; i++)
		payload[1 + i] = rest[i];
	return encodeNdefRecord(0x55, payload);
}

export function encodeTextRecord(text, language) {
	const lang = encodeUtf8(language);
	if (lang.length > 63)
		throw new Error("language too long");
	const body = encodeUtf8(text);
	const payload = new Uint8Array(1 + lang.length + body.length);
	payload[0] = lang.length;
	for (let i = 0; i < lang.length; i++)
		payload[1 + i] = lang[i];
	for (let i = 0; i < body.length; i++)
		payload[1 + lang.length + i] = body[i];
	return encodeNdefRecord(0x54, payload);
}

export function wrapTlv(message) {
	const n = message.length;
	const hdr = (n < 0xFF)
		? Uint8Array.of(0x03, n)
		: Uint8Array.of(0x03, 0xFF, n >> 8, n & 0xFF);
	const out = new Uint8Array(hdr.length + n + 1);
	for (let i = 0; i < hdr.length; i++)
		out[i] = hdr[i];
	for (let i = 0; i < n; i++)
		out[hdr.length + i] = message[i];
	out[out.length - 1] = 0xFE;
	return out;
}

export function wrapNdef(message) {
	if (!message || !message.length)
		return;
	const records = parseNdef(message);
	if (!records.length)
		return;
	const out = { records };
	for (let i = 0; i < records.length; i++) {
		if (records[i].uri) {
			out.text = records[i].uri;
			break;
		}
		if (records[i].text) {
			out.text = records[i].text;
			break;
		}
	}
	return out;
}

/** Encode constructor-style { uri } or { text, language? } to a Type 2/Classic TLV. */
export function encodeNdefOptions(options = {}) {
	const hasUri = "uri" in options;
	const hasText = "text" in options;
	if (hasUri === hasText)
		throw new Error("uri or text required");
	let message;
	if (hasUri) {
		if ((typeof options.uri !== "string") || !options.uri)
			throw new Error("invalid uri");
		message = encodeUriRecord(options.uri);
	}
	else {
		if (typeof options.text !== "string")
			throw new Error("invalid text");
		let language = "en";
		if ("language" in options) {
			if ((typeof options.language !== "string") || !options.language)
				throw new Error("invalid language");
			language = options.language;
		}
		message = encodeTextRecord(options.text, language);
	}
	return wrapTlv(message);
}
