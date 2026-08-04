---
name: Calculate CRC for Binary Data
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

The `CRC` module calculates the [Cyclic Redundancy Check](https://en.wikipedia.org/wiki/Cyclic_redundancy_check) (CRC) for buffers of binary data.

---

Check the table below to find the constructor and parameters to use for the CRC you need to calculate. To calculate the CRC for a buffer using `CRC-16/MODBUS`, use the `CRC16` constructor.

```js
const crc = new CRC16(0x8005, 0xFFFF, true, true, 0x0000);

const data = Uint8Array.of(0, 1, 2, 3, 4, 5, 6, 7);

const crcValue = crc.checksum(data);
// => crcValue is checksum value

crc.close();
```

---

The CRC may be calculated across multiple buffers.

```js
const crc = new CRC16(0x8005, 0xFFFF, true, true, 0x0000);

const data1 = Uint8Array.of(0, 1, 2, 3, 4, 5, 6, 7);
const data2 = Uint8Array.of(7, 6, 5, 4, 3, 2, 1, 0);

crc.checksum(data1);
const crcValue = crc.checksum(data2);
// => crcValue is CRC of data1 and data2

crc.close();

```

---

A single CRC instance can calculate multiple checksums. Call `reset()` to reinitialize the internal state.

```js
const crc = new CRC16(0x8005, 0xFFFF, true, true, 0x0000);

const data1 = Uint8Array.of(0, 1, 2, 3, 4, 5, 6, 7);
const data2 = Uint8Array.of(7, 6, 5, 4, 3, 2, 1, 0);

const crcValue1 = crc.checksum(data1);
// => crcValue1 is CRC of data1

crc.reset();

const crcValue2 = crc.checksum(data2);
// => crcValue2 is CRC of data2


crc.close();
```
---

| Name | Constructor |
|------|--------------|
| CRC-8 | `new CRC8(0x07, 0x00, false, false, 0x00)` |
| CRC-8/CDMA2000 | `new CRC8(0x9B, 0xFF, false, false, 0x00)` |
| CRC-8/DARC | `new CRC8(0x39, 0x00, true, true, 0x00)` |
| CRC-8/DVB-S2 | `new CRC8(0xD5, 0x00, false, false, 0x00)` |
| CRC-8/EBU | `new CRC8(0x1D, 0xFF, true, true, 0x00)` |
| CRC-8/I-CODE | `new CRC8(0x1D, 0xFD, false, false, 0x00)` |
| CRC-8/ITU | `new CRC8(0x07, 0x00, false, false, 0x55)` |
| CRC-8/MAXIM | `new CRC8(0x31, 0x00, true, true, 0x00)` |
| CRC-8/ROHC | `new CRC8(0x07, 0xFF, true, true, 0x00)` |
| CRC-8/WCDMA | `new CRC8(0x9B, 0x00, true, true, 0x00)` |
| CRC-16/CCITT-FALSE | `new CRC16(0x1021, 0xFFFF, false, false, 0x0000)` |
| CRC-16/ARC | `new CRC16(0x8005, 0x0000, true, true, 0x0000)` |
| CRC-16/ARG-CCITT | `new CRC16(0x1021, 0x1D0F, false, false, 0x0000)` |
| CRC-16/BUYPASS | `new CRC16(0x8005, 0x0000, false, false, 0x0000)` |
| CRC-16/CDMA2000 | `new CRC16(0xC867, 0xFFFF, false, false, 0x0000)` |
| CRC-16/DDS-110 | `new CRC16(0x8005, 0x800D, false, false, 0x0000)` |
| CRC-16/DECT-R | `new CRC16(0x0589, 0x0000, false, false, 0x0001)` |
| CRC-16/DECT-X | `new CRC16(0x0589, 0x0000, false, false, 0x0000)` |
| CRC-16/DNP | `new CRC16(0x3D65, 0x0000, true, true, 0xFFFF)` |
| CRC-16/EN-13757 | `new CRC16(0x3D65, 0x0000, false, false, 0xFFFF)` |
| CRC-16/GENIBUS | `new CRC16(0x1021, 0xFFFF, false, false, 0xFFFF)` |
| CRC-16/MAXIM | `new CRC16(0x8005, 0x0000, true, true, 0xFFFF)` |
| CRC-16/MCRF4XX | `new CRC16(0x1021, 0xFFFF, true, true, 0x0000)` |
| CRC-16/RIELLO | `new CRC16(0x1021, 0xB2AA, true, true, 0x0000)` |
| CRC-16/T10-DIF | `new CRC16(0x8BB7, 0x0000, false, false, 0x0000)` |
| CRC-16/TELEDISK | `new CRC16(0xA097, 0x0000, false, false, 0x0000)` |
| CRC-16/TMS37157 | `new CRC16(0x1021, 0x89EC, true, true, 0x0000)` |
| CRC-16/USB | `new CRC16(0x8005, 0xFFFF, true, true, 0xFFFF)` |
| CRC-A | `new CRC16(0x1021, 0xC6C6, true, true, 0x0000)` |
| CRC-16/KERMIT | `new CRC16(0x1021, 0x0000, true, true, 0x0000)` |
| CRC-16/MODBUS | `new CRC16(0x8005, 0xFFFF, true, true, 0x0000)` |
| CRC-16/X-25 | `new CRC16(0x1021, 0xFFFF, true, true, 0xFFFF)` |
| CRC-16/XMODEM | `new CRC16(0x1021, 0x0000, false, false, 0x0000)` |
