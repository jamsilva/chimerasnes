#ifndef CHIMERASNES_CPUADDR_H_
#define CHIMERASNES_CPUADDR_H_

#include <retro_inline.h>

#ifdef SA1_OPCODES
	#define AddCycles(cycles)
#else
	#define AddCycles(cycles) CPU.Cycles += cycles
#endif

#define READ_PC_BYTE(v)                     \
	do                                      \
	{                                       \
		v = CPU.PCBase[ICPU.Registers.PCw]; \
		ICPU.Registers.PCw++;               \
	} while (0)

#define READ_PC_WORD(v)                                 \
	do                                                  \
	{                                                   \
		v = READ_WORD(CPU.PCBase + ICPU.Registers.PCw); \
		ICPU.Registers.PCw += 2;                        \
	} while (0)

#define READ_PC_3WORD(v)                                 \
	do                                                   \
	{                                                    \
		v = READ_3WORD(CPU.PCBase + ICPU.Registers.PCw); \
		ICPU.Registers.PCw += 3;                         \
	} while (0)

extern int32_t OpAddress;

static INLINE void Immediate8()
{
	OpAddress = ICPU.Registers.PBPC;
	ICPU.Registers.PCw++;
}

static INLINE void Immediate16()
{
	OpAddress = ICPU.Registers.PBPC;
	ICPU.Registers.PCw += 2;
}

static INLINE void Relative()
{
	int8_t Int8;
	READ_PC_BYTE(Int8);
	AddCycles(CPU.MemSpeed);
	OpAddress = ((int32_t) (ICPU.Registers.PCw) + Int8) & 0xffff;
}

static INLINE void RelativeLong()
{
	READ_PC_WORD(OpAddress);
	AddCycles(CPU.MemSpeedx2 + Settings.OneCycle);
	OpAddress += ICPU.Registers.PCw;
	OpAddress &= 0xffff;
}

static INLINE void AbsoluteIndexedIndirect(bool read)
{
	uint16_t Work16;
	READ_PC_WORD(Work16);
	OpAddress = (ICPU.Registers.X.W + Work16) & 0xffff;
	AddCycles(CPU.MemSpeedx2 + Settings.OneCycle);
	ICPU.OpenBus = (uint8_t) (Work16 >> 8);
	OpAddress = GetWord(ICPU.ShiftedPB + OpAddress);

	if (read)
		ICPU.OpenBus = (uint8_t) (OpAddress >> 8);
}

static INLINE void AbsoluteIndirectLong(bool read)
{
	READ_PC_WORD(OpAddress);
	AddCycles(CPU.MemSpeedx2);
	ICPU.OpenBus = (uint8_t) (OpAddress >> 8);

	if (read)
		OpAddress = GetWord(OpAddress) | ((ICPU.OpenBus = GetByte(OpAddress + 2)) << 16);
	else
		OpAddress = GetWord(OpAddress) | (GetByte(OpAddress + 2) << 16);
}

static INLINE void AbsoluteIndirect(bool read)
{
	READ_PC_WORD(OpAddress);
	AddCycles(CPU.MemSpeedx2);
	ICPU.OpenBus = (uint8_t) (OpAddress >> 8);
	OpAddress = GetWord(OpAddress);

	if (read)
		ICPU.OpenBus = (uint8_t) (OpAddress >> 8);

	OpAddress += ICPU.ShiftedPB;
}

static INLINE void Absolute(bool read)
{
	uint16_t Work16;
	READ_PC_WORD(Work16);
	OpAddress = Work16 + ICPU.ShiftedDB;

	if (read)
		ICPU.OpenBus = (uint8_t) (Work16 >> 8);

	AddCycles(CPU.MemSpeedx2);
}

static INLINE void AbsoluteLong(bool read)
{
	READ_PC_3WORD(OpAddress);

	if (read)
		ICPU.OpenBus = (uint8_t) (OpAddress >> 16);

	AddCycles(CPU.MemSpeedx2 + CPU.MemSpeed);
}

static INLINE void Direct(bool read)
{
	uint8_t Work8;
	READ_PC_BYTE(Work8);

	if (read)
		ICPU.OpenBus = Work8;

	OpAddress = (Work8 + ICPU.Registers.D.W) & 0xffff;
	AddCycles(CPU.MemSpeed);
}

static INLINE void DirectIndirectIndexed(bool read)
{
	READ_PC_BYTE(ICPU.OpenBus);
	OpAddress = (ICPU.OpenBus + ICPU.Registers.D.W) & 0xffff;
	AddCycles(CPU.MemSpeed);
	OpAddress = GetWord(OpAddress);

	if (read)
		ICPU.OpenBus = (uint8_t) (OpAddress >> 8);

	OpAddress += ICPU.ShiftedDB + ICPU.Registers.Y.W;
}

static INLINE void DirectIndirectIndexedLong(bool read)
{
	READ_PC_BYTE(ICPU.OpenBus);
	OpAddress = (ICPU.OpenBus + ICPU.Registers.D.W) & 0xffff;
	AddCycles(CPU.MemSpeed);

	if (read)
		OpAddress = GetWord(OpAddress) + ((ICPU.OpenBus = GetByte(OpAddress + 2)) << 16) + ICPU.Registers.Y.W;
	else
		OpAddress = GetWord(OpAddress) + (GetByte(OpAddress + 2) << 16) + ICPU.Registers.Y.W;
}

static INLINE void DirectIndexedIndirect(bool read)
{
	READ_PC_BYTE(ICPU.OpenBus);
	OpAddress = (ICPU.OpenBus + ICPU.Registers.D.W + ICPU.Registers.X.W) & 0xffff;
	AddCycles(CPU.MemSpeed);
	OpAddress = GetWord(OpAddress);

	if (read)
		ICPU.OpenBus = (uint8_t) (OpAddress >> 8);

	OpAddress += ICPU.ShiftedDB;
	AddCycles(Settings.OneCycle);
}

static INLINE void DirectIndexedX(bool read)
{
	uint8_t Work8;
	READ_PC_BYTE(Work8);

	if (read)
		ICPU.OpenBus = Work8;

	OpAddress = (Work8 + ICPU.Registers.D.W + ICPU.Registers.X.W);
	OpAddress &= CheckEmulation() ? 0xff : 0xffff;
	AddCycles(CPU.MemSpeed + Settings.OneCycle);
}

static INLINE void DirectIndexedY(bool read)
{
	uint8_t Work8;
	READ_PC_BYTE(Work8);

	if (read)
		ICPU.OpenBus = Work8;

	OpAddress = (Work8 + ICPU.Registers.D.W + ICPU.Registers.Y.W);
	OpAddress &= CheckEmulation() ? 0xff : 0xffff;
	AddCycles(CPU.MemSpeed + Settings.OneCycle);
}

static INLINE void AbsoluteIndexedX(bool read)
{
	uint16_t Work16;
	READ_PC_WORD(Work16);
	OpAddress = ICPU.ShiftedDB + Work16 + ICPU.Registers.X.W;

	if (read)
		ICPU.OpenBus = (uint8_t) (Work16 >> 8);

	AddCycles(CPU.MemSpeedx2);
}

static INLINE void AbsoluteIndexedY(bool read)
{
	uint16_t Work16;
	READ_PC_WORD(Work16);
	OpAddress = ICPU.ShiftedDB + Work16 + ICPU.Registers.Y.W;

	if (read)
		ICPU.OpenBus = (uint8_t) (Work16 >> 8);

	AddCycles(CPU.MemSpeedx2);
}

static INLINE void AbsoluteLongIndexedX(bool read)
{
	uint32_t Work32;
	READ_PC_3WORD(Work32);
	OpAddress = (Work32 + ICPU.Registers.X.W) & 0xffffff;

	if (read)
		ICPU.OpenBus = (uint8_t) (Work32 >> 16);

	AddCycles(CPU.MemSpeedx2 + CPU.MemSpeed);
}

static INLINE void DirectIndirect(bool read)
{
	READ_PC_BYTE(ICPU.OpenBus);
	OpAddress = (ICPU.OpenBus + ICPU.Registers.D.W) & 0xffff;
	AddCycles(CPU.MemSpeed);
	OpAddress = GetWord(OpAddress);

	if (read)
		ICPU.OpenBus = (uint8_t) (OpAddress >> 8);

	OpAddress += ICPU.ShiftedDB;
}

static INLINE void DirectIndirectLong(bool read)
{
	READ_PC_BYTE(ICPU.OpenBus);
	OpAddress = (ICPU.OpenBus + ICPU.Registers.D.W) & 0xffff;
	AddCycles(CPU.MemSpeed);

	if (read)
		OpAddress = GetWord(OpAddress) + ((ICPU.OpenBus = GetByte(OpAddress + 2)) << 16);
	else
		OpAddress = GetWord(OpAddress) + (GetByte(OpAddress + 2) << 16);
}

static INLINE void StackRelative(bool read)
{
	uint8_t Work8;
	READ_PC_BYTE(Work8);

	if (read)
		ICPU.OpenBus = Work8;

	OpAddress = (Work8 + ICPU.Registers.S.W) & 0xffff;
	AddCycles(CPU.MemSpeed + Settings.OneCycle);
}

static INLINE void StackRelativeIndirectIndexed(bool read)
{
	READ_PC_BYTE(ICPU.OpenBus);
	OpAddress = (ICPU.OpenBus + ICPU.Registers.S.W) & 0xffff;
	AddCycles(CPU.MemSpeed + Settings.TwoCycles);
	OpAddress = GetWord(OpAddress);

	if (read)
		ICPU.OpenBus = (uint8_t) (OpAddress >> 8);

	OpAddress = (OpAddress + ICPU.ShiftedDB + ICPU.Registers.Y.W) & 0xffffff;
}
#endif
