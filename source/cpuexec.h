#ifndef CHIMERASNES_CPUEXEC_H_
#define CHIMERASNES_CPUEXEC_H_

#include "65c816.h"
#include "ppu.h"

typedef struct
{
	void (*Opcode)();
} SOpcodes;

typedef struct
{
	bool       Carry       : 1;
	bool       Overflow    : 1;
	bool       Zero        : 1;
	int16_t    _SICPU_PAD1 : 13;
	uint8_t    Negative;
	uint8_t    OpenBus;
	uint32_t   ShiftedDB;
	uint32_t   ShiftedPB;
	uint32_t   Frame;
	SRegisters Registers;
	SOpcodes*  Opcodes;
} SICPU;

extern SICPU    ICPU;
extern SOpcodes OpcodesE1[256];
extern SOpcodes OpcodesM1X1[256];
extern SOpcodes OpcodesM1X0[256];
extern SOpcodes OpcodesM0X1[256];
extern SOpcodes OpcodesM0X0[256];

void MainLoop();
void Reset();
void SoftReset();
void DoHBlankProcessing_SFX();
void DoHBlankProcessing_NoSFX();
void ClearIRQSource(uint32_t source);
void SetIRQSource(uint32_t source);

static INLINE void UnpackStatus()
{
	ICPU.Zero     = !(ICPU.Registers.PL & ZERO);
	ICPU.Negative =  (ICPU.Registers.PL & NEGATIVE);
	ICPU.Carry    =  (ICPU.Registers.PL & CARRY);
	ICPU.Overflow =  (ICPU.Registers.PL & OVERFLOW) >> 6;
}

static INLINE void PackStatus()
{
	ICPU.Registers.PL &= ~(ZERO | NEGATIVE | CARRY | OVERFLOW);
	ICPU.Registers.PL |= ICPU.Carry | (!ICPU.Zero << 1) | (ICPU.Negative & 0x80) | (ICPU.Overflow << 6);
}

static INLINE void FixCycles()
{
	if (CheckEmulation())
		ICPU.Opcodes = OpcodesE1;
	else if (CheckMemory())
	{
		if (CheckIndex())
			ICPU.Opcodes = OpcodesM1X1;
		else
			ICPU.Opcodes = OpcodesM1X0;
	}
	else if (CheckIndex())
		ICPU.Opcodes = OpcodesM0X1;
	else
		ICPU.Opcodes = OpcodesM0X0;
}

static INLINE void Reschedule()
{
	uint8_t which;
	int32_t max;

	if (CPU.WhichEvent == HBLANK_START_EVENT || CPU.WhichEvent == HTIMER_AFTER_EVENT)
	{
		which = HBLANK_END_EVENT;
		max = Settings.H_Max;
	}
	else
	{
		which = HBLANK_START_EVENT;
		max   = Settings.HBlankStart;
	}

	if (PPU.HTimerEnabled &&
		(int32_t) PPU.HTimerPosition < max && (int32_t) PPU.HTimerPosition > CPU.NextEvent &&
		(!PPU.VTimerEnabled || (PPU.VTimerEnabled && CPU.V_Counter == PPU.IRQVBeamPos)))
	{
		which = (int32_t) PPU.HTimerPosition < Settings.HBlankStart ? HTIMER_BEFORE_EVENT : HTIMER_AFTER_EVENT;
		max   = PPU.HTimerPosition;
	}

	CPU.NextEvent = max;
	CPU.WhichEvent = which;
}
#endif
