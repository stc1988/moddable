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
	Commodetto Font Engine using FreeType

	Renders scalable fonts -- TrueType, and OpenType/CFF when enabled -- at any
	pixel size, caching rendered glyphs in the RLE encoded 4 bit gray format that
	Poco blits directly.

	Glyphs are rasterized by calling ft_grays_raster in direct mode, so spans
	arrive one row at a time and are compressed on the way into the cache. That
	avoids ever materializing an 8 bit bitmap of the glyph, which is the largest
	buffer a conventional FT_Render_Glyph path would need.
*/

#include "commodettoFontEngine.h"
#include "commodettoPocoBlit.h"
#include "xsPlatform.h"
#if !XSTOOLS
	#include "mc.defines.h"
#endif

#include <ft2build.h>
#include FT_CONFIG_CONFIG_H
#include <freetype/freetype.h>
#include <freetype/ftmodapi.h>
#include <freetype/ftoutln.h>
#include <freetype/internal/ftobjs.h>

extern const FT_Raster_Funcs ft_grays_raster;

#ifndef MODDEF_CFE_KERN
	#define MODDEF_CFE_KERN (0)
#endif
#ifndef MODDEF_CFE_FT_FACES
	#define MODDEF_CFE_FT_FACES (4)
#endif
#ifndef MODDEF_CFE_FT_CACHESIZE
	#define MODDEF_CFE_FT_CACHESIZE (8 * 1024)
#endif

/*
	"defines": {"cfe": {"ft": {"stats": 1}}} reports what the glyph cache is
	doing at the end of every frame -- enough to size the cache for an
	application and to see what compression is buying. Debug only: nothing here
	is compiled otherwise.
*/
/*
	A frame can ask for more glyphs than the cache holds -- every entry already
	drawn is pinned, so nothing can be evicted to make room. Rather than drop the
	glyph, its pixels come from the heap and are released when the frame ends.
	This is the safety valve: with a cache sized for the application it never
	happens, and text is never silently missing when it does.
*/

#ifndef MODDEF_CFE_FT_STATS
	#define MODDEF_CFE_FT_STATS (0)
#endif

#if MODDEF_CFE_FT_STATS
	#include <stdio.h>
	#ifdef modLogVar
		#define cfeReport(msg) modLogVar(msg)
	#else
		#define cfeReport(msg) fprintf(stderr, "%s", msg)
	#endif
#endif

#if MODDEF_CFE_FT_HINTINGBYTECODE
	#define kGlyphLoadFlags (FT_LOAD_NO_BITMAP | FT_LOAD_TARGET_NORMAL)
#elif MODDEF_CFE_FT_HINTINGAUTO
	#define kGlyphLoadFlags (FT_LOAD_NO_BITMAP | FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_NORMAL)
#else
	#define kGlyphLoadFlags (FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING)
#endif

/*
	A cache entry always carries metrics. It carries pixels only if something
	asked for them: measuring a string walks every glyph, and a measuring pass
	should never rasterize.
*/
#define kEntryHasBits (1 << 0)
#define kEntryLocked (1 << 1)		// used during the frame being built. never move or evict.
#define kEntryPacked (1 << 2)		// RLE, rather than 4 bits per pixel
#define kEntryOverflow (1 << 3)		// pixels are on the heap, only until this frame ends

typedef struct {
	uint16_t			glyphID;
	uint16_t			size;
	uint16_t			faceID;		// not the slot: a slot may be recycled while these pixels are still in use
	uint8_t				flags;
	uint16_t			hashNext;	// next entry in this bucket, as a block offset >> 2

	uint16_t			use;		// last touched, for eviction order

	int16_t				advance;
	int16_t				dx;
	int16_t				dy;
	uint16_t			w;
	uint16_t			h;

	uint16_t			length;		// bytes of pixels, which follow this record
} CFEEntryRecord, *CFEEntry;

// a glyph's pixels sit immediately after its record, wherever that record lives
#define entryPixels(entry) ((uint8_t *)((entry) + 1))

typedef struct {
	const void			*fontData;
	uint32_t			fontDataSize;
	FT_Face				face;
	uint16_t			id;			// bumped every time the slot is reused
	uint32_t			use;		// last selected, for eviction order
} CFEFaceRecord, *CFEFace;

struct CommodettoFontEngineRecord {
	FT_Library			library;
	struct FT_MemoryRec_ memory;
	FT_Raster			raster;

	CFEFaceRecord		faces[MODDEF_CFE_FT_FACES];
	uint8_t				faceIndex;			// current face, kInvalidFace if none
	uint16_t			nextFaceID;
	uint16_t			size;

	uint16_t			use;				// clock, bumped on every glyph access
	uint8_t				locked;

	CFEGlyphRecord		glyph;

	uint8_t				*cache;
	uint32_t			cacheSize;
	uint32_t			cacheUsed;
	uint16_t			entryCount;
	uint16_t			*buckets;			// glyph lookup, at the end of the cache block
	uint16_t			bucketCount;
	void				*frameBlocks;		// glyphs that borrowed the heap for this frame

	// set while rasterizing one glyph
	uint8_t				*bitmap;			// 4 bits per pixel, one row after another
	uint16_t			bitmapRowBytes;
	uint8_t				*row;				// one row expanded to a byte per pixel, for encoding
	int32_t				renderXMin;
	int32_t				renderYMin;
	uint16_t			renderWidth;
	uint16_t			renderHeight;
	uint8_t				*out;				// RLE output
	uint32_t			outOffset;			// where it lives in the cache
	uint32_t			outLength;
	uint32_t			outLimit;
	uint8_t				outFirstNybble;
	uint8_t				overflowed;			// the encoder ran past the space reserved

#if MODDEF_CFE_FT_STATS
	uint32_t			statLookups;
	uint32_t			statHits;
	uint32_t			statRendered;
	uint32_t			statPacked;
	uint32_t			statRaw;
	uint32_t			statUncached;		// rendered but did not fit
	uint32_t			statOverflow;		// pixels came from the heap for one frame
	uint32_t			statEvictions;
	uint32_t			statEvictionPasses;
	uint32_t			statFaceOpens;
	uint32_t			statPeakBytes;
	uint16_t			statPeakEntries;
#endif
};

typedef struct CommodettoFontEngineRecord CommodettoFontEngineRecord;
typedef struct CommodettoFontEngineRecord *CFE;

#define kInvalidFace (0xFF)
#define kNoBlock (0xFFFF)		// end of a hash chain

static void *ftAllocCFE(FT_Memory memory, long size);
static void ftFreeCFE(FT_Memory memory, void *block);
static void *ftReallocCFE(FT_Memory memory, long cur_size, long new_size, void *block);

static uint8_t useFont(CFE cfe, const void *fontData, uint32_t fontDataSize);
static CFEEntry findEntry(CFE cfe, uint16_t glyphID);
static void hashInsert(CFE cfe, uint32_t dataOffset);
static void hashRemove(CFE cfe, CFEEntry entry);
static void unlockAll(CFE cfe);
static uint16_t touch(CFE cfe);
static CFEEntry loadGlyph(CFE cfe, uint16_t glyphID, uint8_t needPixels);
static uint8_t evictSome(CFE cfe, uint32_t need);
static void cacheInitialize(CFE cfe);
static uint32_t cacheAllocate(CFE cfe, uint32_t size);
static void cacheFree(CFE cfe, uint32_t dataOffset);
static void cacheTrim(CFE cfe, uint32_t dataOffset, uint32_t size);

CommodettoFontEngine CFENew(void)
{
	CFE cfe = c_calloc(1, sizeof(CommodettoFontEngineRecord));
	if (!cfe)
		return NULL;

	cfe->memory.alloc = ftAllocCFE;
	cfe->memory.free = ftFreeCFE;
	cfe->memory.realloc = ftReallocCFE;

	if (FT_New_Library(&cfe->memory, &cfe->library))
		goto bail;
	FT_Add_Default_Modules(cfe->library);

	if (ft_grays_raster.raster_new(&cfe->memory, &cfe->raster))
		goto bail;

	{
		uint32_t size = MODDEF_CFE_FT_CACHESIZE;

		while (size >= 1024) {
			/*
				One bucket for every few glyphs the cache might hold, rounded to a
				power of two so the index is a mask. At two bytes each this is a
				rounding error beside the pixels: 32 buckets for an 8K cache, 256
				for 64K. They sit at the end of the same block, since a cache and
				its index are useless apart.
			*/
			uint32_t count = 16;
			while ((count < 256) && ((count * 512) < size))
				count <<= 1;

			cfe->cache = c_malloc(size + (count * sizeof(uint16_t)));
			if (cfe->cache) {
				cfe->cacheSize = size;
				cacheInitialize(cfe);

				cfe->buckets = (uint16_t *)(cfe->cache + size);
				cfe->bucketCount = (uint16_t)count;
				while (count--)
					cfe->buckets[count] = kNoBlock;

				break;
			}

			size >>= 1;
		}
	}

	cfe->faceIndex = kInvalidFace;
	cfe->size = 12;

	return cfe;

bail:
	CFEDispose(cfe);
	return NULL;
}

void CFEDispose(CommodettoFontEngine c)
{
	CFE cfe = (CFE)c;
	uint8_t i;

	if (!cfe)
		return;

	for (i = 0; i < MODDEF_CFE_FT_FACES; i++) {
		if (cfe->faces[i].face)
			FT_Done_Face(cfe->faces[i].face);
	}

	if (cfe->raster)
		ft_grays_raster.raster_done(cfe->raster);

	if (cfe->library)
		FT_Done_Library(cfe->library);

	while (cfe->frameBlocks) {
		void *next = *(void **)cfe->frameBlocks;
		c_free(cfe->frameBlocks);
		cfe->frameBlocks = next;
	}

	if (cfe->cache)			// the bucket index is part of this block
		c_free(cfe->cache);

	c_free(cfe);
}

void CFELockCache(CommodettoFontEngine c, uint8_t lock)
{
	CFE cfe = (CFE)c;

	if (lock) {
		cfe->locked = 1;
		return;
	}

	cfe->locked = 0;
	unlockAll(cfe);

#if MODDEF_CFE_FT_STATS
	{
		char report[160];

		if (cfe->cacheUsed > cfe->statPeakBytes)
			cfe->statPeakBytes = cfe->cacheUsed;
		if (cfe->entryCount > cfe->statPeakEntries)
			cfe->statPeakEntries = cfe->entryCount;

		snprintf(report, sizeof(report),
			"cfe: %u/%u bytes (peak %u), %u glyphs (peak %u), %u%% hit of %u, rendered %u (%u packed, %u raw, %u uncached), %u evicted in %u passes, %u overflow, %u faces\n",
			(unsigned)cfe->cacheUsed, (unsigned)cfe->cacheSize, (unsigned)cfe->statPeakBytes,
			(unsigned)cfe->entryCount, (unsigned)cfe->statPeakEntries,
			(unsigned)(cfe->statLookups ? ((cfe->statHits * 100) / cfe->statLookups) : 0), (unsigned)cfe->statLookups,
			(unsigned)cfe->statRendered, (unsigned)cfe->statPacked, (unsigned)cfe->statRaw, (unsigned)cfe->statUncached,
			(unsigned)cfe->statEvictions, (unsigned)cfe->statEvictionPasses, (unsigned)cfe->statOverflow, (unsigned)cfe->statFaceOpens);
		cfeReport(report);
	}
#endif
}

void CFESetFontData(CommodettoFontEngine c, const void *fontData, uint32_t fontDataSize)
{
	CFE cfe = (CFE)c;
	uint8_t index;

	if ((kInvalidFace != cfe->faceIndex) && (cfe->faces[cfe->faceIndex].fontData == fontData))
		return;			// unchanged

	index = useFont(cfe, fontData, fontDataSize);
	cfe->faceIndex = index;
	if (kInvalidFace == index)
		return;

	FT_Set_Pixel_Sizes(cfe->faces[index].face, 0, cfe->size);
}

void CFESetFontSize(CommodettoFontEngine c, int32_t size)
{
	CFE cfe = (CFE)c;

	if ((size <= 0) || (size > 0xFFFF) || (size == cfe->size))
		return;

	cfe->size = (uint16_t)size;

	if (kInvalidFace != cfe->faceIndex)
		FT_Set_Pixel_Sizes(cfe->faces[cfe->faceIndex].face, 0, cfe->size);
}

void CFEGetFontMetrics(CommodettoFontEngine c, int32_t *ascent, int32_t *descent, int32_t *leading)
{
	CFE cfe = (CFE)c;
	FT_Size_Metrics *metrics;
	int32_t a, d, h;

	if (kInvalidFace == cfe->faceIndex) {
		if (ascent) *ascent = 0;
		if (descent) *descent = 0;
		if (leading) *leading = 0;
		return;
	}

	metrics = &cfe->faces[cfe->faceIndex].face->size->metrics;
	a = (metrics->ascender + 63) >> 6;			// 26.6, and descender is negative
	d = -(metrics->descender >> 6);
	h = (metrics->height + 63) >> 6;

	if (ascent) *ascent = a;
	if (descent) *descent = d;
	if (leading) *leading = (h > (a + d)) ? (h - (a + d)) : 0;
}

CFEGlyph CFEGetGlyphFromGlyphID(CommodettoFontEngine c, uint16_t glyphID, uint8_t needPixels)
{
	CFE cfe = (CFE)c;
	CFEGlyph glyph = &cfe->glyph;
	CFEEntry entry;

	if (kInvalidFace == cfe->faceIndex)
		return C_NULL;

	entry = findEntry(cfe, glyphID);
#if MODDEF_CFE_FT_STATS
	cfe->statLookups += 1;
	if (entry && (!needPixels || (entry->flags & kEntryHasBits)))
		cfe->statHits += 1;
#endif
	if (!entry || (needPixels && !(entry->flags & kEntryHasBits)))
		entry = loadGlyph(cfe, glyphID, needPixels);
	if (!entry)
		return C_NULL;

	entry->use = touch(cfe);
	if (cfe->locked)
		entry->flags |= kEntryLocked;

	glyph->substitute = C_NULL;
	glyph->advance = entry->advance;

	if (!needPixels) {
		glyph->dx = entry->dx;			// where the ink is, for a caller measuring rather than drawing
		glyph->dy = entry->dy;
		glyph->w = entry->w;
		glyph->h = entry->h;
		glyph->sx = glyph->sy = 0;
		glyph->format = 0;
		glyph->bits = C_NULL;
		return glyph;
	}

	if (!(entry->flags & kEntryHasBits)) {
		// glyph rendered but no room
		glyph->dx = glyph->dy = 0;
		glyph->sx = glyph->sy = 0;
		glyph->w = glyph->h = 0;
		glyph->format = kCommodettoBitmapGray16;
		glyph->bits = cfe->cache;
		return glyph;
	}

	glyph->dx = entry->dx;
	glyph->dy = entry->dy;
	glyph->w = entry->w;
	glyph->h = entry->h;
	glyph->sx = 0;
	glyph->sy = 0;
	glyph->format = kCommodettoBitmapGray16;
	if (entry->flags & kEntryPacked)
		glyph->format |= kCommodettoBitmapPacked;
	glyph->bits = entryPixels(entry);

	return glyph;
}

CFEGlyph CFEGetGlyphFromUnicode(CommodettoFontEngine c, uint32_t unicode, uint8_t needPixels)
{
	CFE cfe = (CFE)c;

	if (kInvalidFace == cfe->faceIndex)
		return C_NULL;

	FT_UInt glyphID = FT_Get_Char_Index(cfe->faces[cfe->faceIndex].face, (FT_ULong)unicode);
	return CFEGetGlyphFromGlyphID(c, (uint16_t)glyphID, needPixels);
}

int16_t CFEGetKerningOffset(CommodettoFontEngine c, uint32_t unicode1, uint32_t unicode2)
{
#if MODDEF_CFE_KERN
	CFE cfe = (CFE)c;
	FT_Face face;
	FT_Vector delta;
	FT_UInt left, right;

	if (kInvalidFace == cfe->faceIndex)
		return 0;

	face = cfe->faces[cfe->faceIndex].face;
	if (!FT_HAS_KERNING(face))
		return 0;

	left = FT_Get_Char_Index(face, (FT_ULong)unicode1);
	right = FT_Get_Char_Index(face, (FT_ULong)unicode2);
	if (!left || !right)
		return 0;

	if (FT_Get_Kerning(face, left, right, FT_KERNING_DEFAULT, &delta))
		return 0;

	return (int16_t)(delta.x >> 6);
#else
	return 0;
#endif
}

/*
	The glyph as outlines rather than pixels, for a caller that wants to scale,
	rotate, or stroke it. Rendering is left to whoever asked: commodetto/outline
	draws these with the same rasterizer the font engine uses for glyphs, so
	CFERenderOutline has nothing to add and is not implemented here.
*/
void CFEGetOutlineFromUnicode(CommodettoFontEngine c, uint32_t unicode, uint8_t **outlineOut, uint32_t *outlineSize)
{
	CFE cfe = (CFE)c;
	FT_Face face;
	FT_Outline *outline;
	FT_UInt glyphID;
	uint32_t byteLength;
	int i, pointCount, contourCount;
	uint8_t *buffer, *tags;
	CFEOutline result;
	int32_t *points;
	uint16_t *contours;

	*outlineOut = C_NULL;
	*outlineSize = 0;

	if (kInvalidFace == cfe->faceIndex)
		return;

	face = cfe->faces[cfe->faceIndex].face;
	// glyph 0 when the character is absent, so the outline shows what drawing it would
	glyphID = FT_Get_Char_Index(face, (FT_ULong)unicode);

	if (FT_Load_Glyph(face, glyphID, kGlyphLoadFlags))
		return;

	if (FT_GLYPH_FORMAT_OUTLINE != face->glyph->format)
		return;			// an embedded bitmap strike has no outline to give

	outline = &face->glyph->outline;
	pointCount = outline->n_points;
	contourCount = outline->n_contours;
	if (!pointCount || !contourCount)			// a space draws nothing but still advances
		pointCount = contourCount = 0;

	byteLength = CFEOutlineByteLength(pointCount, contourCount);
	buffer = c_malloc(byteLength);
	if (!buffer)
		return;

	result = (CFEOutline)buffer;
	result->pointCount = (uint16_t)pointCount;
	result->contourCount = (uint16_t)contourCount;

	// linearHoriAdvance is 16.16 and unrounded, where slot->advance has been fitted to the pixel grid
	result->advance = (int32_t)(face->glyph->linearHoriAdvance >> 10);

	points = (int32_t *)(result + 1);
	for (i = 0; i < pointCount; i++) {
		*points++ = (int32_t)outline->points[i].x;
		*points++ = (int32_t)outline->points[i].y;
	}

	contours = (uint16_t *)points;
	for (i = 0; i < contourCount; i++)
		*contours++ = (uint16_t)outline->contours[i];

	tags = (uint8_t *)contours;
	for (i = 0; i < pointCount; i++)
		*tags++ = (uint8_t)FT_CURVE_TAG(outline->tags[i]);

	*outlineOut = buffer;
	*outlineSize = byteLength;
}

int CFEShape(CommodettoFontEngine cfe, const char *utf8In, int32_t byteLengthIn, char *utf8Out, int32_t byteLengthOut)
{
	return -1;
}

void CFELayoutRun(CommodettoFontEngine c, const char *utf8, int32_t byteLength, CFERun run, int32_t runLength, int32_t width)
{
	CFE cfe = (CFE)c;
	const char *utf8End = utf8 + byteLength;

	while (utf8 < utf8End) {
		CFEGlyph glyph;
		unsigned int unicode = PocoNextFromUTF8((uint8_t **)&utf8);
		FT_UInt glyphID;

		if (kInvalidFace == cfe->faceIndex)
			break;

		glyphID = FT_Get_Char_Index(cfe->faces[cfe->faceIndex].face, (FT_ULong)unicode);
		if (0 == glyphID)
			continue;

		glyph = CFEGetGlyphFromGlyphID(c, (uint16_t)glyphID, 0);
		if (!glyph)
			continue;

		run->glyphID = (uint16_t)glyphID;
		run->advance = glyph->advance;
		run++;

		runLength -= 1;
		if (1 == runLength)
			break;
	}

	run->glyphID = kInvalidGlyphID;
}

/*
	faces
*/

static uint8_t useFont(CFE cfe, const void *fontData, uint32_t fontDataSize)
{
	uint8_t i, slot = kInvalidFace;
	uint32_t oldest = 0xFFFFFFFF;

	if (!fontData || !fontDataSize)
		return kInvalidFace;

	for (i = 0; i < MODDEF_CFE_FT_FACES; i++) {
		if (cfe->faces[i].fontData == fontData) {
			cfe->faces[i].use = ++cfe->use;
			return i;
		}
	}

	/*
		Take a free slot, or recycle the least recently used one. Recycling is
		always safe: glyphs already rendered from the old font are just pixels in
		the cache, and they carry the face id they were made with, so they can
		never be mistaken for the new font's. They are reclaimed by ordinary
		eviction. Only the FT_Face goes away, and nothing outside this file holds
		one.
	*/
	for (i = 0; i < MODDEF_CFE_FT_FACES; i++) {
		if (!cfe->faces[i].face) {
			slot = i;
			break;
		}
		if (cfe->faces[i].use < oldest) {
			oldest = cfe->faces[i].use;
			slot = i;
		}
	}

	if (cfe->faces[slot].face) {
		FT_Done_Face(cfe->faces[slot].face);
		cfe->faces[slot].face = NULL;
		cfe->faces[slot].fontData = NULL;
	}

	if (FT_New_Memory_Face(cfe->library, (const FT_Byte *)fontData, (FT_Long)fontDataSize, 0, &cfe->faces[slot].face)) {
		return kInvalidFace;
	}

	cfe->faces[slot].fontData = fontData;
	cfe->faces[slot].fontDataSize = fontDataSize;
#if MODDEF_CFE_FT_STATS
	cfe->statFaceOpens += 1;
#endif
	cfe->faces[slot].id = ++cfe->nextFaceID;
	cfe->faces[slot].use = ++cfe->use;

	return slot;
}

/*
	cache
*/

/*
	Every glyph is one block in the cache: its record, and then its pixels. The
	directory is the cache, so how many glyphs fit depends on how big they are
	rather than on a separate fixed count -- small text gets hundreds of entries
	and large text gets dozens, from one budget. Blocks never move, so a pointer
	to a record stays good.

	Lookup goes through a small bucket index rather than a walk: a cache sized for
	a real application holds hundreds of glyphs, and every character is looked up
	twice, once to measure a string and once to draw it. Chains hold block
	offsets, which are multiples of four, so 16 bits covers a 256K cache.
*/

#define blockAt(cfe, dataOffset) ((CFEEntry)((cfe)->cache + (dataOffset)))
#define blockOffsetOf(cfe, entry) ((uint32_t)((uint8_t *)(entry) - (cfe)->cache))

static uint16_t bucketOf(CFE cfe, uint16_t faceID, uint16_t size, uint16_t glyphID)
{
	uint32_t hash = glyphID ^ (size << 5) ^ (faceID << 11);

	hash ^= hash >> 7;

	return (uint16_t)(hash & (cfe->bucketCount - 1));
}

static void hashInsert(CFE cfe, uint32_t dataOffset)
{
	CFEEntry entry = blockAt(cfe, dataOffset);
	uint16_t bucket = bucketOf(cfe, entry->faceID, entry->size, entry->glyphID);

	entry->hashNext = cfe->buckets[bucket];
	cfe->buckets[bucket] = (uint16_t)(dataOffset >> 2);
}

static void hashRemove(CFE cfe, CFEEntry entry)
{
	uint16_t bucket = bucketOf(cfe, entry->faceID, entry->size, entry->glyphID);
	uint16_t want = (uint16_t)(blockOffsetOf(cfe, entry) >> 2);
	uint16_t walk = cfe->buckets[bucket];

	if (walk == want) {
		cfe->buckets[bucket] = entry->hashNext;
		return;
	}

	while (kNoBlock != walk) {
		CFEEntry other = blockAt(cfe, (uint32_t)walk << 2);

		if (other->hashNext == want) {
			other->hashNext = entry->hashNext;
			return;
		}
		walk = other->hashNext;
	}
}

static CFEEntry findEntry(CFE cfe, uint16_t glyphID)
{
	uint16_t faceID, walk;

	if (!cfe->cache)
		return C_NULL;

	faceID = cfe->faces[cfe->faceIndex].id;
	walk = cfe->buckets[bucketOf(cfe, faceID, cfe->size, glyphID)];

	while (kNoBlock != walk) {
		CFEEntry entry = blockAt(cfe, (uint32_t)walk << 2);

		if ((entry->glyphID == glyphID) && (entry->size == cfe->size) && (entry->faceID == faceID))
			return entry;

		walk = entry->hashNext;
	}

	return C_NULL;
}

/*
	The cache is a small heap inside one buffer. Nothing in it ever moves: Poco's
	drawing commands point straight at a glyph's pixels, so an entry the frame is
	building cannot be relocated -- and a ring would have no choice but to evict
	whatever sits at its tail, which may be exactly such an entry.

	Every block begins with a size that includes its header, with the low bit
	marking it in use. Sizes are multiples of four, so that bit is free. Freeing a
	block merges it with its neighbours, so the space of adjacent evicted glyphs
	returns as one block rather than a scatter of unusable gaps.
*/

#define kCacheNone (0xFFFFFFFF)
#define kCacheHeader (4)
#define kCacheMinimum (kCacheHeader + 8)		// not worth splitting off less

#define cacheHeaderAt(cfe, offset) ((uint32_t *)((cfe)->cache + (offset)))
#define cacheBlockSize(header) ((header) & ~3)
#define cacheBlockUsed(header) ((header) & 1)

static void cacheInitialize(CFE cfe)
{
	*cacheHeaderAt(cfe, 0) = cfe->cacheSize & ~3;		// one free block covering everything
	cfe->cacheUsed = 0;
}

static uint32_t cacheAllocate(CFE cfe, uint32_t size)
{
	uint32_t need = ((size + 3) & ~3) + kCacheHeader;
	uint32_t offset = 0;

	if (!cfe->cache)
		return kCacheNone;

	while (offset < cfe->cacheSize) {
		uint32_t header = *cacheHeaderAt(cfe, offset);
		uint32_t blockSize = cacheBlockSize(header);

		if (!blockSize)
			break;			// should not happen: a zero size would not advance

		if (!cacheBlockUsed(header) && (blockSize >= need)) {
			if ((blockSize - need) >= kCacheMinimum) {
				*cacheHeaderAt(cfe, offset + need) = blockSize - need;		// the remainder stays free
				blockSize = need;
			}
			*cacheHeaderAt(cfe, offset) = blockSize | 1;
			cfe->cacheUsed += blockSize;
			return offset + kCacheHeader;
		}

		offset += blockSize;
	}

	return kCacheNone;
}

static void cacheFree(CFE cfe, uint32_t dataOffset)
{
	uint32_t offset = dataOffset - kCacheHeader;
	uint32_t header = *cacheHeaderAt(cfe, offset);

	cfe->cacheUsed -= cacheBlockSize(header);
	*cacheHeaderAt(cfe, offset) = cacheBlockSize(header);		// clear the in use bit

	// merge every run of adjacent free blocks
	offset = 0;
	while (offset < cfe->cacheSize) {
		uint32_t size = cacheBlockSize(*cacheHeaderAt(cfe, offset));

		if (!size)
			break;

		if (!cacheBlockUsed(*cacheHeaderAt(cfe, offset))) {
			uint32_t next = offset + size;

			while (next < cfe->cacheSize) {
				uint32_t nextHeader = *cacheHeaderAt(cfe, next);
				if (cacheBlockUsed(nextHeader))
					break;
				size += cacheBlockSize(nextHeader);
				next += cacheBlockSize(nextHeader);
			}
			*cacheHeaderAt(cfe, offset) = size;
		}

		offset += size;
	}
}

/*
	Shrink a block to what was actually used, returning the tail to the cache.
*/
static void cacheTrim(CFE cfe, uint32_t dataOffset, uint32_t size)
{
	uint32_t offset = dataOffset - kCacheHeader;
	uint32_t blockSize = cacheBlockSize(*cacheHeaderAt(cfe, offset));
	uint32_t need = ((size + 3) & ~3) + kCacheHeader;

	if ((blockSize - need) < kCacheMinimum)
		return;

	*cacheHeaderAt(cfe, offset) = need | 1;
	*cacheHeaderAt(cfe, offset + need) = blockSize - need;
	cfe->cacheUsed -= blockSize - need;
}

/*
	Drop the least recently used glyph, freeing its directory slot and its pixels.
	An entry in use by the frame being built is never chosen: Poco is about to
	read those pixels.
*/
/*
	Make room for one glyph, dropping several while the walk is already being
	paid for. Finding the single least recently used glyph means walking every
	block, and a cache holding hundreds of glyphs would repeat that walk for each
	one dropped -- so this frees enough for the glyph at hand plus a little slack,
	and no more. Freeing a fixed fraction would be simpler but throws away most of
	the working set on a small cache.

	Each call considers the oldest quarter of the ages present, so if one pass
	does not yield a block big enough the caller comes back and the next pass
	reaches further. Nothing the frame being built has drawn is ever dropped:
	Poco is about to read those pixels.
*/
static uint8_t evictSome(CFE cfe, uint32_t need)
{
	uint32_t offset, target, freed = 0;
	uint16_t oldest = 0xFFFF, newest = 0, threshold;

	if (!cfe->cache)
		return 0;

	for (offset = 0; offset < cfe->cacheSize; ) {
		uint32_t header = *cacheHeaderAt(cfe, offset);
		uint32_t size = cacheBlockSize(header);

		if (!size)
			break;

		if (cacheBlockUsed(header)) {
			CFEEntry entry = blockAt(cfe, offset + kCacheHeader);

			if (!(entry->flags & kEntryLocked)) {
				if (entry->use < oldest) oldest = entry->use;
				if (entry->use > newest) newest = entry->use;
			}
		}

		offset += size;
	}

	if (oldest > newest)
		return 0;			// every glyph belongs to the frame being built

	threshold = oldest + ((newest - oldest) >> 2);

	target = need + kCacheHeader;
	if (target < (cfe->cacheSize >> 4))
		target = cfe->cacheSize >> 4;		// a little slack, so the walk is not repeated per glyph

	for (offset = 0; (offset < cfe->cacheSize) && (freed < target); ) {
		uint32_t header = *cacheHeaderAt(cfe, offset);
		uint32_t size = cacheBlockSize(header);

		if (!size)
			break;

		if (cacheBlockUsed(header)) {
			CFEEntry entry = blockAt(cfe, offset + kCacheHeader);

			if (!(entry->flags & kEntryLocked) && (entry->use <= threshold)) {
				hashRemove(cfe, entry);
				cacheFree(cfe, offset + kCacheHeader);
				cfe->entryCount -= 1;
				freed += size;
#if MODDEF_CFE_FT_STATS
				cfe->statEvictions += 1;
#endif
				continue;			// this block just merged with its neighbours: read it again
			}
		}

		offset += size;
	}

#if MODDEF_CFE_FT_STATS
	cfe->statEvictionPasses += 1;
#endif

	return freed != 0;
}

/*
	The clock is 16 bits, so it has to be allowed to wrap. Everything ages to the
	same value when it does, which costs one pass every 65536 glyph lookups and
	leaves eviction picking by position for a while.
*/
static uint16_t touch(CFE cfe)
{
	if ((0 != ++cfe->use) || !cfe->cache)
		return cfe->use;

	{
		uint32_t offset;

		for (offset = 0; offset < cfe->cacheSize; ) {
			uint32_t header = *cacheHeaderAt(cfe, offset);
			uint32_t size = cacheBlockSize(header);

			if (!size)
				break;

			if (cacheBlockUsed(header))
				blockAt(cfe, offset + kCacheHeader)->use = 0;

			offset += size;
		}
	}

	cfe->use = 1;

	return cfe->use;
}

/*
	The frame is finished, so every glyph is a candidate for eviction again, and
	any that borrowed the heap can give it back.
*/
static void unlockAll(CFE cfe)
{
	uint32_t offset = 0;

	if (!cfe->cache)
		goto frame;

	while (offset < cfe->cacheSize) {
		uint32_t header = *cacheHeaderAt(cfe, offset);
		uint32_t size = cacheBlockSize(header);

		if (!size)
			break;

		if (cacheBlockUsed(header))
			blockAt(cfe, offset + kCacheHeader)->flags &= ~kEntryLocked;

		offset += size;
	}

frame:
	while (cfe->frameBlocks) {
		void *next = *(void **)cfe->frameBlocks;
		c_free(cfe->frameBlocks);
		cfe->frameBlocks = next;
	}
}

static void writeNybble(CFE cfe, uint8_t value)
{
	if (cfe->outLength >= cfe->outLimit) {
		cfe->overflowed = 1;
		return;
	}

	if (cfe->outFirstNybble) {
		cfe->out[cfe->outLength] = value;
		cfe->outFirstNybble = 0;
	}
	else {
		cfe->out[cfe->outLength] |= value << 4;
		cfe->outLength += 1;
		cfe->outFirstNybble = 1;
	}
}

/*
	The run encoding is the one commodetto/RLE4Out writes and Poco's
	Gray16 | Packed blitter reads:

		SKIP:  0nnn  (2 to 9 transparent pixels)
		SOLID: 10nn  (2 to 5 fully inked pixels)
		QUOTE: 11nn  followed by 1 to 4 pixel values

	Gray levels are inverted with respect to coverage: 0 is fully inked and 15
	is transparent.
*/
static void encodeRow(CFE cfe)
{
	const uint8_t solid = 0, skip = 15;
	uint8_t *scan = cfe->row;
	int remain = cfe->renderWidth;
	int pos = 0;

	while (remain > 0) {
		if (remain >= 2) {
			if ((skip == scan[pos]) && (skip == scan[pos + 1])) {
				int count = 2;
				while (((remain - count) > 0) && (count < 9) && (skip == scan[pos + count]))
					count += 1;
				writeNybble(cfe, (uint8_t)(count - 2));
				remain -= count;
				pos += count;
				continue;
			}

			if ((solid == scan[pos]) && (solid == scan[pos + 1])) {
				int count = 2;
				while (((remain - count) > 0) && (count < 5) && (solid == scan[pos + count]))
					count += 1;
				writeNybble(cfe, (uint8_t)(0x08 | (count - 2)));
				remain -= count;
				pos += count;
				continue;
			}
		}

		{
			int quoteLen = 1, i;

			while (quoteLen < 4) {
				if (remain < (quoteLen + 2)) {
					quoteLen = (remain < 4) ? remain : 4;
					break;
				}
				if ((scan[pos + quoteLen] == scan[pos + quoteLen + 1]) &&
					((solid == scan[pos + quoteLen]) || (skip == scan[pos + quoteLen])))
					break;
				quoteLen += 1;
			}

			writeNybble(cfe, (uint8_t)(0x0C | (quoteLen - 1)));
			for (i = 0; i < quoteLen; i++)
				writeNybble(cfe, scan[pos + i]);
			remain -= quoteLen;
			pos += quoteLen;
		}
	}
}

static void expandRow(CFE cfe, uint16_t row)
{
	const uint8_t *bits = cfe->bitmap + (row * cfe->bitmapRowBytes);
	uint8_t *out = cfe->row;
	uint16_t x;

	for (x = 0; x < cfe->renderWidth; x += 1, out += 1) {
		uint8_t two = bits[x >> 1];
		*out = (x & 1) ? (two & 0x0F) : (two >> 4);
	}
}

static void glyphSpan(int y, int count, const FT_Span *spans, void *user)
{
	CFE cfe = user;
	uint8_t *bits;
	int i;

	/*
		The outline was flipped vertically before rasterizing, so y counts down the
		glyph. Rows do not arrive in order, and a row can be visited more than
		once: ftgrays splits work into bands, vertically when an outline needs more
		cells than its pool holds, and horizontally within each of those. So spans
		are painted into the glyph's pixels here and compressed afterwards, rather
		than compressed one row at a time.
	*/
	int32_t row = y - cfe->renderYMin;
	if ((row < 0) || (row >= cfe->renderHeight))
		return;

	bits = cfe->bitmap + (row * cfe->bitmapRowBytes);

	for (i = 0; i < count; i++) {
		int32_t x = (int16_t)spans[i].x - cfe->renderXMin;
		int32_t len = spans[i].len;
		uint8_t value = (uint8_t)((255 - spans[i].coverage) >> 4);		// coverage 255 is fully inked, which is gray 0

		if (x < 0) {
			len += x;
			x = 0;
		}
		if ((x + len) > cfe->renderWidth)
			len = cfe->renderWidth - x;

		while (len-- > 0) {
			uint8_t *b = bits + (x >> 1);
			if (x & 1)
				*b = (*b & 0xF0) | value;
			else
				*b = (*b & 0x0F) | (value << 4);
			x += 1;
		}
	}
}


static CFEEntry loadGlyph(CFE cfe, uint16_t glyphID, uint8_t needPixels)
{
	FT_Face face = cfe->faces[cfe->faceIndex].face;
	FT_GlyphSlot slot;
	CFEEntry entry, existing;
	FT_BBox box;
	int32_t ascent;
	uint32_t rawLength = 0, dataOffset = kCacheNone, need;
	void *frameBlock = C_NULL;

#if MODDEF_CFE_FT_STATS
	if (needPixels)
		cfe->statRendered += 1;
#endif
	if (FT_Load_Glyph(face, glyphID, kGlyphLoadFlags)) {
		return C_NULL;
	}

	slot = face->glyph;

	/*
		The record and its pixels are one block, so how big the glyph is has to be
		known before anything is allocated.
	*/
	if (needPixels && (FT_GLYPH_FORMAT_OUTLINE == slot->format)) {
		FT_Outline_Get_CBox(&slot->outline, &box);
		box.xMin &= ~63;
		box.yMin &= ~63;
		box.xMax = (box.xMax + 63) & ~63;
		box.yMax = (box.yMax + 63) & ~63;

		cfe->renderWidth = (uint16_t)((box.xMax - box.xMin) >> 6);
		cfe->renderHeight = (uint16_t)((box.yMax - box.yMin) >> 6);
		cfe->renderXMin = box.xMin >> 6;
		cfe->bitmapRowBytes = (cfe->renderWidth + 1) >> 1;
		rawLength = (uint32_t)cfe->bitmapRowBytes * cfe->renderHeight;
	}
	else
		needPixels = 0;			//@@ embedded bitmap strikes are not handled yet

	if (rawLength > 0xFFFF) {	// a record holds its length in 16 bits
		needPixels = 0;
		rawLength = 0;
	}

	/*
		Any record already held for this glyph is replaced, since its block was
		sized for what it had then. One the frame is using cannot be freed yet, so
		it is only unlinked, and eviction reclaims it later.
	*/
	existing = findEntry(cfe, glyphID);
	if (existing) {
		hashRemove(cfe, existing);
		if (!(existing->flags & kEntryLocked)) {
			cacheFree(cfe, blockOffsetOf(cfe, existing));
			cfe->entryCount -= 1;
		}
	}

	need = sizeof(CFEEntryRecord) + rawLength;
	dataOffset = cacheAllocate(cfe, need);
	while (kCacheNone == dataOffset) {
		if (!evictSome(cfe, need))
			break;
		dataOffset = cacheAllocate(cfe, need);
	}

	if (kCacheNone != dataOffset) {
		entry = blockAt(cfe, dataOffset);
		cfe->entryCount += 1;
	}
	else {
		/*
			Nothing can be evicted: every glyph in the cache belongs to the frame
			being built. Borrow the heap until the frame ends rather than drop the
			glyph, which would leave a hole in the text.
		*/
		frameBlock = c_malloc(sizeof(void *) + need);
		if (!frameBlock) {
#if MODDEF_CFE_FT_STATS
			cfe->statUncached += 1;
#endif
			return C_NULL;
		}

#if MODDEF_CFE_FT_STATS
		cfe->statOverflow += 1;
#endif
		*(void **)frameBlock = cfe->frameBlocks;
		cfe->frameBlocks = frameBlock;
		entry = (CFEEntry)(sizeof(void *) + (uint8_t *)frameBlock);
	}

	c_memset(entry, 0, sizeof(CFEEntryRecord));
	entry->glyphID = glyphID;
	entry->size = cfe->size;
	entry->faceID = cfe->faces[cfe->faceIndex].id;
	entry->advance = (int16_t)((slot->advance.x + 32) >> 6);
	entry->use = touch(cfe);
	entry->hashNext = kNoBlock;

	if (frameBlock)
		entry->flags |= kEntryOverflow;
	else
		hashInsert(cfe, dataOffset);

	if (!needPixels) {
		/*
			Where the ink of this glyph falls, which is not the box its advance
			describes: a J reaches back before the pen and an f leans past it.
			The metrics are already here from loading the glyph, so a caller
			measuring a string can learn how far the ink escapes without any of
			it being rendered.
		*/
		FT_Glyph_Metrics *metrics = &slot->metrics;
		int32_t left = metrics->horiBearingX >> 6;								// floor, so ink is never outside
		int32_t right = (metrics->horiBearingX + metrics->width + 63) >> 6;		// ceiling, likewise
		int32_t top = (metrics->horiBearingY + 63) >> 6;
		int32_t bottom = (metrics->horiBearingY - metrics->height) >> 6;

		CFEGetFontMetrics((CommodettoFontEngine)cfe, &ascent, NULL, NULL);
		entry->dx = (int16_t)left;
		entry->dy = (int16_t)(ascent - top);			// Poco positions from the top of the line
		entry->w = (uint16_t)(right - left);
		entry->h = (uint16_t)(top - bottom);

		return entry;
	}

	CFEGetFontMetrics((CommodettoFontEngine)cfe, &ascent, NULL, NULL);
	entry->dx = (int16_t)cfe->renderXMin;
	entry->dy = (int16_t)(ascent - (box.yMax >> 6));		// Poco positions from the top of the line
	entry->w = cfe->renderWidth;
	entry->h = cfe->renderHeight;

	if (!rawLength) {			// a space, for instance
		entry->flags |= kEntryHasBits;
		return entry;
	}

	/*
		FreeType's y axis points up and it sweeps in increasing y, so spans would
		arrive bottom row first while a glyph bitmap is stored top row first.
		Flipping the outline makes increasing y mean increasing bitmap row.
	*/
	{
		FT_Matrix flip = { 0x10000, 0, 0, -0x10000 };
		FT_Outline_Transform(&slot->outline, &flip);
	}
	cfe->renderYMin = -(box.yMax >> 6);

	/*
		Transient: four bits per pixel for the glyph, plus one row expanded to a
		byte per pixel while it is compressed. Both are freed before this returns.
	*/
	cfe->bitmap = c_malloc(rawLength);
	if (!cfe->bitmap)
		return entry;
	c_memset(cfe->bitmap, 0xFF, rawLength);		// 15 is transparent

	cfe->row = c_malloc(cfe->renderWidth);
	if (!cfe->row) {
		c_free(cfe->bitmap);
		cfe->bitmap = C_NULL;
		return entry;
	}

	cfe->out = entryPixels(entry);
	cfe->outLimit = rawLength;
	cfe->outLength = 0;
	cfe->outFirstNybble = 1;
	cfe->overflowed = 0;

	{
		FT_Raster_Params params;

		c_memset(&params, 0, sizeof(params));
		params.flags = FT_RASTER_FLAG_AA | FT_RASTER_FLAG_DIRECT | FT_RASTER_FLAG_CLIP;
		params.source = &slot->outline;
		params.gray_spans = glyphSpan;
		params.user = cfe;
		params.clip_box.xMin = cfe->renderXMin;
		params.clip_box.yMin = cfe->renderYMin;
		params.clip_box.xMax = cfe->renderXMin + cfe->renderWidth;
		params.clip_box.yMax = cfe->renderYMin + cfe->renderHeight;

		ft_grays_raster.raster_render(cfe->raster, &params);
	}

	{
		uint16_t row;

		for (row = 0; row < cfe->renderHeight; row++) {
			expandRow(cfe, row);
			encodeRow(cfe);
		}
	}
	if (!cfe->outFirstNybble)
		cfe->outLength += 1;

	/*
		Compression is not always a win. Below roughly 12 pixels most glyphs encode
		larger than they store, because antialiased edges leave few runs to find,
		and a thin upright like 'l' or '!' costs more compressed at any size. Keep
		whichever is smaller: Poco blits both, and the format says which.

		Unpacked rows are addressed by width >> 1 with a fixed nybble phase, so the
		width is rounded up to a whole byte. The added column is transparent.
	*/
	entry->flags |= kEntryHasBits;
	if (!cfe->overflowed && (cfe->outLength <= rawLength)) {
#if MODDEF_CFE_FT_STATS
		cfe->statPacked += 1;
#endif
		entry->length = cfe->outLength;
		entry->flags |= kEntryPacked;
		if (!frameBlock)
			cacheTrim(cfe, dataOffset, sizeof(CFEEntryRecord) + cfe->outLength);
	}
	else {
#if MODDEF_CFE_FT_STATS
		cfe->statRaw += 1;
#endif
		c_memcpy(entryPixels(entry), cfe->bitmap, rawLength);
		entry->length = rawLength;
		entry->w = cfe->bitmapRowBytes << 1;
	}

	c_free(cfe->row);
	cfe->row = C_NULL;
	c_free(cfe->bitmap);
	cfe->bitmap = C_NULL;

	return entry;
}

/*
	FreeType allocator
*/

static void *ftAllocCFE(FT_Memory memory, long size)
{
	return c_malloc(size);
}

static void ftFreeCFE(FT_Memory memory, void *block)
{
	if (block)
		c_free(block);
}

static void *ftReallocCFE(FT_Memory memory, long cur_size, long new_size, void *block)
{
	return c_realloc(block, new_size);
}
