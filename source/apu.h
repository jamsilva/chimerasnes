#ifndef CHIMERASNES_APU_H_
#define CHIMERASNES_APU_H_

#include "port.h"
#include "spc700.h"

#define DEFAULT_ONE_APU_CYCLE 21

#define APU_EXECUTE1()                     \
	{                                      \
		APU.Cycles += APUCycles[*IAPU.PC]; \
		(*ApuOpcodes[*IAPU.PC])();         \
	}

#define APU_EXECUTE()                   \
	if (!IAPU.Executing)                \
		APU.Cycles = CPU.Cycles;        \
	else                                \
		while (APU.Cycles < CPU.Cycles) \
			APU_EXECUTE1();

typedef struct
{
	bool          Executing   : 1;
	bool          Carry       : 1;
	bool          Overflow    : 1;
	int8_t        _SIAPU_PAD1 : 5;
	int8_t        _SIAPU_PAD2 : 8;
	uint8_t       Bit;
	uint8_t       Zero;
	int32_t       OneCycle;
	uint32_t      Address;
	uint32_t      WaitCounter;
	uint8_t*      DirectPage;
	uint8_t*      PC;
	uint8_t*      RAM;
	uint8_t*      WaitAddress1;
	uint8_t*      WaitAddress2;
	SAPURegisters Registers;
} SIAPU;

typedef struct
{
	bool     ShowROM    : 1;
	int8_t   _SAPU_PAD1 : 7;
	bool     TimerEnabled[3];
	uint8_t  OutPorts[4];
	uint8_t  DSP[0x80];
	uint16_t Timer[3];
	uint16_t TimerTarget[3];
	int32_t  Cycles;
} SAPU;

extern SAPU APU;
extern SIAPU IAPU;

static INLINE void APUUnpackStatus()
{
	IAPU.Zero     = (!(IAPU.Registers.P & APU_ZERO)) | (IAPU.Registers.P & APU_NEGATIVE);
	IAPU.Carry    = (IAPU.Registers.P & APU_CARRY);
	IAPU.Overflow = (IAPU.Registers.P & APU_OVERFLOW) >> 6;
}

static INLINE void APUPackStatus()
{
	IAPU.Registers.P &= ~(APU_ZERO | APU_NEGATIVE | APU_CARRY | APU_OVERFLOW);
	IAPU.Registers.P |= IAPU.Carry | ((!IAPU.Zero) << 1) | (IAPU.Zero & 0x80) | (IAPU.Overflow << 6);
}

void    ResetAPU();
bool    InitAPU();
void    DeinitAPU();
void    DecacheSamples();
void    SetAPUControl(uint8_t byte);
void    APUMainLoop();
uint8_t APUGetByte(uint16_t Address);
void    APUSetByte(uint8_t byte, uint16_t Address);

static INLINE uint8_t APUGetByteDP(uint8_t Address)
{
	return APUGetByte((APUCheckDirectPage() ? 0x100 : 0) + Address);
}

static INLINE void APUSetByteDP(uint8_t byte, uint8_t Address)
{
	APUSetByte(byte, (APUCheckDirectPage() ? 0x100 : 0) + Address);
}

extern uint8_t APUCycles[256];       /* Scaled cycle lengths */
extern uint8_t APUCycleLengths[256]; /* Raw data. */
extern void  (*ApuOpcodes[256])();

enum
{
	APU_VOL_LEFT  = 0x00,
	APU_VOL_RIGHT = 0x01,
	APU_P_LOW     = 0x02,
	APU_P_HIGH    = 0x03,
	APU_SRCN      = 0x04,
	APU_ADSR1     = 0x05,
	APU_ADSR2     = 0x06,
	APU_GAIN      = 0x07,
	APU_ENVX      = 0x08,
	APU_OUTX      = 0x09
};

enum
{
	APU_MVOL_LEFT  = 0x0c,
	APU_MVOL_RIGHT = 0x1c,
	APU_EVOL_LEFT  = 0x2c,
	APU_EVOL_RIGHT = 0x3c,
	APU_KON        = 0x4c,
	APU_KOFF       = 0x5c,
	APU_FLG        = 0x6c,
	APU_ENDX       = 0x7c
};

enum
{
	APU_EFB  = 0x0d,
	APU_PMON = 0x2d,
	APU_NON  = 0x3d,
	APU_EON  = 0x4d,
	APU_DIR  = 0x5d,
	APU_ESA  = 0x6d,
	APU_EDL  = 0x7d,
};

enum
{
	APU_C0 = 0x0f,
	APU_C1 = 0x1f,
	APU_C2 = 0x2f,
	APU_C3 = 0x3f,
	APU_C4 = 0x4f,
	APU_C5 = 0x5f,
	APU_C6 = 0x6f,
	APU_C7 = 0x7f
};

enum
{
	APU_SOFT_RESET    = 0x80,
	APU_MUTE          = 0x40,
	APU_ECHO_DISABLED = 0x20
};

#define FREQUENCY_MASK 0x3fff
#endif
