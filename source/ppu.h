#ifndef CHIMERASNES_PPU_H_
#define CHIMERASNES_PPU_H_

#include <stdint.h>
#include <boolean.h>
#include <retro_inline.h>

#define FIRST_VISIBLE_LINE 1

extern uint8_t GetBank;
extern uint16_t SignExtend[2];

#define TILE_2BIT 0
#define TILE_4BIT 1
#define TILE_8BIT 2

#define MAX_2BIT_TILES 4096
#define MAX_4BIT_TILES 2048
#define MAX_8BIT_TILES 1024

#define PPU_H_BEAM_IRQ_SOURCE (1 << 0)
#define PPU_V_BEAM_IRQ_SOURCE (1 << 1)
#define GSU_IRQ_SOURCE        (1 << 2)
#define SA1_DMA_IRQ_SOURCE    (1 << 3)
#define SA1_IRQ_SOURCE        (1 << 4)

typedef struct
{
	uint32_t Count[6];
	uint32_t Left[6][6];
	uint32_t Right[6][6];
} ClipData;

typedef struct
{
	bool     ColorsChanged               : 1;
	bool     DirectColourMapsNeedRebuild : 1;
	bool     DoubleWidthPixels           : 1;
	bool     DoubleHeightPixels          : 1;
	bool     FirstVRAMRead               : 1;
	bool     HalfWidthPixels             : 1;
	bool     Interlace                   : 1;
	bool     OBJChanged                  : 1;
	bool     RenderThisFrame             : 1;
	int8_t   _InternalPPU_PAD1           : 7;
	int8_t   _InternalPPU_PAD2           : 8;
	uint8_t  HDMA;
	uint16_t ScreenColors[256];
	int32_t  Controller;
	int32_t  CurrentLine;
	int32_t  PreviousLine;
	int32_t  RenderedScreenWidth;
	int32_t  RenderedScreenHeight;
	int32_t  PrevMouseX[2];
	int32_t  PrevMouseY[2];
	uint32_t FrameCount;
	uint32_t SuperScope;
	uint32_t Joypads[5];
	uint32_t Mouse[2];
	uint32_t Red[256];
	uint32_t Green[256];
	uint32_t Blue[256];
	int32_t  _InternalPPU_PAD3           : 32;
	uint8_t* XB;
	uint8_t* TileCache[3];
	uint8_t* TileCached[3];
	ClipData Clip[2];
} InternalPPU;

typedef struct
{
	int8_t   _SOBJ_PAD1 : 8;
	uint8_t  Priority;
	uint8_t  Palette;
	uint8_t  Size;
	uint8_t  HFlip;
	uint8_t  VFlip;
	int16_t  HPos;
	uint16_t VPos;
	uint16_t Name;
} SOBJ;

typedef struct
{
	bool     CGFLIP               : 1;
	bool     CGFLIPRead           : 1;
	bool     ForcedBlanking       : 1;
	bool     Mode7HFlip           : 1;
	bool     Mode7VFlip           : 1;
	bool     Need16x8Multiply     : 1;
	bool     RecomputeClipWindows : 1;
	bool     HTimerEnabled        : 1;
	bool     VTimerEnabled        : 1;
	int8_t   _SPPU_PAD1           : 7;
	int8_t   _SPPU_PAD2           : 8;
	int8_t   _SPPU_PAD3           : 8;
	int8_t   _SPPU_PAD4           : 8;
	bool     BGMosaic[4];
	bool     ClipWindow1Inside[6];
	bool     ClipWindow2Inside[6];
	uint8_t  BGMode;
	uint8_t  BG3Priority;
	uint8_t  Brightness;
	uint8_t  BGnxOFSbyte;
	uint8_t  CGADD;
	uint8_t  CGSavedByte;
	uint8_t  FirstSprite;
	uint8_t  FixedColourRed;
	uint8_t  FixedColourGreen;
	uint8_t  FixedColourBlue;
	uint8_t  HBeamFlip;
	uint8_t  VBeamFlip;
	uint8_t  Joypad1ButtonReadPos;
	uint8_t  Joypad2ButtonReadPos;
	uint8_t  Joypad3ButtonReadPos;
	uint8_t  Mode7Repeat;
	uint8_t  Mosaic;
	uint8_t  OAMFlip;
	uint8_t  OAMPriorityRotation;
	uint8_t  OBJSizeSelect;
	uint8_t  OpenBus1;
	uint8_t  OpenBus2;
	uint8_t  RangeTimeOver;
	uint8_t  Window1Left;
	uint8_t  Window1Right;
	uint8_t  Window2Left;
	uint8_t  Window2Right;
	uint8_t  ClipWindow1Enable[6];
	uint8_t  ClipWindow2Enable[6];
	uint8_t  ClipWindowOverlapLogic[6];
	uint8_t  OAMData[512 + 32];
	int16_t  CentreX;
	int16_t  CentreY;
	int16_t  HTimerPosition;
	int16_t  MatrixA;
	int16_t  MatrixB;
	int16_t  MatrixC;
	int16_t  MatrixD;
	uint16_t HBeamPosLatched;
	uint16_t VBeamPosLatched;
	uint16_t IRQHBeamPos;
	uint16_t IRQVBeamPos;
	uint16_t OAMAddr;
	uint16_t OBJNameBase;
	uint16_t OBJNameSelect;
	uint16_t OAMWriteRegister;
	uint16_t SavedOAMAddr;
	uint16_t ScreenHeight;
	uint16_t CGDATA[256];
	uint32_t WRAM;
	SOBJ     OBJ[128];

	struct
	{
		bool     High     : 1;
		int8_t   _VMA_PAD : 7;
		uint8_t  Increment;
		uint16_t Address;
		uint16_t FullGraphicCount;
		uint16_t Mask1;
		uint16_t Shift;
	} VMA;

	struct
	{
		int8_t   _SPPU_PAD1 : 8;
		uint8_t  BGSize;
		uint16_t SCBase;
		uint16_t SCSize;
		uint16_t HOffset;
		uint16_t VOffset;
		uint16_t NameBase;
	} BG[4];
} SPPU;

#define CLIP_OR 0
#define CLIP_AND 1
#define CLIP_XOR 2
#define CLIP_XNOR 3

typedef struct
{
	bool     AAddressDecrement      : 1;
	bool     AAddressFixed          : 1;
	bool     FirstLine              : 1;
	bool     HDMAIndirectAddressing : 1;
	bool     Repeat                 : 1;
	bool     ReverseTransfer        : 1;
	int8_t   _SDMA_PAD1             : 2;
	uint8_t  ABank;
	uint8_t  BAddress;
	uint8_t  IndirectBank;
	uint8_t  LineCount;
	uint8_t  TransferMode;
	uint16_t Address;
	uint16_t AAddress;
	uint16_t IndirectAddress;
	uint16_t TransferBytes;
} SDMA;

void    UpdateScreen();
void    ResetPPU();
void    SoftResetPPU();
void    FixColourBrightness();
void    SuperFXExec();
void    SetPPU(uint8_t Byte, uint16_t Address);
uint8_t GetPPU(uint16_t Address);
void    SetCPU(uint8_t Byte, uint16_t Address);
uint8_t GetCPU(uint16_t Address);
void    InitCX4();
void    SetCX4(uint8_t Byte, uint16_t Address);
uint8_t GetCX4(uint16_t Address);
void    SetCX4RAM(uint8_t Byte, uint16_t Address);
uint8_t GetCX4RAM(uint16_t Address);
void    UpdateJoypads();
void    ProcessMouse(int32_t which1);
void    JustifierButtons(uint32_t* justifiers);
bool    JustifierOffscreen();

extern SDMA        DMA[8];
extern SPPU        PPU;
extern InternalPPU IPPU;

#include "memmap.h"

typedef struct
{
	uint8_t _5C77;
	uint8_t _5C78;
	uint8_t _5A22;
} SnesModel;

extern SnesModel* Model;
extern SnesModel M1SNES;
extern SnesModel M2SNES;

enum
{
	MAX_5C77_VERSION = 0x01,
	MAX_5A22_VERSION = 0x02,
	MAX_5C78_VERSION = 0x03
};

static INLINE void FLUSH_REDRAW()
{
	if (IPPU.PreviousLine != IPPU.CurrentLine)
		UpdateScreen();
}

static INLINE void REGISTER_2104(uint8_t byte)
{
	int32_t addr;
	uint8_t lowbyte, highbyte;

	if (PPU.OAMAddr & 0x100)
	{
		addr = ((PPU.OAMAddr & 0x10f) << 1) + (PPU.OAMFlip & 1);

		if (byte != PPU.OAMData[addr])
		{
			SOBJ* pObj;
			FLUSH_REDRAW();
			PPU.OAMData[addr] = byte;
			IPPU.OBJChanged   = true;

			/* X position high bit, and sprite size (x4) */
			pObj         = &PPU.OBJ[(addr & 0x1f) * 4];
			pObj->HPos   = (pObj->HPos & 0xFF) | SignExtend[(byte >> 0) & 1];
			pObj++->Size = byte & 2;
			pObj->HPos   = (pObj->HPos & 0xFF) | SignExtend[(byte >> 2) & 1];
			pObj++->Size = byte & 8;
			pObj->HPos   = (pObj->HPos & 0xFF) | SignExtend[(byte >> 4) & 1];
			pObj++->Size = byte & 32;
			pObj->HPos   = (pObj->HPos & 0xFF) | SignExtend[(byte >> 6) & 1];
			pObj->Size   = byte & 128;
		}

		PPU.OAMFlip ^= 1;

		if (!(PPU.OAMFlip & 1))
		{
			++PPU.OAMAddr;
			PPU.OAMAddr &= 0x1ff;

			if (PPU.OAMPriorityRotation && PPU.FirstSprite != (PPU.OAMAddr >> 1))
			{
				PPU.FirstSprite = (PPU.OAMAddr & 0xfe) >> 1;
				IPPU.OBJChanged = true;
			}
		}
		else if (PPU.OAMPriorityRotation && (PPU.OAMAddr & 1))
			IPPU.OBJChanged = true;

		return;
	}

	if (!(PPU.OAMFlip & 1))
	{
		PPU.OAMWriteRegister &= 0xff00;
		PPU.OAMWriteRegister |= byte;
		PPU.OAMFlip |= 1;

		if (PPU.OAMPriorityRotation && (PPU.OAMAddr & 1))
			IPPU.OBJChanged = true;

		return;
	}

	PPU.OAMWriteRegister &= 0x00ff;
	lowbyte  = (uint8_t) PPU.OAMWriteRegister;
	highbyte = byte;
	PPU.OAMWriteRegister |= byte << 8;
	addr = PPU.OAMAddr << 1;

	if (lowbyte != PPU.OAMData[addr] || highbyte != PPU.OAMData[addr + 1])
	{
		FLUSH_REDRAW();
		PPU.OAMData[addr]     = lowbyte;
		PPU.OAMData[addr + 1] = highbyte;
		IPPU.OBJChanged       = true;

		if (addr & 2)
		{
			/* Tile */
			PPU.OBJ[addr = PPU.OAMAddr >> 1].Name = PPU.OAMWriteRegister & 0x1ff;

			/* priority, h and v flip. */
			PPU.OBJ[addr].Palette  = (highbyte >> 1) & 7;
			PPU.OBJ[addr].Priority = (highbyte >> 4) & 3;
			PPU.OBJ[addr].HFlip    = (highbyte >> 6) & 1;
			PPU.OBJ[addr].VFlip    = (highbyte >> 7) & 1;
		}
		else
		{
			/* X position (low) */
			PPU.OBJ[addr = PPU.OAMAddr >> 1].HPos &= 0xff00;
			PPU.OBJ[addr].HPos |= lowbyte;

			/* Sprite Y position */
			PPU.OBJ[addr].VPos = highbyte;
		}
	}

	PPU.OAMFlip &= ~1;
	++PPU.OAMAddr;

	if (PPU.OAMPriorityRotation && PPU.FirstSprite != (PPU.OAMAddr >> 1))
	{
		PPU.FirstSprite = (PPU.OAMAddr & 0xfe) >> 1;
		IPPU.OBJChanged = true;
	}
}

static INLINE void REGISTER_2118(uint8_t Byte)
{
	uint32_t address;

	if (PPU.VMA.FullGraphicCount)
	{
		uint32_t rem = PPU.VMA.Address & PPU.VMA.Mask1;
		address      = (((PPU.VMA.Address & ~PPU.VMA.Mask1) + (rem >> PPU.VMA.Shift) + ((rem & (PPU.VMA.FullGraphicCount - 1)) << 3)) << 1) & 0xffff;
		Memory.VRAM[address] = Byte;
	}
	else
		Memory.VRAM[address = (PPU.VMA.Address << 1) & 0xffff] = Byte;

	IPPU.TileCached[TILE_2BIT][address >> 4] = false;
	IPPU.TileCached[TILE_4BIT][address >> 5] = false;
	IPPU.TileCached[TILE_8BIT][address >> 6] = false;

	if (!PPU.VMA.High)
		PPU.VMA.Address += PPU.VMA.Increment;
}

static INLINE void REGISTER_2118_tile(uint8_t Byte)
{
	uint32_t rem = PPU.VMA.Address & PPU.VMA.Mask1;
	uint32_t address = (((PPU.VMA.Address & ~PPU.VMA.Mask1) + (rem >> PPU.VMA.Shift) + ((rem & (PPU.VMA.FullGraphicCount - 1)) << 3)) << 1) & 0xffff;
	Memory.VRAM[address]                     = Byte;
	IPPU.TileCached[TILE_2BIT][address >> 4] = false;
	IPPU.TileCached[TILE_4BIT][address >> 5] = false;
	IPPU.TileCached[TILE_8BIT][address >> 6] = false;

	if (!PPU.VMA.High)
		PPU.VMA.Address += PPU.VMA.Increment;
}

static INLINE void REGISTER_2118_linear(uint8_t Byte)
{
	uint32_t address                         = (PPU.VMA.Address << 1) & 0xffff;
	Memory.VRAM[address]                     = Byte;
	IPPU.TileCached[TILE_2BIT][address >> 4] = false;
	IPPU.TileCached[TILE_4BIT][address >> 5] = false;
	IPPU.TileCached[TILE_8BIT][address >> 6] = false;

	if (!PPU.VMA.High)
		PPU.VMA.Address += PPU.VMA.Increment;
}

static INLINE void REGISTER_2119(uint8_t Byte)
{
	uint32_t address;

	if (PPU.VMA.FullGraphicCount)
	{
		uint32_t rem = PPU.VMA.Address & PPU.VMA.Mask1;
		address      = ((((PPU.VMA.Address & ~PPU.VMA.Mask1) + (rem >> PPU.VMA.Shift) + ((rem & (PPU.VMA.FullGraphicCount - 1)) << 3)) << 1) + 1) & 0xffff;
		Memory.VRAM[address] = Byte;
	}
	else
		Memory.VRAM[address = ((PPU.VMA.Address << 1) + 1) & 0xffff] = Byte;

	IPPU.TileCached[TILE_2BIT][address >> 4] = false;
	IPPU.TileCached[TILE_4BIT][address >> 5] = false;
	IPPU.TileCached[TILE_8BIT][address >> 6] = false;

	if (PPU.VMA.High)
		PPU.VMA.Address += PPU.VMA.Increment;
}

static INLINE void REGISTER_2119_tile(uint8_t Byte)
{
	uint32_t rem     = PPU.VMA.Address & PPU.VMA.Mask1;
	uint32_t address = ((((PPU.VMA.Address & ~PPU.VMA.Mask1) + (rem >> PPU.VMA.Shift) + ((rem & (PPU.VMA.FullGraphicCount - 1)) << 3)) << 1) + 1) & 0xffff;
	Memory.VRAM[address]                     = Byte;
	IPPU.TileCached[TILE_2BIT][address >> 4] = false;
	IPPU.TileCached[TILE_4BIT][address >> 5] = false;
	IPPU.TileCached[TILE_8BIT][address >> 6] = false;

	if (PPU.VMA.High)
		PPU.VMA.Address += PPU.VMA.Increment;
}

static INLINE void REGISTER_2119_linear(uint8_t Byte)
{
	uint32_t address                         = ((PPU.VMA.Address << 1) + 1) & 0xffff;
	Memory.VRAM[address]                     = Byte;
	IPPU.TileCached[TILE_2BIT][address >> 4] = false;
	IPPU.TileCached[TILE_4BIT][address >> 5] = false;
	IPPU.TileCached[TILE_8BIT][address >> 6] = false;

	if (PPU.VMA.High)
		PPU.VMA.Address += PPU.VMA.Increment;
}

static INLINE void REGISTER_2122(uint8_t Byte)
{
	if (PPU.CGFLIP)
	{
		if ((Byte & 0x7f) != (PPU.CGDATA[PPU.CGADD] >> 8) || PPU.CGSavedByte != (uint8_t) (PPU.CGDATA[PPU.CGADD] & 0xff))
		{
			FLUSH_REDRAW();
			PPU.CGDATA[PPU.CGADD]        = (Byte & 0x7f) << 8 | PPU.CGSavedByte;
			IPPU.ColorsChanged           = true;
			IPPU.Red[PPU.CGADD]          = IPPU.XB[PPU.CGSavedByte & 0x1f];
			IPPU.Blue[PPU.CGADD]         = IPPU.XB[(Byte >> 2) & 0x1f];
			IPPU.Green[PPU.CGADD]        = IPPU.XB[(PPU.CGDATA[PPU.CGADD] >> 5) & 0x1f];
			IPPU.ScreenColors[PPU.CGADD] = (uint16_t) BUILD_PIXEL(IPPU.Red[PPU.CGADD], IPPU.Green[PPU.CGADD], IPPU.Blue[PPU.CGADD]);
		}

		PPU.CGADD++;
	}
	else
	{
		PPU.CGSavedByte = Byte;
	}

	PPU.CGFLIP = !PPU.CGFLIP;
}

static INLINE void REGISTER_2180(uint8_t Byte)
{
	Memory.RAM[PPU.WRAM++] = Byte;
	PPU.WRAM &= 0x1ffff;
}

static INLINE uint8_t REGISTER_4212()
{
	uint8_t byte = 0;

	if (CPU.V_Counter >= PPU.ScreenHeight + FIRST_VISIBLE_LINE && CPU.V_Counter < PPU.ScreenHeight + FIRST_VISIBLE_LINE + 3)
		byte = 1;

	if (CPU.Cycles >= Settings.HBlankStart)
		byte |= 0x40;

	if (CPU.V_Counter >= PPU.ScreenHeight + FIRST_VISIBLE_LINE)
		byte |= 0x80;

	return byte;
}
#endif
