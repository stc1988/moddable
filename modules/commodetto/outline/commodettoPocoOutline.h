#include "commodettoPoco.h"

struct PocoOutlineRecord {
	uint16_t	n_points;
	uint16_t	n_contours;
	uint16_t	flags;
	uint8_t		cboxValid;
	uint8_t		reserved;
#if (90 == kPocoRotation) || (180 == kPocoRotation) || (270 == kPocoRotation)
	uint16_t	rw;
	uint16_t	rh;
#endif

	// CBox as integers (no fractional part)
	int16_t		xMin;
	int16_t		yMin;
	uint16_t	w;
	uint16_t	h;
	

	// points as FT_Vector_, contours as uint16_t, tags as uint8_t
};

typedef struct PocoOutlineRecord PocoOutlineRecord;
typedef struct PocoOutlineRecord *PocoOutline;

extern void PocoOutlineFill(Poco poco, PocoColor color, uint8_t blend, PocoOutline pOutline, PocoCoordinate dx, PocoCoordinate dy);

// declared at file scope so that these are the FreeType types, not new types
// scoped to the prototypes below (this header is included before FreeType's)
struct FT_MemoryRec_;
struct FT_Outline_;
struct FT_BBox_;

// FreeType allocator shared by the rasterizer and the stroker. Static storage, so
// objects that outlive the call that created them (the rasterizer) stay valid.
extern struct FT_MemoryRec_ *PocoOutlineFTMemory(void);

// An outline buffer is a PocoOutlineRecord followed by n_points FT_Vector, then
// n_contours uint16_t, then n_points tag bytes. FT_Pos is the size of a long, so
// it is 4 bytes on a microcontroller and 8 on a 64-bit host: never assume either.
#define PocoOutlineByteLength(n_points, n_contours) \
	(sizeof(PocoOutlineRecord) + ((n_points) * 2 * sizeof(FT_Pos)) + ((n_contours) * 2) + (n_points))

extern void PocoOutlineGetCBox(const struct FT_Outline_ *outline, struct FT_BBox_ *box);
extern void PocoOutlineCalculateCBox(PocoOutline pOutline);
#if (90 == kPocoRotation) || (180 == kPocoRotation) || (270 == kPocoRotation)
extern void PocoOutlineRotate(PocoOutline pOutline, int w, int h);
extern void PocoOutlineUnrotate(PocoOutline pOutline);
#endif
