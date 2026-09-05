/*
 * Copyright (c) 2026 Joshua Jun
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

#include "xsmc.h"
#include "xsHost.h"
#include "mc.xs.h"			// for xsID_ values

// Geometry must match gdey037t03.js: logical 416 x 240, native 240 x 416.
#define STRIDE 52			// logical row bytes (416 >> 3)
#define NATIVE_STRIDE 30	// native row bytes (240 >> 3)
#define NATIVE_H 416

// transpose(logicalBuffer, outBuffer): native(nx, ny) = logical(ny, nx).
// Each native output byte packs 8 logical columns at one native row.
void xs_gdey037t03_transpose(xsMachine *the)
{
	uint8_t *logical, *out;
	xsUnsignedValue logicalLen, outLen;

	xsmcGetBufferReadable(xsArg(0), (void **)&logical, &logicalLen);
	if (logicalLen < STRIDE * 240)
		xsUnknownError("buffer too small");

// 	xsmcSetInteger(xsResult, NATIVE_STRIDE * NATIVE_H);
// 	xsResult = xsNew1(xsGlobal, xsID_Uint8Array, xsResult);
// 	xsmcGetBufferWritable(xsResult, (void **)&out, &outLen);
	
	xsResult = xsArg(1);
	xsmcGetBufferWritable(xsArg(1), (void **)&out, &outLen);

	for (int ny = 0; ny < NATIVE_H; ny++) {
		int lShift = 7 - (ny & 7);
		int mask = 1 << lShift;
		const uint8_t *src = logical + (ny >> 3);
		uint8_t *orow = out + ny * NATIVE_STRIDE;
		for (int nb = 0; nb < NATIVE_STRIDE; nb++) {
			uint8_t byte = 0;
			if (src[0]          & mask) byte |= 0x80;
			if (src[STRIDE]     & mask) byte |= 0x40;
			if (src[STRIDE * 2] & mask) byte |= 0x20;
			if (src[STRIDE * 3] & mask) byte |= 0x10;
			if (src[STRIDE * 4] & mask) byte |= 0x08;
			if (src[STRIDE * 5] & mask) byte |= 0x04;
			if (src[STRIDE * 6] & mask) byte |= 0x02;
			if (src[STRIDE * 7] & mask) byte |= 0x01;
			orow[nb] = byte;
			src += STRIDE << 3;	// advance nx by 8
		}
	}
}
