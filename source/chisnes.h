#ifndef CHIMERASNES_CHISNES_H_
#define CHIMERASNES_CHISNES_H_

#include <stdlib.h>
#include <stdint.h>
#include <libretro.h>

#include "port.h"
#include "65c816.h"

#define ROM_NAME_LEN 23

#define SNES_WIDTH           256
#define SNES_HEIGHT          224
#define SNES_HEIGHT_EXTENDED 239
#define MAX_SNES_WIDTH       (SNES_WIDTH << 1)
#define MAX_SNES_HEIGHT      (SNES_HEIGHT_EXTENDED << 1)
#define IMAGE_WIDTH          MAX_SNES_WIDTH
#define IMAGE_HEIGHT         MAX_SNES_HEIGHT

#define SNES_MAX_NTSC_VCOUNTER 262
#define SNES_MAX_PAL_VCOUNTER  312
#define SNES_MAX_VCOUNTER      (Settings.PAL ? SNES_MAX_PAL_VCOUNTER : SNES_MAX_NTSC_VCOUNTER)
#define SNES_MAX_HCOUNTER      341 /* 0-340 */

#define SPC700_TO_65C816_RATIO 2
#define AUTO_FRAMERATE         200

#define DEFAULT_ONE_CYCLE      6u
#define DEFAULT_SLOW_ONE_CYCLE 8u
#define DEFAULT_TWO_CYCLES     12u

/* NTSC master clock signal 21.47727MHz
 * PPU: master clock / 4
 * 1 / PPU clock * 342 -> 63.695us
 * 63.695us / (1 / 3.579545MHz) -> 228 cycles per scanline
 * From Earth Worm Jim: APU executes an average of 65.14285714 cycles per
 * scanline giving an APU clock speed of 1.022731096MHz                    */

/* PAL master clock signal 21.28137MHz
 * PPU: master clock / 4
 * 1 / PPU clock * 342 -> 64.281us
 * 64.281us / (1 / 3.546895MHz) -> 228 cycles per scanline.  */
#define SNES_SCANLINE_TIME       (63.695e-6)
#define SNES_CLOCK_SPEED         (3579545u)
#define SNES_CLOCK_LEN           (1.0 / SNES_CLOCK_SPEED)
#define SNES_CYCLES_PER_SCANLINE ((uint32_t)((SNES_SCANLINE_TIME / SNES_CLOCK_LEN) * 6 + 0.5))

/* NTSC/PAL Differences, It referred to nesdev.
   Consoles in the USA and Europe run at different speeds due to the different television standards used.
   The main differences between the NTSC and PAL PPUs is as follows:

   Property         NTSC                PAL
   Clock speed      21.477272MHz / 4    26.601712MHz / 5
   Length of VBLANK 20 scanlines        70 scanlines */
#define SNES_CYCLES_PER_SECOND (Settings.PAL ? 21281370 : 21477272)

enum
{
	SNES_TR_MASK     = (1u << 4),
	SNES_TL_MASK     = (1u << 5),
	SNES_X_MASK      = (1u << 6),
	SNES_A_MASK      = (1u << 7),
	SNES_RIGHT_MASK  = (1u << 8),
	SNES_LEFT_MASK   = (1u << 9),
	SNES_DOWN_MASK   = (1u << 10),
	SNES_UP_MASK     = (1u << 11),
	SNES_START_MASK  = (1u << 12),
	SNES_SELECT_MASK = (1u << 13),
	SNES_Y_MASK      = (1u << 14),
	SNES_B_MASK      = (1u << 15)
};

enum
{
	SNES_MULTIPLAYER5,
	SNES_JOYPAD,
	SNES_MOUSE,
	SNES_SUPERSCOPE,
	SNES_JUSTIFIER,
	SNES_JUSTIFIER_2,
	SNES_MAX_CONTROLLER_OPTIONS
};

enum
{
	NMI_FLAG         = (1u << 0),
	IRQ_PENDING_FLAG = (1u << 1),
	SCAN_KEYS_FLAG   = (1u << 2)
};

typedef struct
{
	bool     BranchSkip          : 1;
	bool     InDMA               : 1;
	bool     NMIActive           : 1;
	bool     SRAMModified        : 1;
	bool     WaitingForInterrupt : 1;
	int16_t  _SCPUState_PAD1     : 11;
	uint8_t  IRQActive;
	uint8_t  WhichEvent;
	int32_t  Cycles;
	int32_t  FastROMSpeed;
	int32_t  MemSpeed;
	int32_t  MemSpeedx2;
	int32_t  NextEvent;
	int32_t  V_Counter;
	uint32_t Flags;
	uint32_t IRQCycleCount;
	uint32_t NMICycleCount;
	uint32_t NMITriggerPoint;
	uint32_t WaitCounter;
	uint8_t* PC;
	uint8_t* PCBase;
	uint8_t* PCAtOpcodeStart;
	uint8_t* WaitAddress;
} SCPUState;

enum
{
	HBLANK_START_EVENT  = 0,
	HBLANK_END_EVENT    = 1,
	HTIMER_BEFORE_EVENT = 2,
	HTIMER_AFTER_EVENT  = 3,
	NO_EVENT            = 4
};

enum
{
	NOCHIP     = 0,

	/* Variant in the bottom 2 bits */
	V0         = 0,
	V1         = 1,
	V2         = 2,
	V3         = 3,

	/* Generic chips */
	DSP        = 1 << 2,
	GSU_SETA   = 1 << 3,
	PERIPHERAL = 1 << 4,
	OPTRTC     = 1 << 5,
	OTHERCHIP  = 1 << 6,
	RESERVED   = 1 << 7,

	/* Actual chips */
	DSP_1      = DSP | V0,
	DSP_2      = DSP | V1,
	DSP_3      = DSP | V2,
	DSP_4      = DSP | V3,

	GSU        = GSU_SETA | V0,
	ST_010     = GSU_SETA | V1,
	ST_011     = GSU_SETA | V2, /* Unsupported */
	ST_018     = GSU_SETA | V3, /* Unsupported */

	SFT        = PERIPHERAL | V0,
	XBAND      = PERIPHERAL | V1,
	BS         = PERIPHERAL | V2,
	BSFW       = PERIPHERAL | V3,

	S_RTC      = OPTRTC | V1,
	SPC7110    = OPTRTC | V2,
	SPC7110RTC = OPTRTC | V3,

	SA_1       = OTHERCHIP | V0,
	CX_4       = OTHERCHIP | V1,
	S_DD1      = OTHERCHIP | V2,
	OBC_1      = OTHERCHIP | V3,

	NUMCHIPS   = 0xff
};

typedef struct
{
	bool     APUEnabled           : 1;
	bool     PAL                  : 1;
	bool     Shutdown             : 1;
	bool     OverclockCycles      : 1;
	bool     ReduceSpriteFlicker  : 1;
	bool     StarfoxHack          : 1;
	bool     WinterGold           : 1;
	bool     SecretOfEvermoreHack : 1;
	uint8_t  OneCycle;
	uint8_t  SlowOneCycle;
	uint8_t  TwoCycles;
	uint8_t  ControllerOption;
	uint8_t  Chip;
	uint16_t SuperFXSpeedPerLine;
	int32_t  H_Max;
	int32_t  HBlankStart;
} SSettings;

typedef struct
{
	int32_t  NextAPUTimerPos;
	int32_t  APUTimerCounter;
	uint32_t APUTimerCounter_err;
	uint32_t t64Cnt;
} SEXTState;

extern SEXTState EXT;
extern SSettings Settings;
extern SCPUState CPU;
extern bool finishedFrame;

void SetPause(uint32_t mask);
void ClearPause(uint32_t mask);
#endif
