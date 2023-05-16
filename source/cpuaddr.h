#ifndef CHIMERASNES_CPUADDR_H_
#define CHIMERASNES_CPUADDR_H_

#include <retro_inline.h>

#ifdef SA1_OPCODES
	#define AddCycles(cycles) {}
#else
	#define AddCycles(cycles) CPU.Cycles += cycles
#endif

typedef enum
{
	NONE   = 0,
	READ   = 1,
	WRITE  = 2,
	MODIFY = 3,
	JUMP   = 5,
	JSR    = 8
} AccessMode;

static INLINE uint8_t Immediate8Slow(AccessMode a)
{
	uint8_t val = GetByte(ICPU.Registers.PBPC);

	if (a & READ)
		ICPU.OpenBus = val;

	ICPU.Registers.PCw++;
	return val;
}

static INLINE uint8_t Immediate8(AccessMode a)
{
	uint8_t val = CPU.PCBase[ICPU.Registers.PCw];

	if (a & READ)
		ICPU.OpenBus = val;

	AddCycles(CPU.MemSpeed);
	ICPU.Registers.PCw++;
	return val;
}

static INLINE uint16_t Immediate16Slow(AccessMode a)
{
	uint16_t val = GetWord(ICPU.Registers.PBPC, WRAP_BANK);

	if (a & READ)
		ICPU.OpenBus = (uint8_t) (val >> 8);

	ICPU.Registers.PCw += 2;
	return val;
}

static INLINE uint16_t Immediate16(AccessMode a)
{
	uint16_t val = READ_WORD(CPU.PCBase + ICPU.Registers.PCw);

	if (a & READ)
		ICPU.OpenBus = (uint8_t) (val >> 8);

	AddCycles(CPU.MemSpeedx2);
	ICPU.Registers.PCw += 2;
	return val;
}

static INLINE uint32_t RelativeSlow(AccessMode a) /* branch $xx */
{
	int8_t offset = Immediate8Slow(a);
	return ((int16_t) ICPU.Registers.PCw + offset) & 0xffff;
}

static INLINE uint32_t Relative(AccessMode a) /* branch $xx */
{
	int8_t offset = Immediate8(a);
	return ((int16_t) ICPU.Registers.PCw + offset) & 0xffff;
}

static INLINE uint32_t RelativeLongSlow(AccessMode a) /* BRL $xxxx */
{
	int16_t offset = Immediate16Slow(a);
	return ((int32_t) ICPU.Registers.PCw + offset) & 0xffff;
}

static INLINE uint32_t RelativeLong(AccessMode a) /* BRL $xxxx */
{
	int16_t offset = Immediate16(a);
	return ((int32_t) ICPU.Registers.PCw + offset) & 0xffff;
}

static INLINE uint32_t AbsoluteIndexedIndirectSlow(AccessMode a) /* (a,X) */
{
	uint16_t addr, addr2;

	if (a & JSR)
	{
		/* JSR (a,X) pushes the old address in the middle of loading the new.
		 * ICPU.OpenBus needs to be set to account for this. */
		addr = Immediate8Slow(READ);

		if (a == JSR)
			ICPU.OpenBus = ICPU.Registers.PCl;

		addr |= Immediate8Slow(READ) << 8;
	}
	else
		addr = Immediate16Slow(READ);

	AddCycles(Settings.OneCycle);
	addr += ICPU.Registers.X.W;
	addr2 = GetWord(ICPU.ShiftedPB | addr, WRAP_BANK); /* Address load wraps within the bank */
	ICPU.OpenBus = addr2 >> 8;
	return addr2;
}

static INLINE uint32_t AbsoluteIndexedIndirect(AccessMode a) /* (a,X) */
{
	uint16_t addr, addr2;
	addr = Immediate16Slow(READ);
	AddCycles(Settings.OneCycle);
	addr += ICPU.Registers.X.W;
	addr2 = GetWord(ICPU.ShiftedPB | addr, WRAP_BANK); /* Address load wraps within the bank */
	ICPU.OpenBus = addr2 >> 8;
	return addr2;
}

static INLINE uint32_t AbsoluteIndirectLongSlow(AccessMode a) /* [a] */
{
	uint16_t addr = Immediate16Slow(READ);
	uint32_t addr2 = GetWord(addr, WRAP_NONE); /* No info on wrapping, but it doesn't matter anyway due to mirroring */
	ICPU.OpenBus = addr2 >> 8;
	addr2 |= (ICPU.OpenBus = GetByte(addr + 2)) << 16;
	return addr2;
}

static INLINE uint32_t AbsoluteIndirectLong(AccessMode a) /* [a] */
{
	uint16_t addr = Immediate16(READ);
	uint32_t addr2 = GetWord(addr, WRAP_NONE); /* No info on wrapping, but it doesn't matter anyway due to mirroring */
	ICPU.OpenBus = addr2 >> 8;
	addr2 |= (ICPU.OpenBus = GetByte(addr + 2)) << 16;
	return addr2;
}

static INLINE uint32_t AbsoluteIndirectSlow(AccessMode a) /* (a) */
{
	uint16_t addr2 = GetWord(Immediate16Slow(READ), WRAP_NONE); /* No info on wrapping, but it doesn't matter anyway due to mirroring */
	ICPU.OpenBus = addr2 >> 8;
	return addr2;
}

static INLINE uint32_t AbsoluteIndirect(AccessMode a) /* (a) */
{
	uint16_t addr2 = GetWord(Immediate16(READ), WRAP_NONE); /* No info on wrapping, but it doesn't matter anyway due to mirroring */
	ICPU.OpenBus = addr2 >> 8;
	return addr2;
}

static INLINE uint32_t AbsoluteSlow(AccessMode a) /* a */
{
	return ICPU.ShiftedDB | Immediate16Slow(a);
}

static INLINE uint32_t Absolute(AccessMode a) /* a */
{
	return ICPU.ShiftedDB | Immediate16(a);
}

static INLINE uint32_t AbsoluteLongSlow(AccessMode a) /* l */
{
	uint32_t addr = Immediate16Slow(READ);

	/* JSR l pushes the old bank in the middle of loading the new.
	 * ICPU.OpenBus needs to be set to account for this. */
	if (a == JSR)
		ICPU.OpenBus = ICPU.Registers.PB;

	addr |= Immediate8Slow(a) << 16;
	return addr;
}

static INLINE uint32_t AbsoluteLong(AccessMode a) /* l */
{
	uint32_t addr = READ_3WORD(CPU.PCBase + ICPU.Registers.PCw);
	AddCycles(CPU.MemSpeedx2 + CPU.MemSpeed);

	if (a & READ)
		ICPU.OpenBus = addr >> 16;

	ICPU.Registers.PCw += 3;
	return addr;
}

static INLINE uint32_t DirectSlow(AccessMode a) /* d */
{
	uint16_t addr = Immediate8Slow(a) + ICPU.Registers.D.W;

	if (ICPU.Registers.DL != 0)
		AddCycles(Settings.OneCycle);

	return addr;
}

static INLINE uint32_t Direct(AccessMode a) /* d */
{
	uint16_t addr = Immediate8(a) + ICPU.Registers.D.W;

	if (ICPU.Registers.DL != 0)
		AddCycles(Settings.OneCycle);

	return addr;
}

static INLINE uint32_t DirectIndirectSlow(AccessMode a) /* (d) */
{
	uint32_t addr = GetWord(DirectSlow(READ), (!CheckEmulation() || ICPU.Registers.DL) ? WRAP_BANK : WRAP_PAGE);

	if (a & READ)
		ICPU.OpenBus = (uint8_t) (addr >> 8);

	addr |= ICPU.ShiftedDB;
	return addr;
}

static INLINE uint32_t DirectIndirectE0(AccessMode a) /* (d) */
{
	uint32_t addr = GetWord(Direct(READ), WRAP_NONE);

	if (a & READ)
		ICPU.OpenBus = (uint8_t) (addr >> 8);

	addr |= ICPU.ShiftedDB;
	return addr;
}

static INLINE uint32_t DirectIndirectE1(AccessMode a) /* (d) */
{
	uint32_t addr = GetWord(DirectSlow(READ), ICPU.Registers.DL ? WRAP_BANK : WRAP_PAGE);

	if (a & READ)
		ICPU.OpenBus = (uint8_t) (addr >> 8);

	addr |= ICPU.ShiftedDB;
	return addr;
}

static INLINE uint32_t DirectIndirectIndexedSlow(AccessMode a) /* (d),Y */
{
	uint32_t addr = DirectIndirectSlow(a);

	if ((a & WRITE) || !CheckIndex() || (addr & 0xff) + ICPU.Registers.YL >= 0x100)
		AddCycles(Settings.OneCycle);

	return addr + ICPU.Registers.Y.W;
}

static INLINE uint32_t DirectIndirectIndexedE0X0(AccessMode a) /* (d),Y */
{
	uint32_t addr = DirectIndirectE0(a);
	AddCycles(Settings.OneCycle);
	return addr + ICPU.Registers.Y.W;
}

static INLINE uint32_t DirectIndirectIndexedE0X1(AccessMode a) /* (d),Y */
{
	uint32_t addr = DirectIndirectE0(a);

	if ((a & WRITE) || (addr & 0xff) + ICPU.Registers.YL >= 0x100)
		AddCycles(Settings.OneCycle);

	return addr + ICPU.Registers.Y.W;
}

static INLINE uint32_t DirectIndirectIndexedE1(AccessMode a) /* (d),Y */
{
	uint32_t addr = DirectIndirectE1(a);

	if ((a & WRITE) || (addr & 0xff) + ICPU.Registers.YL >= 0x100)
		AddCycles(Settings.OneCycle);

	return addr + ICPU.Registers.Y.W;
}

static INLINE uint32_t DirectIndirectLongSlow(AccessMode a) /* [d] */
{
	uint16_t addr = DirectSlow(READ);
	uint32_t addr2 = GetWord(addr, WRAP_NONE);
	ICPU.OpenBus = addr2 >> 8;
	addr2 |= (ICPU.OpenBus = GetByte(addr + 2)) << 16;
	return addr2;
}

static INLINE uint32_t DirectIndirectLong(AccessMode a) /* [d] */
{
	uint16_t addr = Direct(READ);
	uint32_t addr2 = GetWord(addr, WRAP_NONE);
	ICPU.OpenBus = addr2 >> 8;
	addr2 |= (ICPU.OpenBus = GetByte(addr + 2)) << 16;
	return addr2;
}

static INLINE uint32_t DirectIndirectIndexedLongSlow(AccessMode a) /* [d],Y */
{
	return DirectIndirectLongSlow(a) + ICPU.Registers.Y.W;
}

static INLINE uint32_t DirectIndirectIndexedLong(AccessMode a) /* [d],Y */
{
	return DirectIndirectLong(a) + ICPU.Registers.Y.W;
}

static INLINE uint32_t DirectIndexedXSlow(AccessMode a) /* d,X */
{
	pair addr;
	addr.W = DirectSlow(a);

	if (!CheckEmulation() || ICPU.Registers.DL)
		addr.W += ICPU.Registers.X.W;
	else
		addr.B.l += ICPU.Registers.XL;

	AddCycles(Settings.OneCycle);
	return addr.W;
}

static INLINE uint32_t DirectIndexedXE0(AccessMode a) /* d,X */
{
	uint16_t addr = Direct(a) + ICPU.Registers.X.W;
	AddCycles(Settings.OneCycle);
	return addr;
}

static INLINE uint32_t DirectIndexedXE1(AccessMode a) /* d,X */
{
	if (ICPU.Registers.DL)
		return DirectIndexedXE0(a);

	pair addr;
	addr.W = Direct(a);
	addr.B.l += ICPU.Registers.XL;
	AddCycles(Settings.OneCycle);
	return addr.W;
}

static INLINE uint32_t DirectIndexedYSlow(AccessMode a) /* d,Y */
{
	pair addr;
	addr.W = DirectSlow(a);

	if (!CheckEmulation() || ICPU.Registers.DL)
		addr.W += ICPU.Registers.Y.W;
	else
		addr.B.l += ICPU.Registers.YL;

	AddCycles(Settings.OneCycle);
	return addr.W;
}

static INLINE uint32_t DirectIndexedYE0(AccessMode a) /* d,Y */
{
	uint16_t addr = Direct(a) + ICPU.Registers.Y.W;
	AddCycles(Settings.OneCycle);
	return addr;
}

static INLINE uint32_t DirectIndexedYE1(AccessMode a) /* d,Y */
{
	if (ICPU.Registers.DL)
		return (DirectIndexedYE0(a));

	pair addr;
	addr.W = Direct(a);
	addr.B.l += ICPU.Registers.YL;
	AddCycles(Settings.OneCycle);
	return addr.W;
}

static INLINE uint32_t DirectIndexedIndirectSlow(AccessMode a) /* (d,X) */
{
	uint32_t addr = GetWord(DirectIndexedXSlow(READ), (!CheckEmulation() || ICPU.Registers.DL) ? WRAP_BANK : WRAP_PAGE);

	if (a & READ)
		ICPU.OpenBus = (uint8_t) (addr >> 8);

	return ICPU.ShiftedDB | addr;
}

static INLINE uint32_t DirectIndexedIndirectE0(AccessMode a) /* (d,X) */
{
	uint32_t addr = GetWord(DirectIndexedXE0(READ), WRAP_NONE);

	if (a & READ)
		ICPU.OpenBus = (uint8_t) (addr >> 8);

	return ICPU.ShiftedDB | addr;
}

static INLINE uint32_t DirectIndexedIndirectE1(AccessMode a) /* (d,X) */
{
	uint32_t addr = GetWord(DirectIndexedXE1(READ), ICPU.Registers.DL ? WRAP_BANK : WRAP_PAGE);

	if (a & READ)
		ICPU.OpenBus = (uint8_t) (addr >> 8);

	return ICPU.ShiftedDB | addr;
}

static INLINE uint32_t AbsoluteIndexedXSlow(AccessMode a) /* a,X */
{
	uint32_t addr = AbsoluteSlow(a);

	if ((a & WRITE) || !CheckIndex() || (addr & 0xff) + ICPU.Registers.XL >= 0x100)
		AddCycles(Settings.OneCycle);

	return addr + ICPU.Registers.X.W;
}

static INLINE uint32_t AbsoluteIndexedXX0(AccessMode a) /* a,X */
{
	uint32_t addr = Absolute(a);
	AddCycles(Settings.OneCycle);
	return addr + ICPU.Registers.X.W;
}

static INLINE uint32_t AbsoluteIndexedXX1(AccessMode a) /* a,X */
{
	uint32_t addr = Absolute(a);

	if ((a & WRITE) || (addr & 0xff) + ICPU.Registers.XL >= 0x100)
		AddCycles(Settings.OneCycle);

	return addr + ICPU.Registers.X.W;
}

static INLINE uint32_t AbsoluteIndexedYSlow(AccessMode a) /* a,Y */
{
	uint32_t addr = AbsoluteSlow(a);

	if ((a & WRITE) || !CheckIndex() || (addr & 0xff) + ICPU.Registers.YL >= 0x100)
		AddCycles(Settings.OneCycle);

	return addr + ICPU.Registers.Y.W;
}

static INLINE uint32_t AbsoluteIndexedYX0(AccessMode a) /* a,Y */
{
	uint32_t addr = Absolute(a);
	AddCycles(Settings.OneCycle);
	return addr + ICPU.Registers.Y.W;
}

static INLINE uint32_t AbsoluteIndexedYX1(AccessMode a) /* a,Y */
{
	uint32_t addr = Absolute(a);

	if ((a & WRITE) || (addr & 0xff) + ICPU.Registers.YL >= 0x100)
		AddCycles(Settings.OneCycle);

	return addr + ICPU.Registers.Y.W;
}

static INLINE uint32_t AbsoluteLongIndexedXSlow(AccessMode a) /* l,X */
{
	return AbsoluteLongSlow(a) + ICPU.Registers.X.W;
}

static INLINE uint32_t AbsoluteLongIndexedX(AccessMode a) /* l,X */
{
	return AbsoluteLong(a) + ICPU.Registers.X.W;
}

static INLINE uint32_t StackRelativeSlow(AccessMode a) /* d,S */
{
	uint16_t addr = Immediate8Slow(a) + ICPU.Registers.S.W;
	AddCycles(Settings.OneCycle);
	return addr;
}

static INLINE uint32_t StackRelative(AccessMode a) /* d,S */
{
	uint16_t addr = Immediate8(a) + ICPU.Registers.S.W;
	AddCycles(Settings.OneCycle);
	return addr;
}

static INLINE uint32_t StackRelativeIndirectIndexedSlow(AccessMode a) /* (d,S),Y */
{
	uint32_t addr = GetWord(StackRelativeSlow(READ), WRAP_NONE);

	if (a & READ)
		ICPU.OpenBus = (uint8_t) (addr >> 8);

	addr = (addr + ICPU.Registers.Y.W + ICPU.ShiftedDB) & 0xffffff;
	AddCycles(Settings.OneCycle);
	return addr;
}

static INLINE uint32_t StackRelativeIndirectIndexed(AccessMode a) /* (d,S),Y */
{
	uint32_t addr = GetWord(StackRelative(READ), WRAP_NONE);

	if (a & READ)
		ICPU.OpenBus = (uint8_t) (addr >> 8);

	addr = (addr + ICPU.Registers.Y.W + ICPU.ShiftedDB) & 0xffffff;
	return addr;
}
#endif
