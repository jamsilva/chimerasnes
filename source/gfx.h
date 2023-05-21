#ifndef CHIMERASNES_GFX_H_
#define CHIMERASNES_GFX_H_

#include "port.h"
#include "ppu.h"
#include "chisnes.h"

#include <retro_inline.h>

void StartScreenRefresh();
void DrawScanLine(uint8_t Line);
void EndScreenRefresh();
void SetupOBJ();
void UpdateScreen();
void RenderLine(uint8_t line);
void BuildDirectColourMaps();
bool InitGFX();
void DeinitGFX();

typedef struct
{
	bool      Pseudo     : 1;
	int16_t   _SGFX_PAD1 : 15;
	uint8_t   Z1;        /* Depth for comparison */
	uint8_t   Z2;        /* Depth to save */
	uint8_t   OBJVisibleTiles[128];
	uint8_t   OBJWidths[128];
	uint8_t   r212c;
	uint8_t   r212d;
	uint8_t   r2130;
	uint8_t   r2131;
	int32_t   Delta;
	uint32_t  FixedColour;
	uint32_t  Mode7Mask;
	uint32_t  Mode7PriorityMask;
	uint32_t  PixSize;
	uint32_t  PPL;       /* Number of pixels on each of Screen buffers */
	uint32_t  PPLx2;
	uint32_t  StartY;
	uint32_t  EndY;
	uint32_t  Pitch;
	uint32_t  RealPitch; /* True pitch of Screen buffer. */
	uint32_t  ZPitch;    /* Pitch of ZBuffer */
	ptrdiff_t DepthDelta;
	uint8_t*  DB;
	uint8_t*  S;
	uint8_t*  Screen;
	uint8_t*  Screen_buffer;
	uint8_t*  SubScreen;
	uint8_t*  SubScreen_buffer;
	uint8_t*  SubZBuffer;
	uint8_t*  SubZBuffer_buffer;
	uint8_t*  ZBuffer;
	uint8_t*  ZBuffer_buffer;
	uint16_t* Zero;
	ClipData* pCurrentClip;

	struct
	{
		uint8_t RTOFlags;
		/* With _OBJLines_PAD1 above RTOFlags graphical glitches are more common...
		 * There may be some non-obvious memory corruption here... This order
		 * shouldn't fix things but seems to make the glitches less likely. */
		int8_t _OBJLines_PAD1 : 8;
		int16_t Tiles;

		struct
		{
			int8_t  Sprite;
			uint8_t Line;
		} OBJ[32];
	} OBJLines[SNES_HEIGHT_EXTENDED];
} SGFX;

extern SGFX GFX;

typedef struct
{
	struct
	{
		uint16_t HOffset;
		uint16_t VOffset;
	} BG[4];
} SLineData;

#define H_FLIP     0x4000
#define V_FLIP     0x8000
#define BLANK_TILE 2

typedef struct
{
	bool     DirectColourMode : 1;
	int32_t _SBG_PAD1         : 31;
	uint32_t BitShift;
	uint8_t* Buffer;
	uint8_t* Buffered;
	uint32_t NameSelect;
	uint32_t PaletteMask;
	uint32_t PaletteShift;
	uint32_t SCBase;
	uint32_t StartPalette;
	uint32_t TileAddress;
	uint32_t TileSize;
	uint32_t TileShift;
} SBG;

typedef struct
{
	int16_t CentreX;
	int16_t CentreY;
	int16_t MatrixA;
	int16_t MatrixB;
	int16_t MatrixC;
	int16_t MatrixD;
} SLineMatrixData;

extern uint32_t even_high[4][16];
extern uint32_t even_low[4][16];
extern uint32_t odd_high[4][16];
extern uint32_t odd_low[4][16];
extern SBG      BG;
extern uint16_t DirectColourMaps[8][256];
extern uint8_t  brightness_cap[64];
extern uint8_t  mul_brightness[16][32];

#define SUB_SCREEN_DEPTH  0
#define MAIN_SCREEN_DEPTH 32

static INLINE uint16_t COLOR_ADD(uint16_t C1, uint16_t C2)
{
	return ((brightness_cap[(C1 >> RED_SHIFT_BITS) + (C2 >> RED_SHIFT_BITS) ] << RED_SHIFT_BITS) |
	        (brightness_cap[((C1 >> GREEN_SHIFT_BITS) & 0x1f) + ((C2 >> GREEN_SHIFT_BITS) & 0x1f)] << GREEN_SHIFT_BITS) |
	        (brightness_cap[(C1 & 0x1f) + (C2 & 0x1f)]));
}

#define COLOR_ADD1_2(C1, C2)                   \
	(((((C1) & RGB_REMOVE_LOW_BITS_MASK) +     \
	((C2) & RGB_REMOVE_LOW_BITS_MASK)) >> 1) + \
	(((C1) & (C2) & RGB_LOW_BITS_MASK) | ALPHA_BITS_MASK))

static INLINE uint16_t COLOR_SUB(uint16_t C1, uint16_t C2)
{
	int32_t rb1         = (C1 & (THIRD_COLOR_MASK | FIRST_COLOR_MASK)) | ((0x20 << 0) | (0x20 << RED_SHIFT_BITS));
	int32_t rb2         = C2 & (THIRD_COLOR_MASK | FIRST_COLOR_MASK);
	int32_t rb          = rb1 - rb2;
	int32_t rbcarry     = rb & ((0x20 << RED_SHIFT_BITS) | (0x20 << 0));
	int32_t g           = ((C1 & (SECOND_COLOR_MASK)) | (0x20 << GREEN_SHIFT_BITS)) - (C2 & (SECOND_COLOR_MASK));
	int32_t rgbsaturate = (((g & (0x20 << GREEN_SHIFT_BITS)) | rbcarry) >> 5) * 0x1f;
	uint16_t retval     = ((rb & (THIRD_COLOR_MASK | FIRST_COLOR_MASK)) | (g & SECOND_COLOR_MASK)) & rgbsaturate;
	return (retval & 0x0400) >> 5;
}

#define COLOR_SUB1_2(C1, C2)                \
	GFX.Zero[(((C1) | RGB_HI_BITS_MASKx2) - \
	          ((C2) & RGB_REMOVE_LOW_BITS_MASK)) >> 1]

typedef void (*NormalTileRenderer)(uint32_t Tile, int32_t Offset, uint32_t StartLine, uint32_t LineCount);
typedef void (*ClippedTileRenderer)(uint32_t Tile, int32_t Offset, uint32_t StartPixel, uint32_t Width, uint32_t StartLine, uint32_t LineCount);
typedef void (*LargePixelRenderer)(uint32_t Tile, int32_t Offset, uint32_t StartPixel, uint32_t Pixels, uint32_t StartLine, uint32_t LineCount);
#endif
