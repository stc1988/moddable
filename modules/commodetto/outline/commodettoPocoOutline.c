#include "xsmc.h"
#include "mc.xs.h"
#include "xsHost.h"

#include "mc.defines.h"

#include "commodettoPoco.h"
#include "commodettoPocoOutline.h"

#include <ft2build.h>
#include FT_CONFIG_CONFIG_H
#include <freetype/ftimage.h>
#include <freetype/ftsystem.h>

// FreeType 2.14 changed FT_Span.x from short to unsigned short: rendering into a
// bitmap never sees a negative x. Poco renders outlines in outline space, so a
// translated outline does put spans at negative x. Read the field back as signed.
#define PocoSpanX(spans) ((int16_t)(spans)->x)

// ftgrays.h lives in the FreeType source tree rather than its include tree, so the
// rasterizer is declared here instead of putting that directory on the include path.
extern const FT_Raster_Funcs ft_grays_raster;

typedef struct {
	uint8_t		*bits;
	uint32_t	pitch;
} xsSpanInfoRecord, *xsSpanInfo;

typedef struct {
	FT_Raster raster;
	FT_Raster_Params params;
	int pitch;
} xsOutlineRendererRecord, *xsOutlineRenderer;

static void doOutline(Poco poco, uint8_t *refcon, PocoPixel *dst, PocoDimension w, PocoDimension h, uint8_t xphase);
static void doPolygon(Poco poco, uint8_t *refcon, PocoPixel *dst, PocoDimension w, PocoDimension h, uint8_t xphase);
static void doOutlineOpaqueSpan(int y, int count, const FT_Span *spans, void *user);
static void doOutlineBlendSpan(int y, int count, const FT_Span *spans, void *user);

static xsOutlineRenderer gxOutlineRenderer = NULL;

static void *ftAlloc(FT_Memory memory, long size)
{
	return c_malloc(size);
}

static void ftFree(FT_Memory memory, void *block)
{
	if (block)
		c_free(block);
}

static void *ftRealloc(FT_Memory memory, long cur_size, long new_size, void *block)
{
	return c_realloc(block, new_size);
}

/*
	The control box is the bounding box of the outline's points. FreeType provides
	FT_Outline_Get_CBox, but it lives in ftoutln.c alongside the functions that pull
	in the object layer. This is the same calculation, kept here so that projects
	which draw outlines but use no font engine link only the rasterizer.
*/
void PocoOutlineGetCBox(const FT_Outline *outline, FT_BBox *box)
{
	FT_Pos xMin, yMin, xMax, yMax;
	const FT_Vector *point = outline->points;
	const FT_Vector *end = point + outline->n_points;

	if (outline->n_points <= 0) {
		box->xMin = box->yMin = box->xMax = box->yMax = 0;
		return;
	}

	xMin = xMax = point->x;
	yMin = yMax = point->y;

	for (point += 1; point < end; point++) {
		FT_Pos x = point->x, y = point->y;

		if (x < xMin) xMin = x;
		else if (x > xMax) xMax = x;

		if (y < yMin) yMin = y;
		else if (y > yMax) yMax = y;
	}

	box->xMin = xMin;
	box->yMin = yMin;
	box->xMax = xMax;
	box->yMax = yMax;
}

struct FT_MemoryRec_ *PocoOutlineFTMemory(void)
{
	static struct FT_MemoryRec_ memory;

	if (NULL == memory.alloc) {
		memory.alloc = ftAlloc;
		memory.free = ftFree;
		memory.realloc = ftRealloc;
	}

	return &memory;
}

xsOutlineRenderer PocoOutlineRenderer()
{
	if (gxOutlineRenderer == NULL) {
		xsOutlineRenderer or = c_calloc(1, sizeof(xsOutlineRendererRecord));
		if (NULL == or)
			return NULL;
		or->params.flags = FT_RASTER_FLAG_AA | FT_RASTER_FLAG_DIRECT | FT_RASTER_FLAG_CLIP;
		// the cell pool is statically allocated inside ftgrays, so raster_reset is a no-op
		if (ft_grays_raster.raster_new(PocoOutlineFTMemory(), &or->raster)) {
			c_free(or);
			return NULL;
		}
		gxOutlineRenderer = or;
	}
	return gxOutlineRenderer;
}

typedef struct {
	xsOutlineRenderer or;
	struct FT_Outline_ outline;
	PocoCoordinate x;
	PocoCoordinate y;
	PocoCoordinate dx;
	PocoPixel color;
	uint8_t blend;
} xsOutlineRenderRecord, *xsOutlineRender;

void bufferToFTOutline(void *buffer, struct FT_Outline_ *outline)
{
	PocoOutline header = (PocoOutline)buffer;
	outline->n_points = header->n_points;
	outline->n_contours = header->n_contours;
	outline->points = (struct FT_Vector_ *)(((unsigned char *)buffer) + sizeof(PocoOutlineRecord));
	outline->contours = (unsigned short *)(((unsigned char *)outline->points) + (outline->n_points * 2 * sizeof(FT_Pos)));
	outline->tags = ((unsigned char *)outline->contours) + (outline->n_contours * 2);
	outline->flags = header->flags;

//	int i;
//	printf("READ n_points %d, n_contours %d\n", (int)outline->n_points, (int)outline->n_contours);
//	for (i = 0; i < outline->n_points; i++)
//		printf("x %f, y %f, tag %d\n", (double)outline->points[i].x / 64, (double)outline->points[i].y / 64, (int)outline->tags[i]);
}

void xs_outlinerenderer_blendOutline(xsMachine *the)
{
	Poco poco = xsmcGetHostDataPoco(xsThis);
	xsOutlineRenderer or = PocoOutlineRenderer();
	PocoOutline buffer = (PocoOutline)xsmcGetHostData(xsArg(2));
	xsOutlineRenderRecord orr;
	PocoCoordinate xMax, yMax, dx, dy;

	if (NULL == or)
		xsUnknownError("no memory");

	orr.or = or;
	orr.color = (PocoPixel)xsmcToInteger(xsArg(0));
	orr.blend = (uint8_t)xsmcToInteger(xsArg(1));
	if (xsmcArgc >= 4) {
		dx = xsmcToInteger(xsArg(3));
		dy = xsmcToInteger(xsArg(4));
	}
	else {
		dx = 0;
		dy = 0;
	}
	bufferToFTOutline(buffer, &orr.outline);
#if (90 == kPocoRotation) || (180 == kPocoRotation) || (270 == kPocoRotation)
	PocoOutlineRotate(buffer, poco->width, poco->height);
	#if (90 == kPocoRotation)
		int t = dx;
	 	dx = -dy;
	 	dy = t;
	#elif (180 == kPocoRotation)
	 	dx = -dx;
	 	dy = -dy;
	 #else
		int t = dx;
	 	dx = dy;
	 	dy = -t;
	#endif 	
#endif
	PocoOutlineCalculateCBox(buffer);
	orr.dx = dx;

	int x = buffer->xMin + orr.dx;
	int y = buffer->yMin + dy;
	int w = buffer->w;
	int h = buffer->h;

	xMax = x + w;
	yMax = y + h;

	if (x < poco->x)
		x = poco->x;

	if (xMax > poco->xMax)
		xMax = poco->xMax;

	if (x >= xMax)
		return;

	w = xMax - x;

	if (y < poco->y)
		y = poco->y;

	if (yMax > poco->yMax)
		yMax = poco->yMax;

	if (y >= yMax)
		return;

	h = yMax - y;

	orr.x = x - orr.dx;
	orr.y = y - dy;
	PocoDrawExternal(poco, doOutline, (void *)&orr, sizeof(orr), x, y, w, h);
	
	PocoHold(the, poco, xsmcToReference(xsArg(2)));
}


typedef struct {
	PocoPixel		*dst;
	int16_t			rowBytes;
	PocoCoordinate	x;
	PocoCoordinate	y;
	PocoPixel 		color;
	uint8_t 		blend;
#if 4 == kPocoPixelSize
	uint8_t 		xphase;
#endif
} xsOutlineSpanRecord, *xsOutlineSpan;

void doOutline(Poco poco, uint8_t *refcon, PocoPixel *dst, PocoDimension w, PocoDimension h, uint8_t xphase)
{
	xsOutlineRender orr = (xsOutlineRender)refcon;
	xsOutlineRenderer or = orr->or;
	xsOutlineSpanRecord os;

	os.dst = dst;
	os.rowBytes = poco->rowBytes;
	os.x = orr->x;
	os.y = orr->y;
	os.color = orr->color;
	os.blend = orr->blend;
#if 4 == kPocoPixelSize
	os.xphase = xphase;
#endif

	or->params.user = &os;
	or->params.clip_box.xMin = orr->x; //poco->x - orr->dx;
	or->params.clip_box.yMin = orr->y;
	or->params.clip_box.xMax = or->params.clip_box.xMin + w; //poco->w;
	or->params.clip_box.yMax = or->params.clip_box.yMin + h;

	or->params.source = &orr->outline;
	or->params.gray_spans = (255 == os.blend) ? doOutlineOpaqueSpan : doOutlineBlendSpan;

	ft_grays_raster.raster_render(or->raster, &or->params);

	orr->y += h;
}

typedef struct {
	xsOutlineRenderer or;
	PocoCoordinate x;
	PocoCoordinate y;
	short n_points;
	PocoPixel color;
	uint8_t blend;
	FT_Pos points[16 * 2];
} xsPolygonRenderRecord, *xsPolygonRender;

const char gPolygonFlags[16] = {FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON,
						FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON};

void xs_outlinerenderer_blendPolygon(xsMachine *the)
{
	Poco poco = xsmcGetHostDataPoco(xsThis);
	xsOutlineRenderer or = PocoOutlineRenderer();
	xsPolygonRenderRecord prr;
	PocoCoordinate xMax, yMax;
	int i;
	int argc = xsmcArgc;

	if (NULL == or)
		xsUnknownError("no memory");

	prr.or = or;
	prr.color = (PocoPixel)xsmcToInteger(xsArg(0));
	prr.blend = (uint8_t)xsmcToInteger(xsArg(1));
	prr.n_points = (argc - 2);
	if ((prr.n_points >= 32) || (prr.n_points <= 0))
		xsUnknownError("too many points");
	if (1 == prr.n_points) {
		xsmcGet(xsResult, xsArg(3), xsID_length);
		prr.n_points = xsmcToInteger(xsResult);
		if ((prr.n_points >= 32) || (prr.n_points <= 0))
			xsUnknownError("too many points");
		for (i = 0; i < prr.n_points; i++) {
			xsmcGetIndex(xsResult, xsArg(3), i);
			prr.points[i] = xsmcToInteger(xsResult) << 6;
		}
		prr.n_points >>= 1;
	}
	else {
		for (i = 0; i < prr.n_points; i++)
			prr.points[i] = xsmcToInteger(xsArg(2 + i)) << 6;
		prr.n_points >>= 1;
	}
	prr.n_points -= 1;

	FT_BBox box;
	FT_Outline outline = {
    	.n_contours = 1,
    	.n_points = prr.n_points + 1,
    	.points = (FT_Vector *)prr.points,
    	.tags = (unsigned char *)gPolygonFlags,
    	.contours = (unsigned short *)&prr.n_points,
    	.flags = FT_OUTLINE_NONE
	};
	PocoOutlineGetCBox(&outline, &box);

	int x = box.xMin >> 6;
	int y = box.yMin >> 6;
	int w = ((box.xMax + 63) >> 6) - x;
	int h = ((box.yMax + 63) >> 6) - y;

	xMax = x + w;
	yMax = y + h;

	if (x < poco->x)
		x = poco->x;

	if (xMax > poco->xMax)
		xMax = poco->xMax;

	if (x >= xMax)
		return;

	w = xMax - x;

	if (y < poco->y)
		y = poco->y;

	if (yMax > poco->yMax)
		yMax = poco->yMax;

	if (y >= yMax)
		return;

	h = yMax - y;

	prr.x = x;
	prr.y = y;
	PocoDrawExternal(poco, doPolygon, (void *)&prr, offsetof(xsPolygonRenderRecord, points) + ((1 + prr.n_points) * 2 * sizeof(FT_Pos)), x, y, w, h);
}

void doPolygon(Poco poco, uint8_t *refcon, PocoPixel *dst, PocoDimension w, PocoDimension h, uint8_t xphase)
{
	xsPolygonRender prr = (xsPolygonRender)refcon;
	xsOutlineRenderer or = prr->or;
	xsOutlineSpanRecord os;
	FT_Outline outline = {
    	.n_contours = 1,
    	.n_points = prr->n_points + 1,
    	.points = (FT_Vector *)prr->points,
    	.tags = (unsigned char *)gPolygonFlags,
    	.contours = (unsigned short *)&prr->n_points,
    	.flags = FT_OUTLINE_NONE,		// FT_OUTLINE_EVEN_ODD_FILL
	};

	os.dst = dst;
	os.rowBytes = poco->rowBytes;
	os.x = prr->x;
	os.y = prr->y;
	os.color = prr->color;
	os.blend = prr->blend;
#if 4 == kPocoPixelSize
	os.xphase = xphase;
#endif

	or->params.user = &os;
	or->params.clip_box.xMin = poco->x;
	or->params.clip_box.yMin = prr->y;
	or->params.clip_box.xMax = poco->x + poco->w;
	or->params.clip_box.yMax = prr->y + h;

	or->params.source = &outline;
	or->params.gray_spans = (255 == os.blend) ? doOutlineOpaqueSpan : doOutlineBlendSpan;

	ft_grays_raster.raster_render(or->raster, &or->params);

	prr->y += h;
}

#if 0
void doOutlineOpaqueSpan(int y, int count, const FT_Span *spans, void *user)
{
	xsOutlineSpan os = user;
	PocoPixel *pixels = (PocoPixel *)(((uint8_t *)os->dst) + ((y - os->y) * os->rowBytes));

	do {
		uint16_t len = spans->len;
		PocoPixel *p = pixels + (PocoSpanX(spans) - os->x);
		uint16_t c = spans->coverage >> 3;
		uint16_t pixel = (c << 11) | (c << 6) | c;

		while (len--)
			*p++ = pixel;

		spans++;
	} while (--count);
}

#elif kCommodettoBitmapRGB565LE == kPocoPixelFormat

void doOutlineOpaqueSpan(int y, int count, const FT_Span *spans, void *user)
{
	xsOutlineSpan os = user;
	PocoPixel *pixels = (PocoPixel *)(((uint8_t *)os->dst) + ((y - os->y) * os->rowBytes));
	int src32 = os->color;
	src32 |= src32 << 16;
	src32 &= 0x07E0F81F;

	pixels -= os->x;
	do {
		uint16_t len = spans->len;
		PocoPixel *p = pixels + PocoSpanX(spans);
		uint8_t blend = spans->coverage;

		if (255 == blend) {
			uint16_t pixel = os->color;
			while (len--)
				*p++ = pixel;
		}
		else {
			blend >>= 3;

			while (len--) {
				int	dst, src;

				dst = *p;
				dst |= dst << 16;
				dst &= 0x07E0F81F;
				src = src32 - dst;
				dst = blend * src + (dst << 5) - dst;
				dst += 0x02008010;
				dst += (dst >> 5) & 0x07E0F81F;
				dst >>= 5;
				dst &= 0x07E0F81F;
				dst |= dst >> 16;
				*p++ = (PocoPixel)dst;
			}
		}

		spans++;
	} while (--count);
}

#elif kCommodettoBitmapRGB565BE == kPocoPixelFormat

void doOutlineOpaqueSpan(int y, int count, const FT_Span *spans, void *user)
{
	xsOutlineSpan os = user;
	PocoPixel *pixels = (PocoPixel *)(((uint8_t *)os->dst) + ((y - os->y) * os->rowBytes));
	int src32 = commodetto_bswap16(os->color);
	src32 |= src32 << 16;
	src32 &= 0x07E0F81F;

	pixels -= os->x;
	do {
		uint16_t len = spans->len;
		PocoPixel *p = pixels + PocoSpanX(spans);
		uint8_t blend = spans->coverage;

		if (255 == blend) {
			uint16_t pixel = os->color;
			while (len--)
				*p++ = pixel;
		}
		else {
			blend >>= 3;

			while (len--) {
				int	dst, src;

				dst = commodetto_bswap16(*p);
				dst |= dst << 16;
				dst &= 0x07E0F81F;
				src = src32 - dst;
				dst = blend * src + (dst << 5) - dst;
				dst += 0x02008010;
				dst += (dst >> 5) & 0x07E0F81F;
				dst >>= 5;
				dst &= 0x07E0F81F;
				dst |= dst >> 16;
				*p++ = (PocoPixel)commodetto_bswap16((uint16_t)dst);
			}
		}

		spans++;
	} while (--count);
}

#elif kCommodettoBitmapGray16 == kPocoPixelFormat

void doOutlineOpaqueSpan(int y, int count, const FT_Span *spans, void *user)
{
	xsOutlineSpan os = user;
	PocoPixel *pixels = (PocoPixel *)(((uint8_t *)os->dst) + ((y - os->y) * os->rowBytes));
	uint8_t color = os->color;

	do {
		uint16_t len = spans->len;
		int dx = PocoSpanX(spans) - os->x;
		PocoPixel *p = pixels + (dx >> 1);
		uint8_t xphase = os->xphase + (dx & 1);
		if (2 == xphase) {
			xphase = 0;
			p++;
		}
		uint16_t c = spans->coverage;
		if (255 == c) {
			if (len && xphase) {
				uint8_t pixel = *p;
				*p++ = (pixel & 0xF0) | color;
				xphase = 0;
				len -= 1;
			}
			while (len >= 2) {
				*p++ = color | (color << 4);
				len -= 2;
			}
			if (len)
				*p = (*p & 0x0F) | (color << 4);
		}
		else {
			while (len--) {
				uint8_t pixel = *p;
				uint16_t accum;
				if (xphase)
					accum = pixel & 0x0F;
				else
					accum = (pixel & 0xF0) >> 4;
				accum *= (255 - c);
				accum += color * c;
				accum >>= 8;
				if (xphase) {
					*p++ = (pixel & 0xF0) | accum;
					xphase = 0;
				}
				else {
					*p = (pixel & 0x0F) | (accum << 4);
					xphase = 1;
				}
			}
		}

		spans++;
	} while (--count);
}
#elif kCommodettoBitmapGray256 == kPocoPixelFormat

void doOutlineOpaqueSpan(int y, int count, const FT_Span *spans, void *user)
{
	xsOutlineSpan os = user;
	PocoPixel *pixels = (PocoPixel *)(((uint8_t *)os->dst) + ((y - os->y) * os->rowBytes));
	PocoPixel color = os->color;

	pixels -= os->x;
	do {
		uint16_t len = spans->len;
		PocoPixel *p = pixels + PocoSpanX(spans);
		uint8_t blend = spans->coverage >> 3;

		if (31 == blend) {
			while (len--)
				*p++ = color;
		}
		else {
			uint16_t pixel = color * blend;

			blend = 31 - blend;
			while (len--) {
				uint16_t t = (*p * blend) + pixel;
				*p++ = t >> 5;
			}
		}

		spans++;
	} while (--count);
}

#else
	#error unsupported outline pixel format
#endif

#if kCommodettoBitmapRGB565LE == kPocoPixelFormat

void doOutlineBlendSpan(int y, int count, const FT_Span *spans, void *user)
{
	xsOutlineSpan os = user;
	PocoPixel *pixels = (PocoPixel *)(((uint8_t *)os->dst) + ((y - os->y) * os->rowBytes));
	int src32 = os->color;
	src32 |= src32 << 16;
	src32 &= 0x07E0F81F;

	pixels -= os->x;
	do {
		uint16_t len = spans->len;
		PocoPixel *p = pixels + PocoSpanX(spans);
		uint8_t blend = (spans->coverage * os->blend) >> 11;

		while (len--) {
			int	dst, src;

			dst = *p;
			dst |= dst << 16;
			dst &= 0x07E0F81F;
			src = src32 - dst;
			dst = blend * src + (dst << 5) - dst;
			dst += 0x02008010;
			dst += (dst >> 5) & 0x07E0F81F;
			dst >>= 5;
			dst &= 0x07E0F81F;
			dst |= dst >> 16;
			*p++ = (PocoPixel)dst;
		}

		spans++;
	} while (--count);
}

#elif kCommodettoBitmapRGB565BE == kPocoPixelFormat

void doOutlineBlendSpan(int y, int count, const FT_Span *spans, void *user)
{
	xsOutlineSpan os = user;
	PocoPixel *pixels = (PocoPixel *)(((uint8_t *)os->dst) + ((y - os->y) * os->rowBytes));
	int src32 = commodetto_bswap16(os->color);
	src32 |= src32 << 16;
	src32 &= 0x07E0F81F;

	pixels -= os->x;
	do {
		uint16_t len = spans->len;
		PocoPixel *p = pixels + PocoSpanX(spans);
		uint8_t blend = (spans->coverage * os->blend) >> 11;

		while (len--) {
			int	dst, src;

			dst = commodetto_bswap16(*p);
			dst |= dst << 16;
			dst &= 0x07E0F81F;
			src = src32 - dst;
			dst = blend * src + (dst << 5) - dst;
			dst += 0x02008010;
			dst += (dst >> 5) & 0x07E0F81F;
			dst >>= 5;
			dst &= 0x07E0F81F;
			dst |= dst >> 16;
			*p++ = (PocoPixel)commodetto_bswap16((uint16_t)dst);
		}

		spans++;
	} while (--count);
}

#elif kCommodettoBitmapGray16 == kPocoPixelFormat

void doOutlineBlendSpan(int y, int count, const FT_Span *spans, void *user)
{
	xsOutlineSpan os = user;
	PocoPixel *pixels = (PocoPixel *)(((uint8_t *)os->dst) + ((y - os->y) * os->rowBytes));
	uint8_t color = os->color;

	do {
		uint16_t len = spans->len;
		int dx = PocoSpanX(spans) - os->x;
		PocoPixel *p = pixels + (dx >> 1);
		uint8_t xphase = os->xphase + (dx & 1);
		if (2 == xphase) {
			xphase = 0;
			p++;
		}
		uint16_t c = (spans->coverage * os->blend) >> 8;
		uint16_t cc = color * c;
		while (len--) {
			uint8_t pixel = *p;
			uint16_t accum;
			if (xphase)
				accum = pixel & 0x0F;
			else
				accum = (pixel & 0xF0) >> 4;
			accum *= (255 - c);
			accum += cc;
			accum >>= 8;
			if (xphase) {
				*p++ = (pixel & 0xF0) | accum;
				xphase = 0;
			}
			else {
				*p = (pixel & 0x0F) | (accum << 4);
				xphase = 1;
			}
		}

		spans++;
	} while (--count);
}

#elif kCommodettoBitmapGray256 == kPocoPixelFormat

void doOutlineBlendSpan(int y, int count, const FT_Span *spans, void *user)
{
	xsOutlineSpan os = user;
	PocoPixel *pixels = (PocoPixel *)(((uint8_t *)os->dst) + ((y - os->y) * os->rowBytes));
	PocoPixel color = os->color;

	pixels -= os->x;
	do {
		uint16_t len = spans->len;
		PocoPixel *p = pixels + PocoSpanX(spans);
		uint8_t blend = spans->coverage >> 3;
		uint16_t pixel = color * blend;

		blend = 31 - blend;
		while (len--) {
			uint16_t t = (*p * blend) + pixel;
			*p++ = t >> 5;
		}

		spans++;
	} while (--count);
}

#else
	#error unsupported outline pixel format
#endif


void PocoOutlineFill(Poco poco, PocoColor color, uint8_t blend, PocoOutline pOutline, PocoCoordinate dx, PocoCoordinate dy)
{
	xsOutlineRenderRecord orr;
	PocoCoordinate xMax, yMax;
	orr.or = PocoOutlineRenderer();
	if (NULL == orr.or)
		return;
	orr.color = color;
	orr.blend = blend;
	bufferToFTOutline(pOutline, &orr.outline);
	
#if (90 == kPocoRotation) || (180 == kPocoRotation) || (270 == kPocoRotation)
	PocoOutlineRotate(pOutline, poco->width, poco->height);
	#if (90 == kPocoRotation)
		int t = dx;
	 	dx = -dy;
	 	dy = t;
	#elif (180 == kPocoRotation)
	 	dx = -dx;
	 	dy = -dy;
	 #else
		int t = dx;
	 	dx = dy;
	 	dy = -t;
	#endif 	
#endif
	PocoOutlineCalculateCBox(pOutline);
	orr.dx = dx;

	int x = pOutline->xMin + dx;
	int y = pOutline->yMin + dy;
	int w = pOutline->w;
	int h = pOutline->h;

	xMax = x + w;
	yMax = y + h;

	if (x < poco->x)
		x = poco->x;

	if (xMax > poco->xMax)
		xMax = poco->xMax;

	if (x >= xMax)
		return;

	w = xMax - x;

	if (y < poco->y)
		y = poco->y;

	if (yMax > poco->yMax)
		yMax = poco->yMax;

	if (y >= yMax)
		return;

	h = yMax - y;

	orr.x = x - orr.dx;
	orr.y = y - dy;
	PocoDrawExternal(poco, doOutline, (void *)&orr, sizeof(orr), x, y, w, h);
}

