/*
 * Copyright (c) 2020-2026  Moddable Tech, Inc.
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
#include "commodettoBitmap.h"

#define kInvalidGlyphID (0xFFFF)

struct CommodettoFontEngineRecord;
typedef struct CommodettoFontEngineRecord *CommodettoFontEngine;

#ifndef __commodettofontengine_internal__
struct CFEGlyphRecord {
	uint16_t				advance;
	int16_t				dx;
	int16_t				dy;

	uint16_t				w;
	uint16_t				h;
	uint8_t				format;
	void					*bits;

#if COMMODETTO_BITMAP_ID
	uint32_t				id;
#endif

	uint16_t				sx;
	uint16_t				sy;

	const char				*substitute;
};
#endif

typedef struct CFEGlyphRecord CFEGlyphRecord;
typedef struct CFEGlyphRecord *CFEGlyph;

typedef struct {
	uint16_t	glyphID;
	int16_t		advance;
} CFERunRecord, *CFERun;

CommodettoFontEngine CFENew(void);
void CFEDispose(CommodettoFontEngine cfe);

void CFESetFontData(CommodettoFontEngine cfe, const void *fontData, uint32_t fontDataSize);
void CFESetFontSize(CommodettoFontEngine cfe, int32_t size);
void CFEGetFontMetrics(CommodettoFontEngine cfe, int32_t *ascent, int32_t *descent, int32_t *leading);

void CFELockCache(CommodettoFontEngine cfe, uint8_t lock);

CFEGlyph CFEGetGlyphFromGlyphID(CommodettoFontEngine cfe, uint16_t glyphID, uint8_t needPixels);
CFEGlyph CFEGetGlyphFromUnicode(CommodettoFontEngine cfe, uint32_t unicode, uint8_t needPixels);

int16_t CFEGetKerningOffset(CommodettoFontEngine cfe, uint32_t unicode1, uint32_t unicode2);

void CFELayoutRun(CommodettoFontEngine cfe, const char *utf8, int32_t byteLength, CFERun run, int32_t runLength, int32_t width);
int CFEShape(CommodettoFontEngine cfe, const char *utf8In, int32_t byteLengthIn, char *utf8Out, int32_t byteLengthOut);

/*
	The outline of one glyph at the current size, as CFEGetOutlineFromUnicode
	returns it: a CFEOutlineRecord, then pointCount pairs of int32_t, then
	contourCount uint16_t, then pointCount uint8_t.

	Coordinates are 26.6 fixed point about the origin on the baseline, y up.
	Each contour holds the index of its last point. A tag is 0 for a quadratic
	control point, 1 for a point on the curve, and 2 for a cubic control point.
	The caller frees the buffer.

	The advance is 26.6 too, and unrounded: an outline is usually scaled before
	it is drawn, so rounding each glyph to whole pixels here would both drift
	along a run and be magnified by the scale. A glyph with nothing to draw,
	a space, still has an advance and so still gets a record.
*/
typedef struct {
	uint16_t	pointCount;
	uint16_t	contourCount;
	int32_t		advance;
} CFEOutlineRecord, *CFEOutline;

#define CFEOutlineByteLength(pointCount, contourCount) \
	(sizeof(CFEOutlineRecord) + ((pointCount) * 2 * sizeof(int32_t)) + ((contourCount) * sizeof(uint16_t)) + (pointCount))

void CFEGetOutlineFromUnicode(CommodettoFontEngine cfe, uint32_t unicode, uint8_t **outline, uint32_t *outlineSize);
void CFERenderOutline(CommodettoFontEngine cfe, uint8_t *outline, uint32_t outlineSize, double scaleX, double scaleY, CFEGlyph glyph);
