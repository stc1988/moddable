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

/*
	NXP PN532 NFC / RFID reader over I2C (ECMA-419 sensor).

	Module: embedded:sensor/RFID/PN532

	Framed host-controller protocol (not a register map). Use device.io.I2C,
	not SMBus. Default 7-bit address 0x24.

	ISO14443A UID poll + on-demand NDEF (Type 2 / Classic / Type 4)
	+ MIFARE Classic sector auth / block read / write.

	Constructor options: sensor, reset, interrupt, target.
	configure({ antenna }) turns the RF field on or off.
	Call sample() from the application (IRQ is command-ready, not card-present).

	MIFARE Classic (factory keys FFFFFFFFFFFFh):
	  start({ block, key?, keyType? })
	  readBlock(block) -> Uint8Array(16)
	  writeBlock(block, data)  // Uint8Array(16)
	  stop()

	NDEF (Type 2 NTAG / Ultralight, and Classic sector 1):
	  readNDEF()
	  writeNDEF({ uri }) or writeNDEF({ text, language? })
*/

import Timer from "timer";
import { extractNdefTlv, wrapNdef, encodeNdefOptions, NDEF_KEY, NDEF_MAX } from "embedded:sensor/RFID/ndef";

const Command = Object.freeze({
	GetFirmwareVersion: 0x02,
	SAMConfiguration: 0x14,
	RFConfiguration: 0x32,
	InListPassiveTarget: 0x4A,
	InDataExchange: 0x40,
	InRelease: 0x52,
});

const PICC = Object.freeze({
	MF_AUTH_KEY_A: 0x60,
	MF_AUTH_KEY_B: 0x61,
	MF_READ: 0x30,
	MF_WRITE: 0xA0,
	MF_UL_WRITE: 0xA2,
});

const TFI_HOST = 0xD4;
const TFI_PN532 = 0xD5;
const I2C_READY = 0x01;
const BRTY_ISO14443A = 0x00;

const MF_KEY_SIZE = 6;
const MF_BLOCK_SIZE = 16;
const DEFAULT_KEY = Uint8Array.of(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
const DEFAULT_ADDRESS = 0x24;
const PACKET = 64;
const CMD_SIZE = 20;		// writeBlock: InDataExchange + Tg + WRITE + addr + 16

const CMD_GET_FW = Uint8Array.of(Command.GetFirmwareVersion);
const CMD_RF_MAX_RETRIES = Uint8Array.of(Command.RFConfiguration, 0x05, 0xFF, 0x01, 0x00);
const CMD_RF_ON = Uint8Array.of(Command.RFConfiguration, 0x01, 0x01);
const CMD_RF_OFF = Uint8Array.of(Command.RFConfiguration, 0x01, 0x00);
const CMD_IN_LIST = Uint8Array.of(Command.InListPassiveTarget, 1, BRTY_ISO14443A);
const CMD_IN_RELEASE = Uint8Array.of(Command.InRelease, 0x00);
const APDU_SELECT_AID = Uint8Array.of(
	0x00, 0xA4, 0x04, 0x00, 0x07,
	0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01, 0x00
);
const APDU_SELECT_CC = Uint8Array.of(0x00, 0xA4, 0x00, 0x0C, 0x02, 0xE1, 0x03);
const APDU_READ_CC = Uint8Array.of(0x00, 0xB0, 0x00, 0x00, 0x0F);
const APDU_READ_NLEN = Uint8Array.of(0x00, 0xB0, 0x00, 0x00, 0x02);

const identification = Object.freeze({
	model: "PN532",
	classification: "RFID",
	description: "13.56 MHz ISO14443A NFC/RFID reader (I2C)",
});

function typeName(sak) {
	if (sak & 0x04)
		return "incomplete";
	switch (sak) {
		case 0x09: return "MIFARE Mini";
		case 0x08: return "MIFARE 1K";
		case 0x18: return "MIFARE 4K";
		case 0x00: return "MIFARE Ultralight";
		case 0x10:
		case 0x11: return "MIFARE Plus";
		default: break;
	}
	if (sak & 0x20)
		return "ISO14443-4";
	if (sak & 0x40)
		return "ISO18092";
	return "Unknown";
}

function uidEqual(a, b) {
	if (!a || !b || (a.length !== b.length))
		return false;
	for (let i = 0; i < a.length; i++) {
		if (a[i] !== b[i])
			return false;
	}
	return true;
}

function packCommand(dest, cmd) {
	const len = cmd.length + 1;
	dest[0] = 0x00;
	dest[1] = 0x00;
	dest[2] = 0xFF;
	dest[3] = len;
	dest[4] = (~len + 1) & 0xFF;
	dest[5] = TFI_HOST;
	let sum = TFI_HOST;
	for (let i = 0; i < cmd.length; i++) {
		dest[6 + i] = cmd[i];
		sum = (sum + cmd[i]) & 0xFF;
	}
	dest[6 + cmd.length] = (~sum + 1) & 0xFF;
	dest[7 + cmd.length] = 0x00;
	return 8 + cmd.length;
}

function findStart(buf, length) {
	for (let i = 0; i < length - 5; i++) {
		if ((buf[i] === 0x00) && (buf[i + 1] === 0x00) && (buf[i + 2] === 0xFF))
			return i;
	}
	return -1;
}

export default class PN532 {
	#io;
	#reset;
	#interrupt;
	#antenna = true;
	#lastUid;
	#present = false;
	#version = 0;
	#tx = new Uint8Array(PACKET);
	#rx = new Uint8Array(PACKET);
	#rdy = new Uint8Array(1);
	#cmd = new Uint8Array(CMD_SIZE);
	#ndef = new Uint8Array(NDEF_MAX);

	constructor(options) {
		if ("target" in options)
			this.target = options.target;

		const sensor = options.sensor;
		if (!sensor?.io)
			throw new Error("sensor required");

		try {
			if ("reset" in options) {
				const reset = options.reset;
				this.#reset = new reset.io({
					...reset,
					mode: reset.io.Output
				});
			}

			if ("interrupt" in options) {
				const interrupt = options.interrupt;
				this.#interrupt = new interrupt.io({
					...interrupt,
					mode: interrupt.io.Input
				});
			}

			this.#io = new sensor.io({
				hz: 100_000,
				address: DEFAULT_ADDRESS,
				timeout: 1000,
				...sensor
			});

			this.#init();
		}
		catch (e) {
			this.close();
			throw e;
		}
	}

	configure(options) {
		if ("antenna" in options) {
			this.#antenna = options.antenna;
			this.#setRF(options.antenna);
		}
	}

	get configuration() {
		return {
			antenna: this.#antenna,
		};
	}

	sample() {
		try {
			const card = this.#listTarget(80);
			if (card) {
				const changed = !this.#present || !uidEqual(this.#lastUid, card.uid);
				this.#present = true;
				this.#lastUid = card.uid;
				this.#release();
				if (changed)
					return {
						present: true,
						uid: card.uid,
						uidHex: card.uid.toHex(),
						sak: card.sak,
						type: typeName(card.sak),
					};
			}
			else if (this.#present) {
				this.#present = false;
				this.#lastUid = undefined;
				return { present: false };
			}
		}
		catch {
			return;
		}
	}

	get identification() {
		return identification;
	}

	get version() {
		return this.#version;
	}

	close() {
		try {
			this.#release();
		}
		catch {
		}
		try {
			this.#setRF(false);
		}
		catch {
		}
		this.#io?.close();
		this.#io = undefined;
		this.#reset?.close();
		this.#reset = undefined;
		this.#interrupt?.close();
		this.#interrupt = undefined;
	}

	start(options = {}) {
		const block = options.block;
		const key = options.key ?? DEFAULT_KEY;
		const auth = (options.keyType === "B") ? PICC.MF_AUTH_KEY_B : PICC.MF_AUTH_KEY_A;
		const tries = (typeof options.retries === "number") ? options.retries : 3;

		let lastError = "start failed";
		for (let n = 0; n < tries; n++) {
			this.#release();
			const card = this.#listTarget(200);
			if (!card) {
				lastError = "no card";
				if (n + 1 < tries)
					Timer.delay(15);
				continue;
			}

			const send = this.#cmd;
			send[0] = Command.InDataExchange;
			send[1] = 1;
			send[2] = auth;
			send[3] = block;
			for (let i = 0; i < MF_KEY_SIZE; i++)
				send[4 + i] = key[i];
			const uid = card.uid;
			const base = uid.length - 4;
			for (let i = 0; i < 4; i++)
				send[10 + i] = uid[base + i];

			try {
				const payload = this.#transact(send.subarray(0, 14), 200);
				if (payload[0] === 0) {
					this.#lastUid = uid;
					return { uid, sak: card.sak, type: typeName(card.sak) };
				}
				lastError = `start failed (${payload[0]})`;
			}
			catch (e) {
				lastError = e;
			}
			if (n + 1 < tries)
				Timer.delay(15);
		}
		throw new Error(lastError);
	}

	/**
	 * Read one 16-byte block (MIFARE Classic). Sector must be authenticated.
	 * @param {number} block
	 * @returns {Uint8Array} length 16
	 */
	readBlock(block) {
		const send = this.#cmd;
		send[0] = Command.InDataExchange;
		send[1] = 1;
		send[2] = PICC.MF_READ;
		send[3] = block;
		const payload = this.#transact(send.subarray(0, 4), 200);
		if (payload[0] !== 0)
			throw new Error(`readBlock failed (${payload[0]})`);
		if (payload.length < 1 + MF_BLOCK_SIZE)
			throw new Error("short read");
		return payload.slice(1, 1 + MF_BLOCK_SIZE);
	}

	/**
	 * Write 16 bytes to one MIFARE Classic block. Sector must be authenticated.
	 * Avoid block 0 and sector trailers (3, 7, 11, ...) unless you know the access bits.
	 *
	 * @param {number} block
	 * @param {ArrayLike<number>|ArrayBuffer} data  Exactly 16 bytes
	 */
	writeBlock(block, data) {
		const send = this.#cmd;
		send[0] = Command.InDataExchange;
		send[1] = 1;
		send[2] = PICC.MF_WRITE;
		send[3] = block;
		for (let i = 0; i < MF_BLOCK_SIZE; i++)
			send[4 + i] = data[i];

		const payload = this.#transact(send.subarray(0, 4 + MF_BLOCK_SIZE), 200);
		if (payload[0] !== 0)
			throw new Error(`writeBlock failed (${payload[0]})`);
	}

	stop() {
		this.#release();
	}

	readNDEF() {
		this.#release();
		const card = this.#listTarget(200);
		if (!card)
			return;
		try {
			return this.#tryNDEF(card);
		}
		finally {
			this.#release();
		}
	}

	writeNDEF(options = {}) {
		const tlv = encodeNdefOptions(options);

		this.#release();
		const card = this.#listTarget(200);
		if (!card)
			throw new Error("no card");
		try {
			if (card.sak === 0x00)
				this.#writeNDEFType2(card, tlv);
			else if ((card.sak === 0x08) || (card.sak === 0x18) || (card.sak === 0x09))
				this.#writeNDEFClassic(card, tlv);
			else
				throw new Error("NDEF write requires Type 2 or Classic tag");
			return this.#tryNDEF(card);
		}
		finally {
			this.#release();
		}
	}

	#init() {
		if (this.#reset) {
			this.#reset.write(0);
			Timer.delay(2);
			this.#reset.write(1);
			Timer.delay(10);
		}

		let version;
		for (let n = 0; n < 3; n++) {
			try {
				const payload = this.#transact(CMD_GET_FW, 200);
				if (payload.length >= 4) {
					version = ((payload[0] << 24) | (payload[1] << 16) | (payload[2] << 8) | payload[3]) >>> 0;
					break;
				}
			}
			catch {
			}
		}
		if (version === undefined)
			throw new Error("PN532 not found");
		this.#version = version;

		const sam = this.#cmd;
		sam[0] = Command.SAMConfiguration;
		sam[1] = 0x01;
		sam[2] = 0x14;
		sam[3] = this.#interrupt ? 0x01 : 0x00;
		this.#transact(sam.subarray(0, 4), 200);
		// CfgItem 5 MaxRetries: one passive-activation try so poll does not block
		this.#transact(CMD_RF_MAX_RETRIES, 200);
		this.#setRF(true);
	}

	#setRF(on) {
		if (this.#io)
			this.#transact(on ? CMD_RF_ON : CMD_RF_OFF, 100);
	}

	#listTarget(timeout) {
		let payload;
		try {
			payload = this.#transact(CMD_IN_LIST, timeout);
		}
		catch {
			return;
		}
		if (!payload || (payload[0] !== 1))
			return;
		// NbTg, Tg, ATQA(2), SAK, NFCIDLen, NFCID
		if (payload.length < 6)
			return;
		const uidLen = payload[5];
		if ((uidLen < 4) || (uidLen > 10) || (payload.length < 6 + uidLen))
			return;
		const uid = new Uint8Array(uidLen);
		for (let i = 0; i < uidLen; i++)
			uid[i] = payload[6 + i];
		return { uid, sak: payload[4] };
	}

	#release() {
		if (!this.#io)
			return;
		try {
			this.#transact(CMD_IN_RELEASE, 80);
		}
		catch {
		}
	}

	#tryNDEF(card) {
		const sak = card.sak;
		if (sak === 0x00)
			return this.#ndefType2();
		if (sak & 0x20)
			return this.#ndefType4();
		if ((sak === 0x08) || (sak === 0x18) || (sak === 0x09))
			return this.#ndefClassic(card);
		try {
			const t2 = this.#ndefType2();
			if (t2)
				return t2;
		}
		catch {
		}
		return this.#ndefClassic(card);
	}

	#read16(addr) {
		const send = this.#cmd;
		send[0] = Command.InDataExchange;
		send[1] = 1;
		send[2] = PICC.MF_READ;
		send[3] = addr;
		const payload = this.#transact(send.subarray(0, 4), 200);
		if (payload[0] !== 0)
			throw new Error("read failed");
		if (payload.length < 1 + MF_BLOCK_SIZE)
			throw new Error("short read");
		return payload.subarray(1, 1 + MF_BLOCK_SIZE);
	}

	#ndefType2() {
		const buf = this.#ndef;
		let filled = 0;
		let page = 4;
		while (filled < NDEF_MAX) {
			let chunk;
			try {
				chunk = this.#read16(page);
			}
			catch {
				break;
			}
			const n = Math.min(MF_BLOCK_SIZE, NDEF_MAX - filled);
			for (let i = 0; i < n; i++)
				buf[filled + i] = chunk[i];
			filled += n;
			page += 4;
			const msg = extractNdefTlv(buf, filled);
			if (msg === undefined)
				continue;
			return wrapNdef(msg);
		}
		return wrapNdef(extractNdefTlv(buf, filled));
	}

	#writeNDEFType2(card, tlv) {
		if (card.sak !== 0x00)
			throw new Error("NDEF write requires Type 2 tag");
		const cc = this.#read16(3);
		if (cc[0] !== 0xE1)
			throw new Error("not NDEF formatted");
		const capacity = cc[2] * 8;
		if (!capacity || (tlv.length > capacity))
			throw new Error("NDEF too large");
		let page = 4;
		for (let i = 0; i < tlv.length; i += 4)
			this.#writePage(page++, tlv, i);
	}

	#writePage(page, bytes, offset) {
		const send = this.#cmd;
		send[0] = Command.InDataExchange;
		send[1] = 1;
		send[2] = PICC.MF_UL_WRITE;
		send[3] = page;
		send[4] = bytes[offset] ?? 0;
		send[5] = bytes[offset + 1] ?? 0;
		send[6] = bytes[offset + 2] ?? 0;
		send[7] = bytes[offset + 3] ?? 0;
		const payload = this.#transact(send.subarray(0, 8), 200);
		if (payload[0] !== 0)
			throw new Error(`writePage failed (${payload[0]})`);
		Timer.delay(5);
	}

	#authListed(uid, block, key) {
		const send = this.#cmd;
		send[0] = Command.InDataExchange;
		send[1] = 1;
		send[2] = PICC.MF_AUTH_KEY_A;
		send[3] = block;
		for (let i = 0; i < MF_KEY_SIZE; i++)
			send[4 + i] = key[i];
		const base = uid.length - 4;
		for (let i = 0; i < 4; i++)
			send[10 + i] = uid[base + i];
		try {
			const payload = this.#transact(send.subarray(0, 14), 200);
			return payload[0] === 0;
		}
		catch {
			return false;
		}
	}

	/** Failed auth may drop the listed PICC; re-list before the next key. */
	#authWithKeys(card, block) {
		if (this.#authListed(card.uid, block, NDEF_KEY))
			return true;
		this.#release();
		const again = this.#listTarget(200);
		if (!again)
			return false;
		card.uid = again.uid;
		card.sak = again.sak;
		return this.#authListed(card.uid, block, DEFAULT_KEY);
	}

	#writeNDEFClassic(card, tlv) {
		if (tlv.length > 48)
			throw new Error("NDEF too large");
		if (!this.#authWithKeys(card, 4))
			throw new Error("start failed");
		const padded = this.#ndef;
		const n = tlv.length;
		for (let i = 0; i < 48; i++)
			padded[i] = (i < n) ? tlv[i] : 0;
		this.writeBlock(4, padded.subarray(0, 16));
		this.writeBlock(5, padded.subarray(16, 32));
		this.writeBlock(6, padded.subarray(32, 48));
	}

	#ndefClassic(card) {
		if (!this.#authWithKeys(card, 4))
			return;
		const buf = this.#ndef;
		let o = 0;
		for (const block of [4, 5, 6]) {
			const chunk = this.#read16(block);
			for (let i = 0; i < MF_BLOCK_SIZE; i++)
				buf[o++] = chunk[i];
		}
		return wrapNdef(extractNdefTlv(buf, 48));
	}

	#exchange(data) {
		const n = data.length;
		const total = 2 + n;
		const send = (total <= this.#cmd.length) ? this.#cmd : new Uint8Array(total);
		for (let i = n - 1; i >= 0; i--)
			send[2 + i] = data[i];
		send[0] = Command.InDataExchange;
		send[1] = 1;
		const payload = this.#transact((send.length === total) ? send : send.subarray(0, total), 300);
		if (payload[0] !== 0)
			throw new Error(`exchange (${payload[0]})`);
		return payload.subarray(1);
	}

	#apdu(bytes) {
		const resp = this.#exchange(bytes);
		if (resp.length < 2)
			throw new Error("short APDU");
		const sw = (resp[resp.length - 2] << 8) | resp[resp.length - 1];
		if (sw !== 0x9000)
			throw new Error(`APDU 0x${sw.toString(16)}`);
		return resp.subarray(0, resp.length - 2);
	}

	#ndefType4() {
		this.#apdu(APDU_SELECT_AID);
		this.#apdu(APDU_SELECT_CC);
		const cc = this.#apdu(APDU_READ_CC);
		let fileId0 = 0xE1, fileId1 = 0x04;
		for (let i = 0; i + 7 < cc.length; i++) {
			if ((cc[i] === 0x04) && (cc[i + 1] === 0x06)) {
				fileId0 = cc[i + 2];
				fileId1 = cc[i + 3];
				break;
			}
		}
		const sel = this.#cmd;
		sel[0] = 0x00;
		sel[1] = 0xA4;
		sel[2] = 0x00;
		sel[3] = 0x0C;
		sel[4] = 0x02;
		sel[5] = fileId0;
		sel[6] = fileId1;
		this.#apdu(sel.subarray(0, 7));
		const nlenBuf = this.#apdu(APDU_READ_NLEN);
		if (nlenBuf.length < 2)
			return;
		const nlen = (nlenBuf[0] << 8) | nlenBuf[1];
		if ((nlen <= 0) || (nlen > NDEF_MAX))
			return;
		const msg = new Uint8Array(nlen);
		let got = 0;
		while (got < nlen) {
			const off = 2 + got;
			const chunk = Math.min(32, nlen - got);
			const read = this.#cmd;
			read[0] = 0x00;
			read[1] = 0xB0;
			read[2] = off >> 8;
			read[3] = off & 0xFF;
			read[4] = chunk;
			const data = this.#apdu(read.subarray(0, 5));
			const n = Math.min(data.length, nlen - got);
			for (let i = 0; i < n; i++)
				msg[got + i] = data[i];
			got += n;
			if (!n)
				break;
		}
		return wrapNdef(got ? msg.subarray(0, got) : undefined);
	}

	#transact(cmd, timeout) {
		const n = packCommand(this.#tx, cmd);
		this.#io.write(this.#tx.subarray(0, n));
		if (!this.#waitReady(timeout))
			throw new Error("PN532 timeout");
		if (!this.#readAck())
			throw new Error("PN532 no ACK");
		if (!this.#waitReady(timeout))
			throw new Error("PN532 timeout");
		return this.#readResponse(cmd[0] + 1);
	}

	#waitReady(timeout) {
		const steps = Math.max(1, Math.ceil(timeout / 5));
		for (let i = 0; i < steps; i++) {
			if (this.#interrupt) {
				if (this.#interrupt.read() === 0)
					return true;
			}
			else {
				this.#io.read(this.#rdy);
				if (this.#rdy[0] === I2C_READY)
					return true;
			}
			Timer.delay(5);
		}
		return false;
	}

	#readAck() {
		const buf = this.#rx;
		this.#io.read(buf.subarray(0, 7));
		for (let i = 0; i <= 1; i++) {
			if ((buf[i] === 0x00) && (buf[i + 1] === 0x00) && (buf[i + 2] === 0xFF)
					&& (buf[i + 3] === 0x00) && (buf[i + 4] === 0xFF) && (buf[i + 5] === 0x00))
				return true;
		}
		return false;
	}

	#readResponse(expectCmd) {
		const buf = this.#rx;
		this.#io.read(buf);
		const start = findStart(buf, buf.length);
		if (start < 0)
			throw new Error("PN532 bad frame");
		const len = buf[start + 3];
		const lcs = buf[start + 4];
		if (((len + lcs) & 0xFF) !== 0)
			throw new Error("PN532 length checksum");
		if (len === 0)
			throw new Error("PN532 unexpected ACK");
		if (len === 1)
			throw new Error(`PN532 error (0x${buf[start + 5].toString(16)})`);
		if (buf[start + 5] !== TFI_PN532)
			throw new Error("PN532 unexpected TFI");
		if (buf[start + 6] !== expectCmd)
			throw new Error("PN532 unexpected response");
		const payloadLen = len - 2;
		if (start + 7 + payloadLen + 1 > buf.length)
			throw new Error("PN532 short frame");
		let sum = TFI_PN532 + expectCmd;
		for (let i = 0; i < payloadLen; i++)
			sum = (sum + buf[start + 7 + i]) & 0xFF;
		if (((sum + buf[start + 7 + payloadLen]) & 0xFF) !== 0)
			throw new Error("PN532 data checksum");
		const payload = new Uint8Array(payloadLen);
		for (let i = 0; i < payloadLen; i++)
			payload[i] = buf[start + 7 + i];
		return payload;
	}

	static {
		this.prototype[Symbol.dispose] = this.prototype.close;
	}
}
