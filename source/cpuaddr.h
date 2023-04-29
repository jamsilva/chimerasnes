#ifndef CHIMERASNES_CPUADDR_H_
#define CHIMERASNES_CPUADDR_H_

#include <retro_inline.h>

#ifdef SA1_OPCODES
	#define AddCycles(cycles)
#else
	#define AddCycles(cycles) CPU.Cycles += cycles
#endif

extern int32_t OpAddress;

static INLINE void Immediate8()
{
	OpAddress = ICPU.ShiftedPB + CPU.PC - CPU.PCBase;
	CPU.PC++;
}

static INLINE void Immediate16()
{
	OpAddress = ICPU.ShiftedPB + CPU.PC - CPU.PCBase;
	CPU.PC += 2;
}

static INLINE void Relative()
{
	int8_t Int8 = *CPU.PC++;
	AddCycles(CPU.MemSpeed);
	OpAddress = ((int32_t) (CPU.PC - CPU.PCBase) + Int8) & 0xffff;
}

static INLINE void RelativeLong()
{
#ifdef MSB_FIRST
	OpAddress = CPU.PC[0] + (CPU.PC[1] << 8);
#else
	OpAddress = *(uint16_t*) CPU.PC;
#endif

	AddCycles(CPU.MemSpeedx2 + Settings.OneCycle);
	CPU.PC += 2;
	OpAddress += (CPU.PC - CPU.PCBase);
	OpAddress &= 0xffff;
}

static INLINE void AbsoluteIndexedIndirect(bool read)
{
#ifdef MSB_FIRST
	OpAddress = (ICPU.Registers.X.W + CPU.PC[0] + (CPU.PC[1] << 8)) & 0xffff;
#else
	OpAddress = (ICPU.Registers.X.W + *(uint16_t*) CPU.PC) & 0xffff;
#endif

	AddCycles(CPU.MemSpeedx2 + Settings.OneCycle);
	ICPU.OpenBus = CPU.PC[1];
	CPU.PC += 2;
	OpAddress = GetWord(ICPU.ShiftedPB + OpAddress);

	if (read)
		ICPU.OpenBus = (uint8_t) (OpAddress >> 8);
}

static INLINE void AbsoluteIndirectLong(bool read)
{
#ifdef MSB_FIRST
	OpAddress = CPU.PC[0] + (CPU.PC[1] << 8);
#else
	OpAddress = *(uint16_t*) CPU.PC;
#endif

	AddCycles(CPU.MemSpeedx2);
	ICPU.OpenBus = CPU.PC[1];
	CPU.PC += 2;

	if (read)
		OpAddress = GetWord(OpAddress) | ((ICPU.OpenBus = GetByte(OpAddress + 2)) << 16);
	else
		OpAddress = GetWord(OpAddress) | (GetByte(OpAddress + 2) << 16);
}

static INLINE void AbsoluteIndirect(bool read)
{
#ifdef MSB_FIRST
	OpAddress = CPU.PC[0] + (CPU.PC[1] << 8);
#else
	OpAddress = *(uint16_t*) CPU.PC;
#endif

	AddCycles(CPU.MemSpeedx2);
	ICPU.OpenBus = CPU.PC[1];
	CPU.PC += 2;
	OpAddress = GetWord(OpAddress);

	if (read)
		ICPU.OpenBus = (uint8_t) (OpAddress >> 8);

	OpAddress += ICPU.ShiftedPB;
}

static INLINE void Absolute(bool read)
{
#ifdef MSB_FIRST
	OpAddress = CPU.PC[0] + (CPU.PC[1] << 8) + ICPU.ShiftedDB;
#else
	OpAddress = *(uint16_t*) CPU.PC + ICPU.ShiftedDB;
#endif

	if (read)
		ICPU.OpenBus = CPU.PC[1];

	CPU.PC += 2;
	AddCycles(CPU.MemSpeedx2);
}

static INLINE void AbsoluteLong(bool read)
{
#ifdef MSB_FIRST
	OpAddress = CPU.PC[0] + (CPU.PC[1] << 8) + (CPU.PC[2] << 16);
#else
	OpAddress = (*(uint32_t*) CPU.PC) & 0xffffff;
#endif

	if (read)
		ICPU.OpenBus = CPU.PC[2];

	CPU.PC += 3;
	AddCycles(CPU.MemSpeedx2 + CPU.MemSpeed);
}

static INLINE void Direct(bool read)
{
	if (read)
		ICPU.OpenBus = *CPU.PC;

	OpAddress = (*CPU.PC++ + ICPU.Registers.D.W) & 0xffff;
	AddCycles(CPU.MemSpeed);
}

static INLINE void DirectIndirectIndexed(bool read)
{
	ICPU.OpenBus   = *CPU.PC;
	OpAddress = (*CPU.PC++ + ICPU.Registers.D.W) & 0xffff;
	AddCycles(CPU.MemSpeed);
	OpAddress = GetWord(OpAddress);

	if (read)
		ICPU.OpenBus = (uint8_t) (OpAddress >> 8);

	OpAddress += ICPU.ShiftedDB + ICPU.Registers.Y.W;
}

static INLINE void DirectIndirectIndexedLong(bool read)
{
	ICPU.OpenBus   = *CPU.PC;
	OpAddress = (*CPU.PC++ + ICPU.Registers.D.W) & 0xffff;
	AddCycles(CPU.MemSpeed);

	if (read)
		OpAddress = GetWord(OpAddress) + ((ICPU.OpenBus = GetByte(OpAddress + 2)) << 16) + ICPU.Registers.Y.W;
	else
		OpAddress = GetWord(OpAddress) + (GetByte(OpAddress + 2) << 16) + ICPU.Registers.Y.W;
}

static INLINE void DirectIndexedIndirect(bool read)
{
	ICPU.OpenBus   = *CPU.PC;
	OpAddress = (*CPU.PC++ + ICPU.Registers.D.W + ICPU.Registers.X.W) & 0xffff;
	AddCycles(CPU.MemSpeed);
	OpAddress = GetWord(OpAddress);

	if (read)
		ICPU.OpenBus = (uint8_t) (OpAddress >> 8);

	OpAddress += ICPU.ShiftedDB;
	AddCycles(Settings.OneCycle);
}

static INLINE void DirectIndexedX(bool read)
{
	if (read)
		ICPU.OpenBus = *CPU.PC;

	OpAddress = (*CPU.PC++ + ICPU.Registers.D.W + ICPU.Registers.X.W);
	OpAddress &= CheckEmulation() ? 0xff : 0xffff;
	AddCycles(CPU.MemSpeed + Settings.OneCycle);
}

static INLINE void DirectIndexedY(bool read)
{
	if (read)
		ICPU.OpenBus = *CPU.PC;

	OpAddress = (*CPU.PC++ + ICPU.Registers.D.W + ICPU.Registers.Y.W);
	OpAddress &= CheckEmulation() ? 0xff : 0xffff;
	AddCycles(CPU.MemSpeed + Settings.OneCycle);
}

static INLINE void AbsoluteIndexedX(bool read)
{
#ifdef MSB_FIRST
	OpAddress = ICPU.ShiftedDB + CPU.PC[0] + (CPU.PC[1] << 8) + ICPU.Registers.X.W;
#else
	OpAddress = ICPU.ShiftedDB + *(uint16_t*) CPU.PC + ICPU.Registers.X.W;
#endif

	if (read)
		ICPU.OpenBus = CPU.PC[1];

	CPU.PC += 2;
	AddCycles(CPU.MemSpeedx2);
}

static INLINE void AbsoluteIndexedY(bool read)
{
#ifdef MSB_FIRST
	OpAddress = ICPU.ShiftedDB + CPU.PC[0] + (CPU.PC[1] << 8) + ICPU.Registers.Y.W;
#else
	OpAddress = ICPU.ShiftedDB + *(uint16_t*) CPU.PC + ICPU.Registers.Y.W;
#endif

	if (read)
		ICPU.OpenBus = CPU.PC[1];

	CPU.PC += 2;
	AddCycles(CPU.MemSpeedx2);
}

static INLINE void AbsoluteLongIndexedX(bool read)
{
#ifdef MSB_FIRST
	OpAddress = (CPU.PC[0] + (CPU.PC[1] << 8) + (CPU.PC[2] << 16) + ICPU.Registers.X.W) & 0xffffff;
#else
	OpAddress = (*(uint32_t*) CPU.PC + ICPU.Registers.X.W) & 0xffffff;
#endif

	if (read)
		ICPU.OpenBus = CPU.PC[2];

	CPU.PC += 3;
	AddCycles(CPU.MemSpeedx2 + CPU.MemSpeed);
}

static INLINE void DirectIndirect(bool read)
{
	ICPU.OpenBus   = *CPU.PC;
	OpAddress = (*CPU.PC++ + ICPU.Registers.D.W) & 0xffff;
	AddCycles(CPU.MemSpeed);
	OpAddress = GetWord(OpAddress);

	if (read)
		ICPU.OpenBus = (uint8_t) (OpAddress >> 8);

	OpAddress += ICPU.ShiftedDB;
}

static INLINE void DirectIndirectLong(bool read)
{
	ICPU.OpenBus   = *CPU.PC;
	OpAddress = (*CPU.PC++ + ICPU.Registers.D.W) & 0xffff;
	AddCycles(CPU.MemSpeed);

	if (read)
		OpAddress = GetWord(OpAddress) + ((ICPU.OpenBus = GetByte(OpAddress + 2)) << 16);
	else
		OpAddress = GetWord(OpAddress) + (GetByte(OpAddress + 2) << 16);
}

static INLINE void StackRelative(bool read)
{
	if (read)
		ICPU.OpenBus = *CPU.PC;

	OpAddress = (*CPU.PC++ + ICPU.Registers.S.W) & 0xffff;
	AddCycles(CPU.MemSpeed + Settings.OneCycle);
}

static INLINE void StackRelativeIndirectIndexed(bool read)
{
	ICPU.OpenBus   = *CPU.PC;
	OpAddress = (*CPU.PC++ + ICPU.Registers.S.W) & 0xffff;
	AddCycles(CPU.MemSpeed + Settings.TwoCycles);
	OpAddress = GetWord(OpAddress);

	if (read)
		ICPU.OpenBus = (uint8_t) (OpAddress >> 8);

	OpAddress = (OpAddress + ICPU.ShiftedDB + ICPU.Registers.Y.W) & 0xffffff;
}
#endif
