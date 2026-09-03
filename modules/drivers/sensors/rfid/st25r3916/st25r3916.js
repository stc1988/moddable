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
	ST25R3916 / ST25R3917 NFC frontend over I2C (ECMA-419 sensor).

	Module: embedded:sensor/RFID/ST25R3916

	Register/command I2C (Space A/B), default 7-bit address 0x50.
	Use device.io.SMBus. No IRQ pin required - interrupt status is polled.

	ISO14443A UID poll + Type 2 NDEF (NTAG / Ultralight).
	MIFARE Classic Crypto1 is software on this chip; not implemented.

	I2C framing matches M5Unit-NFC and pguyot/st25r391x:
	  write  00xxxxxx, read 01xxxxxx, FIFO load 0x80, FIFO read 0x9F,
	  direct command 11xxxxxx (sendByte), Space B 0xFB, test 0xFC.

	Constructor options: sensor, reset, target.
	configure({ antenna }) turns the RF field on or off.
	Call sample() from the application (no card-present interrupt).
*/

import Timer from "timer";
import { extractNdefTlv, wrapNdef, encodeNdefOptions, NDEF_MAX } from "embedded:sensor/RFID/ndef";

const Register = Object.freeze({
	IO1: 0x00,
	IO2: 0x01,
	OpControl: 0x02,
	Mode: 0x03,
	BitRate: 0x04,
	ISO14443A: 0x05,
	Aux: 0x0A,
	RxCfg1: 0x0B,
	MaskMainIrq: 0x16,
	MaskTimerIrq: 0x17,
	MainIrq: 0x1A,
	TimerIrq: 0x1B,
	ErrorIrq: 0x1C,
	PtIrq: 0x1D,
	FifoStatus1: 0x1E,
	FifoStatus2: 0x1F,
	Collision: 0x20,
	TxBytes1: 0x22,
	TxBytes2: 0x23,
	TxDriver: 0x28,
	AuxDisplay: 0x31,
	Identity: 0x3F,
});

const Command = Object.freeze({
	SetDefault: 0xC0,
	StopAll: 0xC2,
	TxWithCRC: 0xC4,
	TxWithoutCRC: 0xC5,
	TxREQA: 0xC6,
	TxWUPA: 0xC7,
	FieldOn: 0xC8,
	ResetRxGain: 0xD5,
	AdjustRegulators: 0xD6,
	ClearFIFO: 0xDB,
	SpaceB: 0xFB,
	TestAccess: 0xFC,
});

const SPACE_B_NFCA = Uint8Array.of(0x0C, 0x51, 0x00);
const TEST_UNLOCK = Uint8Array.of(0x04, 0x10);

const Mode = Object.freeze({
	Write: 0x00,
	Read: 0x40,
	FifoLoad: 0x80,
	FifoRead: 0x9F,
});

const Op = Object.freeze({
	en: 0x80,
	rx_en: 0x40,
	tx_en: 0x08,
	en_fd: 0x03,
});

const Irq = Object.freeze({
	osc: 0x80,
	rxs: 0x20,
	rxe: 0x10,
	txe: 0x08,
	col: 0x04,
	dct: 0x80,
});

const PICC = Object.freeze({
	CT: 0x88,
	HLTA: 0x50,
	MF_READ: 0x30,
	MF_UL_WRITE: 0xA2,
});

const ISOA = Object.freeze({
	antcl: 0x01,
	no_crc_rx: 0x80,
});

const IC_TYPE_3916 = 0x05;
const DEFAULT_ADDRESS = 0x50;
const MF_ACK = 0x0A;
const MF_BLOCK_SIZE = 16;

const identification = Object.freeze({
	model: "ST25R3916",
	classification: "RFID",
	description: "13.56 MHz ISO14443A NFC/RFID frontend (I2C)",
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

export default class ST25R3916 {
	#io;
	#resetPin;
	#antenna = true;
	#lastUid;
	#present = false;
	#irqBuf = new Uint8Array(4);
	#statusBuf = new Uint8Array(2);
	#fifo = new Uint8Array(64);
	#tx = new Uint8Array(12);
	#load = new Uint8Array(12);
	#ac = new Uint8Array(5);
	#uid = new Uint8Array(10);
	#block = new Uint8Array(MF_BLOCK_SIZE);
	#ndef = new Uint8Array(NDEF_MAX);
	#fifoView = [];
	#loadView = [];
	#fifoLen = 0;
	#misses = 0;
	#hz = 400_000;
	#lastMain = 0;

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

			this.#hz = sensor.hz ?? 400_000;
			this.#io = new sensor.io({
				hz: 400_000,
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
				this.#readyField();
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
				this.#misses = 0;
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
			else {
				this.#misses++;
				if (this.#present) {
					this.#present = false;
					this.#lastUid = undefined;
					return { present: false };
				}
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
		return this.#read(Register.Identity);
	}

	close() {
		try {
			this.#antennaOff();
		}
		catch {
		}
		this.#io?.close();
		this.#io = undefined;
		this.#resetPin?.close();
		this.#resetPin = undefined;
	}

	start() {
		throw new Error("MIFARE Classic crypto is not implemented");
	}

	readBlock(block) {
		const card = this.#activate();
		if (!card)
			throw new Error("no card");
		try {
			return this.#read16(block).slice();
		}
		finally {
			this.#haltA();
		}
	}

	writeBlock(block, data) {
		const card = this.#activate();
		if (!card)
			throw new Error("no card");
		if (card.sak !== 0x00)
			throw new Error("writeBlock requires Type 2 tag");
		try {
			for (let i = 0; i < 4; i++) {
				this.#writePage(block + i, data, i * 4);
				Timer.delay(5);
			}
		}
		finally {
			this.#haltA();
		}
	}

	stop() {
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
		}
	}

	writeNDEF(options = {}) {
		const tlv = encodeNdefOptions(options);
		const card = this.#activate();
		if (!card)
			throw new Error("no card");
		try {
			if (card.sak !== 0x00)
				throw new Error("NDEF write requires Type 2 tag");
			this.#writeNDEFType2(tlv);
			return this.#tryNDEF(card);
		}
		finally {
			this.#haltA();
		}
	}

	// ------------------------------------------------------------------
	#activate() {
		if (this.#misses >= 8) {
			this.#readyField();
			this.#misses = 0;
		}
		if (!this.#reqAorWupa(true) && !this.#reqAorWupa(false))
			return;
		const card = this.#select();
		if (card)
			return card;
		if (this.#reqAorWupa(true) || this.#reqAorWupa(false))
			return this.#select();
	}

	#tryNDEF(card) {
		if (card.sak === 0x00)
			return this.#ndefType2();
	}

	#read16(addr) {
		this.#tx[0] = PICC.MF_READ;
		this.#tx[1] = addr;
		if (!this.#transceive(this.#tx, 2, 0, true) || (this.#fifoLen < MF_BLOCK_SIZE))
			throw new Error("read failed");
		const out = this.#block;
		for (let i = 0; i < MF_BLOCK_SIZE; i++)
			out[i] = this.#fifo[i];
		return out;
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

	#writeNDEFType2(tlv) {
		const cc = this.#read16(3);
		if (cc[0] !== 0xE1)
			throw new Error("not NDEF formatted");
		const capacity = cc[2] * 8;
		if (!capacity || (tlv.length > capacity))
			throw new Error("NDEF too large");
		let page = 4;
		for (let i = 0; i < tlv.length; i += 4) {
			this.#writePage(page++, tlv, i);
			Timer.delay(5);
		}
	}

	#writePage(page, src, offset) {
		const cmd = this.#tx;
		cmd[0] = PICC.MF_UL_WRITE;
		cmd[1] = page;
		cmd[2] = src[offset] ?? 0;
		cmd[3] = src[offset + 1] ?? 0;
		cmd[4] = src[offset + 2] ?? 0;
		cmd[5] = src[offset + 3] ?? 0;
		if (!this.#transceive(cmd, 6, 0, true) || ((this.#fifo[0] & 0x0F) !== MF_ACK))
			throw new Error("writePage failed");
	}

	#init() {
		this.#command(Command.StopAll);
		Timer.delay(2);
		this.#command(Command.SetDefault);
		Timer.delay(10);

		// Datasheet / M5 / Linux: unlock overheat protection after Set default.
		this.#io.writeBuffer(Command.TestAccess, TEST_UNLOCK);

		const io1 = (this.#hz >= 400_000) ? 0x10 : 0x00;	// i2c_thd0 at 400 kHz
		const io2 = 0x80 | 0x20 | 0x04;						// sup3v | aat_en | io_drv_lvl
		this.#write(Register.IO1, io1);
		this.#write(Register.IO2, io2);

		let id = 0;
		for (let n = 0; n < 5; n++) {
			id = this.#read(Register.Identity);
			if (((id >> 3) & 0x1F) === IC_TYPE_3916)
				break;
			Timer.delay(20);
			id = 0;
		}
		if (((id >> 3) & 0x1F) !== IC_TYPE_3916)
			throw new Error("ST25R3916 not found");

		this.#write(Register.MaskMainIrq, 0);
		this.#write(Register.MaskTimerIrq, 0);
		this.#clearIrq();
		this.#oscOn();
		this.#readyField();
	}

	#oscOn() {
		this.#write(Register.OpControl, Op.en | Op.en_fd);
		if (!this.#waitMain(Irq.osc, 50))
			throw new Error("oscillator");
		if (!(this.#read(Register.AuxDisplay) & 0x10))
			throw new Error("oscillator");
	}

	#iso14443a() {
		this.#write(Register.OpControl, this.#read(Register.OpControl) & ~0x04);	// wu off
		this.#write(Register.Mode, 0x08);		// ISO14443A initiator
		this.#write(Register.BitRate, 0);
		this.#write(Register.TxDriver, 0x70);	// AM 12%
		this.#write(Register.ISO14443A, 0);
		this.#write(Register.RxCfg1, 0x08);
		this.#write(Register.RxCfg1 + 1, 0x2D);
		this.#write(Register.RxCfg1 + 2, 0);
		this.#write(Register.RxCfg1 + 3, 0);
		// Space B correlator (Linux nfca)
		this.#io.writeBuffer(Command.SpaceB, SPACE_B_NFCA);
	}

	#readyField() {
		this.#command(Command.AdjustRegulators);
		this.#waitTimer(Irq.dct, 20);
		this.#command(Command.StopAll);
		this.#command(Command.ResetRxGain);
		this.#iso14443a();
		this.#antennaOn();
	}

	#antennaOn() {
		this.#command(Command.FieldOn);
		Timer.delay(5);
		const v = this.#read(Register.OpControl);
		this.#write(Register.OpControl, v | Op.tx_en | Op.rx_en);
	}

	#antennaOff() {
		if (!this.#io)
			return;
		this.#command(Command.StopAll);
		const v = this.#read(Register.OpControl);
		this.#write(Register.OpControl, v & ~(Op.tx_en | Op.rx_en));
	}

	#reqAorWupa(wupa) {
		this.#write(Register.ISO14443A, 0);
		this.#write(Register.Aux, this.#read(Register.Aux) | ISOA.no_crc_rx);
		this.#command(Command.ClearFIFO);
		this.#clearIrq();
		this.#command(wupa ? Command.TxWUPA : Command.TxREQA);
		// Wait for RXE only. I_col often arrives first on weak tags; treating
		// it as failure dropped every sticker and many bank cards.
		if (!this.#waitMain(Irq.rxe, 10))
			return;
		return this.#readFIFO() >= 2;
	}

	#select() {
		const uid = this.#uid;
		let uidLen = 0;
		for (let level = 1; level <= 3; level++) {
			const sel = 0x91 + (level * 2);
			this.#write(Register.ISO14443A, ISOA.antcl);
			this.#write(Register.Aux, this.#read(Register.Aux) & ~ISOA.no_crc_rx);

			this.#tx[0] = sel;
			this.#tx[1] = 0x20;
			if (!this.#transceive(this.#tx, 2, 16, false) || (this.#fifoLen < 5))
				return;
			const ac = this.#ac;
			for (let i = 0; i < 5; i++)
				ac[i] = this.#fifo[i];
			if ((ac[0] ^ ac[1] ^ ac[2] ^ ac[3]) !== ac[4])
				return;

			this.#write(Register.ISO14443A, 0);
			this.#tx[0] = sel;
			this.#tx[1] = 0x70;
			this.#tx[2] = ac[0];
			this.#tx[3] = ac[1];
			this.#tx[4] = ac[2];
			this.#tx[5] = ac[3];
			this.#tx[6] = ac[4];
			if (!this.#transceive(this.#tx, 7, 0, true) || !this.#fifoLen)
				return;
			const sak = this.#fifo[0];

			if (ac[0] === PICC.CT) {
				uid[uidLen++] = ac[1];
				uid[uidLen++] = ac[2];
				uid[uidLen++] = ac[3];
			}
			else {
				uid[uidLen++] = ac[0];
				uid[uidLen++] = ac[1];
				uid[uidLen++] = ac[2];
				uid[uidLen++] = ac[3];
			}

			if (!(sak & 0x04)) {
				const out = new Uint8Array(uidLen);
				for (let i = 0; i < uidLen; i++)
					out[i] = uid[i];
				return { uid: out, sak };
			}
		}
	}

	#haltA() {
		try {
			this.#write(Register.ISO14443A, 0);
			this.#tx[0] = PICC.HLTA;
			this.#tx[1] = 0;
			this.#command(Command.ClearFIFO);
			this.#io.writeBuffer(Mode.FifoLoad, this.#tx.subarray(0, 2));
			this.#writeTxBits(16);
			this.#clearIrq();
			this.#command(Command.TxWithCRC);
			this.#waitMain(Irq.txe, 5);
		}
		catch {
		}
	}

	/**
	 * @param {Uint8Array} tx
	 * @param {number} txLen
	 * @param {number} bits  0 = txLen * 8
	 * @param {boolean} crc
	 * @returns {boolean}
	 */
	#transceive(tx, txLen, bits, crc) {
		const bitCount = bits || (txLen << 3);
		this.#command(Command.ClearFIFO);
		const load = this.#view(this.#load, this.#loadView, txLen);
		for (let i = 0; i < txLen; i++)
			load[i] = tx[i];
		this.#io.writeBuffer(Mode.FifoLoad, load);
		this.#writeTxBits(bitCount);
		this.#clearIrq();
		this.#command(crc ? Command.TxWithCRC : Command.TxWithoutCRC);
		if (!this.#waitMain(Irq.txe, 5))
			return false;
		// First MainIrq read often has TXE|RXE already (FDT ~91 us).
		// Reading the register clears both; do not wait again for RXE.
		if (!(this.#lastMain & Irq.rxe) && !this.#waitMain(Irq.rxe, 10))
			return false;
		return this.#readFIFO() > 0;
	}

	#writeTxBits(bits) {
		this.#write(Register.TxBytes1, bits >> 8);
		this.#write(Register.TxBytes2, bits & 0xFF);
	}

	#readFIFO() {
		const st = this.#statusBuf;
		this.#io.readBuffer(Register.FifoStatus1 | Mode.Read, st);
		const count = st[0] | ((st[1] & 0xC0) << 2);
		this.#fifoLen = 0;
		if (!count)
			return 0;
		const n = count > 64 ? 64 : count;
		this.#io.readBuffer(Mode.FifoRead, this.#view(this.#fifo, this.#fifoView, n));
		this.#fifoLen = n;
		return n;
	}

	#view(buf, cache, n) {
		let v = cache[n];
		if (!v) {
			v = new Uint8Array(buf.buffer, 0, n);
			cache[n] = v;
		}
		return v;
	}

	#read(reg) {
		return this.#io.readUint8(reg | Mode.Read);
	}

	#write(reg, value) {
		this.#io.writeUint8(reg | Mode.Write, value);
	}

	#command(cmd) {
		this.#io.sendByte(cmd);
	}

	#clearIrq() {
		this.#io.readBuffer(Register.MainIrq | Mode.Read, this.#irqBuf);
	}

	/** Poll MainIrq until mask, timer mask, or `ms` wall-clock elapses. */
	#wait(mainMask, timerMask, ms) {
		const buf = this.#irqBuf;
		const deadline = Date.now() + ms;
		do {
			this.#io.readBuffer(Register.MainIrq | Mode.Read, buf);
			this.#lastMain = buf[0];
			if (buf[0] & mainMask)
				return "main";
			if (timerMask && (buf[1] & timerMask))
				return "timer";
		} while (Date.now() < deadline);
	}

	#waitMain(mask, ms) {
		return this.#wait(mask, 0, ms) === "main";
	}

	#waitTimer(mask, ms) {
		return this.#wait(0, mask, ms) === "timer";
	}

	static {
		this.prototype[Symbol.dispose] = this.prototype.close;
	}
}
