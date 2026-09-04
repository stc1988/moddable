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
	MFRC522 / WS1850S RFID reader over I2C (ECMA-419 sensor).

	Module: embedded:sensor/RFID/MFRC522

	I2C register addresses are plain PCD_Register values (no << 1), matching
	M5Stack / Arozcan MFRC522-I2C (default address 0x28).

	ISO14443A UID poll + on-demand NDEF (Type 2 / Classic)
	+ MIFARE Classic sector auth / block read / write.
	Soft-reset by default - do not pass reset when RST is shared with LCD_RESET (M5Dial G8).

	Constructor options: sensor, reset, target.
	configure({ antenna }) turns the RF field on or off.
	Call sample() from the application (no card-present interrupt).

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

const Register = Object.freeze({
	Command: 0x01,
	ComIrq: 0x04,
	DivIrq: 0x05,
	Error: 0x06,
	Status2: 0x08,
	FIFOData: 0x09,
	FIFOLevel: 0x0A,
	Control: 0x0C,
	BitFraming: 0x0D,
	Coll: 0x0E,
	Mode: 0x11,
	TxControl: 0x14,
	TxASK: 0x15,
	CRCResultH: 0x21,
	CRCResultL: 0x22,
	TMode: 0x2A,
	TPrescaler: 0x2B,
	TReloadH: 0x2C,
	TReloadL: 0x2D,
	Version: 0x37,
});

const PCD = Object.freeze({
	Idle: 0x00,
	CalcCRC: 0x03,
	Transceive: 0x0C,
	MFAuthent: 0x0E,
	SoftReset: 0x0F,
});

const PICC = Object.freeze({
	REQA: 0x26,
	WUPA: 0x52,
	CT: 0x88,
	SEL_CL1: 0x93,
	SEL_CL2: 0x95,
	HLTA: 0x50,
	MF_AUTH_KEY_A: 0x60,
	MF_AUTH_KEY_B: 0x61,
	MF_READ: 0x30,
	MF_WRITE: 0xA0,
	MF_UL_WRITE: 0xA2,
});

const Status = Object.freeze({
	OK: 1,
	ERROR: 2,
	COLLISION: 3,
	TIMEOUT: 4,
	NO_ROOM: 5,
	CRC_WRONG: 8,
	MIFARE_NACK: 0xFF,
});

const MF_ACK = 0x0A;
const MF_KEY_SIZE = 6;
const MF_BLOCK_SIZE = 16;
/** Factory-default MIFARE Classic key (all sectors at delivery). */
const DEFAULT_KEY = Uint8Array.of(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);

const DEFAULT_ADDRESS = 0x28;

const identification = Object.freeze({
	model: "WS1850S/MFRC522",
	classification: "RFID",
	description: "13.56 MHz ISO14443A RFID/NFC reader (I2C)",
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

export default class MFRC522 {
	#io;
	#resetPin;
	#antenna = true;
	#lastUid;
	#present = false;
	#crypto = false;
	#fifoWrite = new Uint8Array(64);
	#back = new Uint8Array(18);
	#cmd = new Uint8Array(18);
	#crc = new Uint8Array(2);
	#uidBytes = new Uint8Array(10);
	#sakBuf = new Uint8Array(3);
	#ndef = new Uint8Array(NDEF_MAX);
	#fifoView = [];

	constructor(options) {
		if ("target" in options)
			this.target = options.target;

		const sensor = options.sensor;
		if (!sensor?.io)
			throw new Error("sensor required");

		try {
			if ("reset" in options) {
				const reset = options.reset;
				this.#resetPin = new reset.io({
					...reset,
					mode: reset.io.Output
				});
				this.#resetPin.write(0);
				Timer.delay(2);
				this.#resetPin.write(1);
				Timer.delay(50);
			}

			this.#io = new sensor.io({
				hz: 100_000,
				address: DEFAULT_ADDRESS,
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
			if (options.antenna)
				this.#antennaOn();
			else
				this.#antennaOff();
		}
	}

	get configuration() {
		return {
			antenna: this.#antenna,
		};
	}

	sample() {
		try {
			const card = this.#activate();
			if (card) {
				const changed = !this.#present || !uidEqual(this.#lastUid, card.uid);
				this.#present = true;
				this.#lastUid = card.uid;
				this.#haltA();
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
		return {
			...identification,
			version: this.#read(Register.Version)
		};
	}

	close() {
		try {
			this.#stopCrypto1();
		}
		catch {
			/* this space intentionally left blank */
		}
		try {
			this.#antennaOff();
		}
		catch {
			/* this space intentionally left blank */
		}
		this.#io?.close();
		this.#io = undefined;
		this.#resetPin?.close();
		this.#resetPin = undefined;
	}

	start(options = {}) {
		const block = options.block;
		const key = options.key ?? DEFAULT_KEY;
		if (key.length !== MF_KEY_SIZE)
			throw new Error("key must be 6 bytes");
		const auth = (options.keyType === "B") ? PICC.MF_AUTH_KEY_B : PICC.MF_AUTH_KEY_A;
		const tries = (typeof options.retries === "number") ? options.retries : 3;

		let lastStatus = Status.ERROR;
		for (let n = 0; n < tries; n++) {
			this.#stopCrypto1();
			const card = this.#activate();
			if (!card) {
				lastStatus = Status.TIMEOUT;
				if (n + 1 < tries)
					Timer.delay(15);
				continue;
			}

			const uid = card.uid;
			const r = this.#authenticate(auth, block, key, uid);
			if (r.status === Status.OK) {
				this.#crypto = true;
				this.#lastUid = uid;
				return { uid, sak: card.sak, type: typeName(card.sak) };
			}
			lastStatus = r.status;
			if (n + 1 < tries)
				Timer.delay(15);
		}
		if (lastStatus === Status.TIMEOUT)
			throw new Error("no card");
		throw new Error(`start failed (${lastStatus})`);
	}

	readBlock(block) {
		return this.#read16(block);
	}

	writeBlock(block, data) {
		if (data.length !== MF_BLOCK_SIZE)
			throw new Error("block must be 16 bytes");

		// Step 1: write command + block
		const cmd = this.#cmd;
		cmd[0] = PICC.MF_WRITE;
		cmd[1] = block;
		let st = this.#mifareTransceive(cmd, 2);
		if (st !== Status.OK)
			throw new Error(`writeBlock cmd failed (${st})`);

		// Step 2: 16 data bytes
		st = this.#mifareTransceive(data, MF_BLOCK_SIZE);
		if (st !== Status.OK)
			throw new Error(`writeBlock data failed (${st})`);
	}

	stop() {
		this.#stopCrypto1();
	}

	readNDEF() {
		const card = this.#activate();
		if (!card)
			throw new Error("no card");
		try {
			return this.#tryNDEF(card);
		}
		finally {
			this.#haltA();
			this.#stopCrypto1();
		}
	}

	writeNDEF(options = {}) {
		const tlv = encodeNdefOptions(options);
		const card = this.#activate();
		if (!card)
			throw new Error("no card");
		try {
			if (card.sak === 0x00)
				this.#writeNDEFType2(tlv);
			else if ((card.sak === 0x08) || (card.sak === 0x18) || (card.sak === 0x09))
				this.#writeNDEFClassic(card, tlv);
			else
				throw new Error("NDEF write requires Type 2 or Classic tag");
			return this.#tryNDEF(card);
		}
		finally {
			this.#haltA();
			this.#stopCrypto1();
		}
	}

	// ------------------------------------------------------------------
	#stopCrypto1() {
		if (this.#io)
			this.#clearBit(Register.Status2, 0x08);
		this.#crypto = false;
	}

	/** PICC ACTIVE without Halt (for crypto / block IO). */
	#activate() {
		this.#stopCrypto1();
		const atqa = this.#back;
		let status = this.#reqAorWupa(PICC.WUPA, atqa, 2);
		if ((status !== Status.OK) && (status !== Status.COLLISION)) {
			status = this.#reqAorWupa(PICC.REQA, atqa, 2);
			if ((status !== Status.OK) && (status !== Status.COLLISION))
				return;
		}
		const uid = { size: 0, bytes: this.#uidBytes, sak: 0 };
		status = this.#select(uid);
		if (status !== Status.OK)
			return;
		const out = new Uint8Array(uid.size);
		out.set(uid.bytes.subarray(0, uid.size));
		return { uid: out, sak: uid.sak };
	}

	/** Transceive with CRC_A appended; expect 4-bit MF_ACK (0xA). */
	#mifareTransceive(data, length) {
		const buf = this.#cmd;
		if (data.subarray)
			buf.set(data.subarray(0, length));
		else {
			for (let i = 0; i < length; i++)
				buf[i] = data[i];
		}
		const crc = this.#crc;
		if (this.#calcCRC(buf, length, crc) !== Status.OK)
			return Status.ERROR;
		buf[length] = crc[0];
		buf[length + 1] = crc[1];
		const back = this.#back;
		const r = this.#transceive(buf, length + 2, back, 1, 0, 0, false);
		if (r.status !== Status.OK)
			return r.status;
		if ((r.backLen !== 1) || (r.validBits !== 4))
			return Status.ERROR;
		if (back[0] !== MF_ACK)
			return Status.MIFARE_NACK;
		return Status.OK;
	}

	#tryNDEF(card) {
		const sak = card.sak;
		if (sak === 0x00)
			return this.#ndefType2();
		if ((sak === 0x08) || (sak === 0x18) || (sak === 0x09))
			return this.#ndefClassic(card);
		try {
			const t2 = this.#ndefType2();
			if (t2)
				return t2;
		}
		catch {
			/* this space intentionally left blank */
		}
		return this.#ndefClassic(card);
	}

	#read16(addr) {
		const buffer = this.#cmd;
		buffer[0] = PICC.MF_READ;
		buffer[1] = addr;
		const crc = this.#crc;
		if (this.#calcCRC(buffer, 2, crc) !== Status.OK)
			throw new Error("CRC failed");
		buffer[2] = crc[0];
		buffer[3] = crc[1];
		const r = this.#transceive(buffer, 4, buffer, 18, 0, 0, true);
		if (r.status !== Status.OK)
			throw new Error("read failed");
		if (r.backLen < 16)
			throw new Error("short read");
		return buffer.slice(0, MF_BLOCK_SIZE);
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
			buf.set(chunk.subarray(0, n), filled);
			filled += n;
			page += 4;
			const msg = extractNdefTlv(buf, filled);
			if (msg === undefined)
				continue;
			return wrapNdef(msg);
		}
		return wrapNdef(extractNdefTlv(buf, filled));
	}

	// AUTH + block + key(6) + UID(last 4)
	#authenticate(auth, block, key, uid) {
		const send = this.#cmd;
		send[0] = auth;
		send[1] = block;
		send.set(key, 2);
		send.set(uid.subarray(-4), 8);
		return this.#communicate(PCD.MFAuthent, 0x10, send, 12, null, 0, 0);
	}

	#authSelected(uid, block, key) {
		this.#stopCrypto1();
		const r = this.#authenticate(PICC.MF_AUTH_KEY_A, block, key, uid);
		if (r.status === Status.OK) {
			this.#crypto = true;
			return true;
		}
		return false;
	}

	#ndefClassic(card) {
		if (!this.#authWithKeys(card, 4))
			return;
		const buf = this.#ndef;
		for (const block of [4, 5, 6])
			buf.set(this.#read16(block), (block - 4) * MF_BLOCK_SIZE);
		return wrapNdef(extractNdefTlv(buf, 48));
	}

	/** Failed MFAuthent leaves the PICC unselected; re-activate before the next key. */
	#authWithKeys(card, block) {
		if (this.#authSelected(card.uid, block, NDEF_KEY))
			return true;
		const again = this.#activate();
		if (!again)
			return false;
		card.uid = again.uid;
		card.sak = again.sak;
		return this.#authSelected(card.uid, block, DEFAULT_KEY);
	}

	#writeNDEFType2(tlv) {
		const cc = this.#read16(3);
		if (cc[0] !== 0xE1)
			throw new Error("not NDEF formatted");
		const capacity = cc[2] * 8;
		if (!capacity || (tlv.length > capacity))
			throw new Error("NDEF too large");
		let page = 4;
		for (let i = 0; i < tlv.length; i += 4) {
			const cmd = this.#cmd;
			cmd[0] = PICC.MF_UL_WRITE;
			cmd[1] = page++;
			cmd[2] = tlv[i] ?? 0;
			cmd[3] = tlv[i + 1] ?? 0;
			cmd[4] = tlv[i + 2] ?? 0;
			cmd[5] = tlv[i + 3] ?? 0;
			const st = this.#mifareTransceive(cmd, 6);
			if (st !== Status.OK)
				throw new Error(`writePage failed (${st})`);
			Timer.delay(5);
		}
	}

	#writeNDEFClassic(card, tlv) {
		if (tlv.length > 48)
			throw new Error("NDEF too large");
		if (!this.#authWithKeys(card, 4))
			throw new Error("start failed");
		const padded = this.#ndef;
		padded.set(tlv);
		padded.fill(0, tlv.length, 48);
		this.writeBlock(4, padded.subarray(0, 16));
		this.writeBlock(5, padded.subarray(16, 32));
		this.writeBlock(6, padded.subarray(32, 48));
	}

	#init() {
		this.#reset();
		this.#write(Register.TMode, 0x80);
		this.#write(Register.TPrescaler, 0xA9);
		this.#write(Register.TReloadH, 0x03);
		this.#write(Register.TReloadL, 0xE8);
		this.#write(Register.TxASK, 0x40);
		this.#write(Register.Mode, 0x3D);
		this.#antennaOn();
	}

	#reset() {
		this.#write(Register.Command, PCD.SoftReset);
		Timer.delay(50);
		for (let i = 0; i < 20; i++) {
			if (!(this.#read(Register.Command) & 0x10))
				break;
			Timer.delay(5);
		}
	}

	#antennaOn() {
		const v = this.#read(Register.TxControl);
		if ((v & 0x03) !== 0x03)
			this.#write(Register.TxControl, v | 0x03);
	}

	#antennaOff() {
		const v = this.#read(Register.TxControl);
		this.#write(Register.TxControl, v & ~0x03);
	}

	#read(reg) {
		return this.#io.readUint8(reg);
	}

	#write(reg, value) {
		this.#io.writeUint8(reg, value);
	}

	#setBit(reg, mask) {
		this.#write(reg, this.#read(reg) | mask);
	}

	#clearBit(reg, mask) {
		this.#write(reg, this.#read(reg) & ~mask);
	}

	#writeFIFO(data, length) {
		const buf = this.#fifoWrite;
		if (data.subarray)
			buf.set(data.subarray(0, length));
		else {
			for (let i = 0; i < length; i++)
				buf[i] = data[i];
		}
		this.#io.writeBuffer(Register.FIFOData, this.#view(buf, this.#fifoView, length));
	}

	#readFIFO(count, dest, rxAlign = 0) {
		if (count <= 0)
			return;
		if (!rxAlign && (dest.length === count)) {
			this.#io.readBuffer(Register.FIFOData, dest);
			return;
		}
		const tmp = this.#view(this.#fifoWrite, this.#fifoView, count);
		this.#io.readBuffer(Register.FIFOData, tmp);
		if (rxAlign) {
			const mask = (0xFF << rxAlign) & 0xFF;
			dest[0] = (dest[0] & ~mask) | (tmp[0] & mask);
			if (count > 1)
				dest.set(tmp.subarray(1), 1);
		}
		else {
			dest.set(tmp);
		}
	}

	#view(buf, cache, n) {
		let v = cache[n];
		if (!v) {
			v = new Uint8Array(buf.buffer, 0, n);
			cache[n] = v;
		}
		return v;
	}

	#calcCRC(data, length, result) {
		this.#write(Register.Command, PCD.Idle);
		this.#write(Register.DivIrq, 0x04);
		this.#setBit(Register.FIFOLevel, 0x80);
		this.#writeFIFO(data, length);
		this.#write(Register.Command, PCD.CalcCRC);

		for (let i = 0; i < 5000; i++) {
			if (this.#read(Register.DivIrq) & 0x04)
				break;
			if (i === 4999)
				return Status.TIMEOUT;
		}
		this.#write(Register.Command, PCD.Idle);
		result[0] = this.#read(Register.CRCResultL);
		result[1] = this.#read(Register.CRCResultH);
		return Status.OK;
	}

	#communicate(command, waitIRq, sendData, sendLen, backData, maxBack, validBitsIn, rxAlign = 0, checkCRC = false) {
		const txLastBits = validBitsIn ?? 0;
		const bitFraming = (rxAlign << 4) + txLastBits;

		this.#write(Register.Command, PCD.Idle);
		this.#write(Register.ComIrq, 0x7F);
		this.#setBit(Register.FIFOLevel, 0x80);
		if (sendLen)
			this.#writeFIFO(sendData, sendLen);
		this.#write(Register.BitFraming, bitFraming);
		this.#write(Register.Command, command);
		if (command === PCD.Transceive)
			this.#setBit(Register.BitFraming, 0x80);

		let n;
		for (let i = 0; i < 2000; i++) {
			n = this.#read(Register.ComIrq);
			if (n & waitIRq)
				break;
			if (n & 0x01)
				return { status: Status.TIMEOUT };
			if (i === 1999)
				return { status: Status.TIMEOUT };
		}

		const errorRegValue = this.#read(Register.Error);
		if (errorRegValue & 0x13)
			return { status: Status.ERROR };

		let backLen = 0;
		let validBits = 0;
		if (backData && (maxBack > 0)) {
			n = this.#read(Register.FIFOLevel);
			if (n > maxBack)
				return { status: Status.NO_ROOM };
			backLen = n;
			this.#readFIFO(n, backData, rxAlign);
			validBits = this.#read(Register.Control) & 0x07;
		}

		if (errorRegValue & 0x08)
			return { status: Status.COLLISION, backLen, validBits };

		if (backData && backLen && checkCRC) {
			if ((backLen === 1) && (validBits === 4))
				return { status: Status.ERROR };
			if ((backLen < 2) || (validBits !== 0))
				return { status: Status.CRC_WRONG };
			const control = this.#crc;
			const st = this.#calcCRC(backData, backLen - 2, control);
			if (st !== Status.OK)
				return { status: st };
			if ((backData[backLen - 2] !== control[0]) || (backData[backLen - 1] !== control[1]))
				return { status: Status.CRC_WRONG };
		}

		return { status: Status.OK, backLen, validBits };
	}

	#transceive(sendData, sendLen, backData, maxBack, validBits, rxAlign = 0, checkCRC = false) {
		return this.#communicate(PCD.Transceive, 0x30, sendData, sendLen, backData, maxBack, validBits, rxAlign, checkCRC);
	}

	#reqAorWupa(command, bufferATQA, bufferSize) {
		if (bufferSize < 2)
			return Status.NO_ROOM;
		this.#clearBit(Register.Coll, 0x80);
		this.#cmd[0] = command;
		const r = this.#transceive(this.#cmd, 1, bufferATQA, bufferSize, 7);
		if (r.status !== Status.OK)
			return r.status;
		if ((r.backLen !== 2) || (r.validBits !== 0))
			return Status.ERROR;
		return Status.OK;
	}

	/**
	 * Cascade anticollision/select for a single PICC in the field.
	 * Handles 4- and 7-byte UIDs (common Type A cards).
	 */
	#select(uid) {
		const crc = this.#crc;
		const buffer = this.#cmd;
		const response = this.#back;
		const sakBuf = this.#sakBuf;
		const levels = [
			{ sel: PICC.SEL_CL1, index: 0 },
			{ sel: PICC.SEL_CL2, index: 3 },
		];

		this.#clearBit(Register.Coll, 0x80);

		for (let level = 0; level < levels.length; level++) {
			const { sel, index: uidIndex } = levels[level];

			// Anticollision: SEL + NVB(0x20) -> up to 5 bytes (UID + BCC)
			buffer[0] = sel;
			buffer[1] = 0x20;
			this.#write(Register.BitFraming, 0x00);
			let r = this.#transceive(buffer, 2, response, 5, 0, 0);
			if (r.status !== Status.OK)
				return r.status;
			if (r.backLen < 5)
				return Status.ERROR;

			// SELECT: SEL + NVB(0x70) + 4 data + BCC + CRC_A
			buffer[0] = sel;
			buffer[1] = 0x70;
			buffer.set(response.subarray(0, 5), 2);
			if (this.#calcCRC(buffer, 7, crc) !== Status.OK)
				return Status.ERROR;
			buffer[7] = crc[0];
			buffer[8] = crc[1];

			r = this.#transceive(buffer, 9, sakBuf, 3, 0, 0);
			if (r.status !== Status.OK)
				return r.status;
			if ((r.backLen !== 3) || (r.validBits !== 0))
				return Status.ERROR;
			if (this.#calcCRC(sakBuf, 1, crc) !== Status.OK)
				return Status.ERROR;
			if ((crc[0] !== sakBuf[1]) || (crc[1] !== sakBuf[2]))
				return Status.CRC_WRONG;

			const hasCascadeTag = response[0] === PICC.CT;
			if (hasCascadeTag)
				uid.bytes.set(response.subarray(1, 4), uidIndex);
			else
				uid.bytes.set(response.subarray(0, 4), uidIndex);

			uid.sak = sakBuf[0];
			if (!(sakBuf[0] & 0x04)) {
				if ((level === 0) && !hasCascadeTag)
					uid.size = 4;
				else if (level === 1)
					uid.size = 7;
				else
					uid.size = hasCascadeTag ? (uidIndex + 3) : (uidIndex + 4);
				return Status.OK;
			}
			if ((level === 0) && hasCascadeTag)
				continue;
			return Status.ERROR;
		}
		return Status.ERROR;
	}

	#haltA() {
		const buffer = this.#cmd;
		buffer[0] = PICC.HLTA;
		buffer[1] = 0;
		const crc = this.#crc;
		if (this.#calcCRC(buffer, 2, crc) !== Status.OK)
			return Status.ERROR;
		buffer[2] = crc[0];
		buffer[3] = crc[1];
		const r = this.#transceive(buffer, 4, null, 0, 0);
		// Timeout is success for HLTA
		if (r.status === Status.TIMEOUT)
			return Status.OK;
		if (r.status === Status.OK)
			return Status.ERROR;
		return r.status;
	}

	static {
		this.prototype[Symbol.dispose] = this.prototype.close;
	}
}
