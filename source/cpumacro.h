#ifndef CHIMERASNES_CPUMACRO_H_
#define CHIMERASNES_CPUMACRO_H_

#include <retro_inline.h>

#ifdef SA1_OPCODES
	#define AddCycles(cycles)
#else
	#define AddCycles(cycles) CPU.Cycles += cycles
#endif

extern int32_t OpAddress;

static INLINE void SetZN16(uint16_t Work)
{
	ICPU.Zero = (bool) Work;
	ICPU.Negative = (uint8_t) (Work >> 8);
}

static INLINE void SetZN8(uint8_t Work)
{
	ICPU.Zero = Work;
	ICPU.Negative = Work;
}

static INLINE void ADC16()
{
	uint16_t Work16 = GetWord(OpAddress);

	if (CheckDecimal())
	{
		uint16_t Ans16;
		uint8_t  A1 = (ICPU.Registers.A.W) & 0xf;
		uint8_t  A2 = (ICPU.Registers.A.W >> 4) & 0xf;
		uint8_t  A3 = (ICPU.Registers.A.W >> 8) & 0xf;
		uint8_t  A4 = (ICPU.Registers.A.W >> 12) & 0xf;
		uint8_t  W1 = Work16 & 0xf;
		uint8_t  W2 = (Work16 >> 4) & 0xf;
		uint8_t  W3 = (Work16 >> 8) & 0xf;
		uint8_t  W4 = (Work16 >> 12) & 0xf;
		A1 += W1 + CheckCarry();

		if (A1 > 9)
		{
			A1 -= 10;
			A1 &= 0xf;
			A2++;
		}

		A2 += W2;

		if (A2 > 9)
		{
			A2 -= 10;
			A2 &= 0xf;
			A3++;
		}

		A3 += W3;

		if (A3 > 9)
		{
			A3 -= 10;
			A3 &= 0xf;
			A4++;
		}

		A4 += W4;

		if (A4 > 9)
		{
			A4 -= 10;
			A4 &= 0xf;
			SetCarry();
		}
		else
			ClearCarry();

		Ans16 = (A4 << 12) | (A3 << 8) | (A2 << 4) | (A1);

		if (~(ICPU.Registers.A.W ^ Work16) & (Work16 ^ Ans16) & 0x8000)
			SetOverflow();
		else
			ClearOverflow();

		ICPU.Registers.A.W = Ans16;
	}
	else
	{
		uint32_t Ans32 = ICPU.Registers.A.W + Work16 + CheckCarry();
		ICPU.Carry = (Ans32 > 0xffff);

		if (~(ICPU.Registers.A.W ^ Work16) & (Work16 ^ (uint16_t) Ans32) & 0x8000)
			SetOverflow();
		else
			ClearOverflow();

		ICPU.Registers.A.W = (uint16_t) Ans32;
	}

	SetZN16(ICPU.Registers.A.W);
}

static INLINE void ADC8()
{
	uint8_t Work8 = GetByte(OpAddress);

	if (CheckDecimal())
	{
		int8_t  Ans8;
		uint8_t A1 = (ICPU.Registers.A.W) & 0xf;
		uint8_t A2 = (ICPU.Registers.A.W >> 4) & 0xf;
		uint8_t W1 = Work8 & 0xf;
		uint8_t W2 = (Work8 >> 4) & 0xf;
		A1 += W1 + CheckCarry();

		if (A1 > 9)
		{
			A1 -= 10;
			A1 &= 0xf;
			A2++;
		}

		A2 += W2;

		if (A2 > 9)
		{
			A2 -= 10;
			A2 &= 0xf;
			SetCarry();
		}
		else
			ClearCarry();

		Ans8 = (A2 << 4) | A1;

		if (~(ICPU.Registers.AL ^ Work8) & (Work8 ^ Ans8) & 0x80)
			SetOverflow();
		else
			ClearOverflow();

		ICPU.Registers.AL = Ans8;
	}
	else
	{
		int16_t Ans16 = ICPU.Registers.AL + Work8 + CheckCarry();
		ICPU.Carry = (Ans16 > 0xff);

		if (~(ICPU.Registers.AL ^ Work8) & (Work8 ^ (uint8_t) Ans16) & 0x80)
			SetOverflow();
		else
			ClearOverflow();

		ICPU.Registers.AL = (uint8_t) Ans16;
	}

	SetZN8(ICPU.Registers.AL);
}

static INLINE void AND16()
{
	ICPU.Registers.A.W &= GetWord(OpAddress);
	SetZN16(ICPU.Registers.A.W);
}

static INLINE void AND8()
{
	ICPU.Registers.AL &= GetByte(OpAddress);
	SetZN8(ICPU.Registers.AL);
}

static INLINE void A_ASL16()
{
	ICPU.Carry = (ICPU.Registers.AH & 0x80) != 0;
	ICPU.Registers.A.W <<= 1;
	AddCycles(ONE_CYCLE);
	SetZN16(ICPU.Registers.A.W);
}

static INLINE void A_ASL8()
{
	ICPU.Carry = (ICPU.Registers.AL & 0x80) != 0;
	ICPU.Registers.AL <<= 1;
	AddCycles(ONE_CYCLE);
	SetZN8(ICPU.Registers.AL);
}

static INLINE void ASL16()
{
	uint16_t Work16 = GetWord(OpAddress);
	ICPU.Carry = (bool) (Work16 & 0x8000);
	Work16 <<= 1;
	AddCycles(ONE_CYCLE);
	SetByte(Work16 >> 8, OpAddress + 1);
	ICPU.OpenBus = Work16 & 0xff;
	SetByte(ICPU.OpenBus, OpAddress);
	SetZN16(Work16);
}

static INLINE void ASL8()
{
	uint8_t Work8 = GetByte(OpAddress);
	ICPU.Carry = (bool) (Work8 & 0x80);
	Work8 <<= 1;
	AddCycles(ONE_CYCLE);
	SetByte(Work8, OpAddress);
	ICPU.OpenBus = Work8;
	SetZN8(Work8);
}

static INLINE void BIT16()
{
	uint16_t Work16 = GetWord(OpAddress);
	ICPU.Overflow = (bool) (Work16 & 0x4000);
	ICPU.Negative = (uint8_t) (Work16 >> 8);
	ICPU.Zero = (bool) (Work16 & ICPU.Registers.A.W);
}

static INLINE void BIT8()
{
	uint8_t Work8 = GetByte(OpAddress);
	ICPU.Overflow = (bool) (Work8 & 0x40);
	ICPU.Negative = Work8;
	ICPU.Zero = Work8 & ICPU.Registers.AL;
}

static INLINE void CMP16()
{
	int32_t Int32 = (int32_t) ICPU.Registers.A.W - (int32_t) GetWord(OpAddress);
	ICPU.Carry = (Int32 >= 0);
	SetZN16((uint16_t) Int32);
}

static INLINE void CMP8()
{
	int16_t Int16 = (int16_t) ICPU.Registers.AL - (int16_t) GetByte(OpAddress);
	ICPU.Carry = (Int16 >= 0);
	SetZN8((uint8_t) Int16);
}

static INLINE void CMX16()
{
	int32_t Int32 = (int32_t) ICPU.Registers.X.W - (int32_t) GetWord(OpAddress);
	ICPU.Carry = (Int32 >= 0);
	SetZN16((uint16_t) Int32);
}

static INLINE void CMX8()
{
	int16_t Int16 = (int16_t) ICPU.Registers.XL - (int16_t) GetByte(OpAddress);
	ICPU.Carry = (Int16 >= 0);
	SetZN8((uint8_t) Int16);
}

static INLINE void CMY16()
{
	int32_t Int32 = (int32_t) ICPU.Registers.Y.W - (int32_t) GetWord(OpAddress);
	ICPU.Carry = (Int32 >= 0);
	SetZN16((uint16_t) Int32);
}

static INLINE void CMY8()
{
	int16_t Int16 = (int16_t) ICPU.Registers.YL - (int16_t) GetByte(OpAddress);
	ICPU.Carry = (Int16 >= 0);
	SetZN8((uint8_t) Int16);
}

static INLINE void A_DEC16()
{
	CPU.WaitAddress = NULL;
	ICPU.Registers.A.W--;
	AddCycles(ONE_CYCLE);
	SetZN16(ICPU.Registers.A.W);
}

static INLINE void A_DEC8()
{
	CPU.WaitAddress = NULL;
	ICPU.Registers.AL--;
	AddCycles(ONE_CYCLE);
	SetZN8(ICPU.Registers.AL);
}

static INLINE void DEC16()
{
	uint16_t Work16;
	CPU.WaitAddress = NULL;
	Work16 = GetWord(OpAddress) - 1;
	AddCycles(ONE_CYCLE);
	SetByte(Work16 >> 8, OpAddress + 1);
	ICPU.OpenBus = Work16 & 0xff;
	SetByte(ICPU.OpenBus, OpAddress);
	SetZN16(Work16);
}

static INLINE void DEC8()
{
	uint8_t Work8;
	CPU.WaitAddress = NULL;
	Work8 = GetByte(OpAddress) - 1;
	AddCycles(ONE_CYCLE);
	SetByte(Work8, OpAddress);
	ICPU.OpenBus = Work8;
	SetZN8(Work8);
}

static INLINE void EOR16()
{
	ICPU.Registers.A.W ^= GetWord(OpAddress);
	SetZN16(ICPU.Registers.A.W);
}

static INLINE void EOR8()
{
	ICPU.Registers.AL ^= GetByte(OpAddress);
	SetZN8(ICPU.Registers.AL);
}

static INLINE void A_INC16()
{
	CPU.WaitAddress = NULL;
	ICPU.Registers.A.W++;
	AddCycles(ONE_CYCLE);
	SetZN16(ICPU.Registers.A.W);
}

static INLINE void A_INC8()
{
	CPU.WaitAddress = NULL;
	ICPU.Registers.AL++;
	AddCycles(ONE_CYCLE);
	SetZN8(ICPU.Registers.AL);
}

static INLINE void INC16()
{
	uint16_t Work16;
	CPU.WaitAddress = NULL;
	Work16 = GetWord(OpAddress) + 1;
	AddCycles(ONE_CYCLE);
	SetByte(Work16 >> 8, OpAddress + 1);
	ICPU.OpenBus = Work16 & 0xff;
	SetByte(ICPU.OpenBus, OpAddress);
	SetZN16(Work16);
}

static INLINE void INC8()
{
	uint8_t Work8;
	CPU.WaitAddress = NULL;
	Work8 = GetByte(OpAddress) + 1;
	AddCycles(ONE_CYCLE);
	SetByte(Work8, OpAddress);
	ICPU.OpenBus = Work8;
	SetZN8(Work8);
}

static INLINE void LDA16()
{
	ICPU.Registers.A.W = GetWord(OpAddress);
	SetZN16(ICPU.Registers.A.W);
}

static INLINE void LDA8()
{
	ICPU.Registers.AL = GetByte(OpAddress);
	SetZN8(ICPU.Registers.AL);
}

static INLINE void LDX16()
{
	ICPU.Registers.X.W = GetWord(OpAddress);
	SetZN16(ICPU.Registers.X.W);
}

static INLINE void LDX8()
{
	ICPU.Registers.XL = GetByte(OpAddress);
	SetZN8(ICPU.Registers.XL);
}

static INLINE void LDY16()
{
	ICPU.Registers.Y.W = GetWord(OpAddress);
	SetZN16(ICPU.Registers.Y.W);
}

static INLINE void LDY8()
{
	ICPU.Registers.YL = GetByte(OpAddress);
	SetZN8(ICPU.Registers.YL);
}

static INLINE void A_LSR16()
{
	ICPU.Carry = ICPU.Registers.AL & 1;
	ICPU.Registers.A.W >>= 1;
	AddCycles(ONE_CYCLE);
	SetZN16(ICPU.Registers.A.W);
}

static INLINE void A_LSR8()
{
	ICPU.Carry = ICPU.Registers.AL & 1;
	ICPU.Registers.AL >>= 1;
	AddCycles(ONE_CYCLE);
	SetZN8(ICPU.Registers.AL);
}

static INLINE void LSR16()
{
	uint16_t Work16 = GetWord(OpAddress);
	ICPU.Carry = Work16 & 1;
	Work16 >>= 1;
	AddCycles(ONE_CYCLE);
	SetByte(Work16 >> 8, OpAddress + 1);
	ICPU.OpenBus = Work16 & 0xff;
	SetByte(ICPU.OpenBus, OpAddress);
	SetZN16(Work16);
}

static INLINE void LSR8()
{
	uint8_t Work8 = GetByte(OpAddress);
	ICPU.Carry = Work8 & 1;
	Work8 >>= 1;
	AddCycles(ONE_CYCLE);
	SetByte(Work8, OpAddress);
	ICPU.OpenBus = Work8;
	SetZN8(Work8);
}

static INLINE void ORA16()
{
	ICPU.Registers.A.W |= GetWord(OpAddress);
	SetZN16(ICPU.Registers.A.W);
}

static INLINE void ORA8()
{
	ICPU.Registers.AL |= GetByte(OpAddress);
	SetZN8(ICPU.Registers.AL);
}

static INLINE void A_ROL16()
{
	uint32_t Work32 = (ICPU.Registers.A.W << 1) | CheckCarry();
	ICPU.Carry = (Work32 > 0xffff);
	ICPU.Registers.A.W = (uint16_t) Work32;
	AddCycles(ONE_CYCLE);
	SetZN16((uint16_t) Work32);
}

static INLINE void A_ROL8()
{
	uint16_t Work16 = ICPU.Registers.AL;
	Work16 <<= 1;
	Work16 |= CheckCarry();
	ICPU.Carry = (Work16 > 0xff);
	ICPU.Registers.AL = (uint8_t) Work16;
	AddCycles(ONE_CYCLE);
	SetZN8((uint8_t) Work16);
}

static INLINE void ROL16()
{
	uint32_t Work32 = GetWord(OpAddress);
	Work32 <<= 1;
	Work32 |= CheckCarry();
	ICPU.Carry = (Work32 > 0xffff);
	AddCycles(ONE_CYCLE);
	SetByte((Work32 >> 8) & 0xff, OpAddress + 1);
	ICPU.OpenBus = Work32 & 0xff;
	SetByte(ICPU.OpenBus, OpAddress);
	SetZN16((uint16_t) Work32);
}

static INLINE void ROL8()
{
	uint16_t Work16 = GetByte(OpAddress);
	Work16 <<= 1;
	Work16 |= CheckCarry();
	ICPU.Carry = (Work16 > 0xff);
	AddCycles(ONE_CYCLE);
	SetByte((uint8_t) Work16, OpAddress);
	ICPU.OpenBus = Work16 & 0xff;
	SetZN8((uint8_t) Work16);
}

static INLINE void A_ROR16()
{
	uint32_t Work32 = ICPU.Registers.A.W;
	Work32 |= (int32_t) CheckCarry() << 16;
	ICPU.Carry = (uint8_t) (Work32 & 1);
	Work32 >>= 1;
	ICPU.Registers.A.W = (uint16_t) Work32;
	AddCycles(ONE_CYCLE);
	SetZN16((uint16_t) Work32);
}

static INLINE void A_ROR8()
{
	uint16_t Work16 = ICPU.Registers.AL | ((uint16_t) CheckCarry() << 8);
	ICPU.Carry = (uint8_t) Work16 & 1;
	Work16 >>= 1;
	ICPU.Registers.AL = (uint8_t) Work16;
	AddCycles(ONE_CYCLE);
	SetZN8((uint8_t) Work16);
}

static INLINE void ROR16()
{
	uint32_t Work32 = GetWord(OpAddress);
	Work32 |= (int32_t) CheckCarry() << 16;
	ICPU.Carry = (uint8_t) (Work32 & 1);
	Work32 >>= 1;
	AddCycles(ONE_CYCLE);
	SetByte((Work32 >> 8) & 0xff, OpAddress + 1);
	ICPU.OpenBus = Work32 & 0xff;
	SetByte(ICPU.OpenBus, OpAddress);
	SetZN16((uint16_t) Work32);
}

static INLINE void ROR8()
{
	uint16_t Work16 = GetByte(OpAddress);
	Work16 |= (int32_t) CheckCarry() << 8;
	ICPU.Carry = (uint8_t) (Work16 & 1);
	Work16 >>= 1;
	AddCycles(ONE_CYCLE);
	SetByte((uint8_t) Work16, OpAddress);
	ICPU.OpenBus = Work16 & 0xff;
	SetZN8((uint8_t) Work16);
}

static INLINE void SBC16()
{
	uint16_t Work16 = GetWord(OpAddress);

	if (CheckDecimal())
	{
		uint16_t Ans16;
		uint8_t  A1 = (ICPU.Registers.A.W) & 0xf;
		uint8_t  A2 = (ICPU.Registers.A.W >> 4) & 0xf;
		uint8_t  A3 = (ICPU.Registers.A.W >> 8) & 0xf;
		uint8_t  A4 = (ICPU.Registers.A.W >> 12) & 0xf;
		uint8_t  W1 = Work16 & 0xf;
		uint8_t  W2 = (Work16 >> 4) & 0xf;
		uint8_t  W3 = (Work16 >> 8) & 0xf;
		uint8_t  W4 = (Work16 >> 12) & 0xf;
		A1 -= W1 + !CheckCarry();
		A2 -= W2;
		A3 -= W3;
		A4 -= W4;

		if (A1 > 9)
		{
			A1 += 10;
			A2--;
		}

		if (A2 > 9)
		{
			A2 += 10;
			A3--;
		}

		if (A3 > 9)
		{
			A3 += 10;
			A4--;
		}

		if (A4 > 9)
		{
			A4 += 10;
			ClearCarry();
		}
		else
			SetCarry();

		Ans16 = (A4 << 12) | (A3 << 8) | (A2 << 4) | (A1);

		if ((ICPU.Registers.A.W ^ Work16) & (ICPU.Registers.A.W ^ Ans16) & 0x8000)
			SetOverflow();
		else
			ClearOverflow();

		ICPU.Registers.A.W = Ans16;
	}
	else
	{
		int32_t Int32 = (int32_t) ICPU.Registers.A.W - (int32_t) Work16 + (int32_t) CheckCarry() - 1;
		ICPU.Carry = (Int32 >= 0);

		if ((ICPU.Registers.A.W ^ Work16) & (ICPU.Registers.A.W ^ (uint16_t) Int32) & 0x8000)
			SetOverflow();
		else
			ClearOverflow();

		ICPU.Registers.A.W = (uint16_t) Int32;
	}

	SetZN16(ICPU.Registers.A.W);
}

static INLINE void SBC8()
{
	uint8_t Work8 = GetByte(OpAddress);

	if (CheckDecimal())
	{
		uint8_t Ans8;
		uint8_t A1 = (ICPU.Registers.A.W) & 0xf;
		uint8_t A2 = (ICPU.Registers.A.W >> 4) & 0xf;
		uint8_t W1 = Work8 & 0xf;
		uint8_t W2 = (Work8 >> 4) & 0xf;
		A1 -= W1 + !CheckCarry();
		A2 -= W2;

		if (A1 > 9)
		{
			A1 += 10;
			A2--;
		}
		if (A2 > 9)
		{
			A2 += 10;
			ClearCarry();
		}
		else
			SetCarry();

		Ans8 = (A2 << 4) | A1;

		if ((ICPU.Registers.AL ^ Work8) & (ICPU.Registers.AL ^ Ans8) & 0x80)
			SetOverflow();
		else
			ClearOverflow();

		ICPU.Registers.AL = Ans8;
	}
	else
	{
		int16_t Int16 = (int16_t) ICPU.Registers.AL - (int16_t) Work8 + (int16_t) CheckCarry() - 1;
		ICPU.Carry = (Int16 >= 0);

		if ((ICPU.Registers.AL ^ Work8) & (ICPU.Registers.AL ^ (uint8_t) Int16) & 0x80)
			SetOverflow();
		else
			ClearOverflow();

		ICPU.Registers.AL = (uint8_t) Int16;
	}

	SetZN8(ICPU.Registers.AL);
}

static INLINE void STA16()
{
	SetWord(ICPU.Registers.A.W, OpAddress);
	ICPU.OpenBus = ICPU.Registers.AH;
}

static INLINE void STA8()
{
	SetByte(ICPU.Registers.AL, OpAddress);
	ICPU.OpenBus = ICPU.Registers.AL;
}

static INLINE void STX16()
{
	SetWord(ICPU.Registers.X.W, OpAddress);
	ICPU.OpenBus = ICPU.Registers.XH;
}

static INLINE void STX8()
{
	SetByte(ICPU.Registers.XL, OpAddress);
	ICPU.OpenBus = ICPU.Registers.XL;
}

static INLINE void STY16()
{
	SetWord(ICPU.Registers.Y.W, OpAddress);
	ICPU.OpenBus = ICPU.Registers.YH;
}

static INLINE void STY8()
{
	SetByte(ICPU.Registers.YL, OpAddress);
	ICPU.OpenBus = ICPU.Registers.YL;
}

static INLINE void STZ16()
{
	SetWord(0, OpAddress);
	ICPU.OpenBus = 0;
}

static INLINE void STZ8()
{
	SetByte(0, OpAddress);
	ICPU.OpenBus = 0;
}

static INLINE void TSB16()
{
	uint16_t Work16 = GetWord(OpAddress);
	ICPU.Zero = (bool) (Work16 & ICPU.Registers.A.W);
	Work16 |= ICPU.Registers.A.W;
	AddCycles(ONE_CYCLE);
	SetByte(Work16 >> 8, OpAddress + 1);
	ICPU.OpenBus = Work16 & 0xff;
	SetByte(ICPU.OpenBus, OpAddress);
}

static INLINE void TSB8()
{
	uint8_t Work8 = GetByte(OpAddress);
	ICPU.Zero = Work8 & ICPU.Registers.AL;
	Work8 |= ICPU.Registers.AL;
	AddCycles(ONE_CYCLE);
	SetByte(Work8, OpAddress);
	ICPU.OpenBus = Work8;
}

static INLINE void TRB16()
{
	uint16_t Work16 = GetWord(OpAddress);
	ICPU.Zero = (bool) (Work16 & ICPU.Registers.A.W);
	Work16 &= ~ICPU.Registers.A.W;
	AddCycles(ONE_CYCLE);
	SetByte(Work16 >> 8, OpAddress + 1);
	ICPU.OpenBus = Work16 & 0xff;
	SetByte(ICPU.OpenBus, OpAddress);
}

static INLINE void TRB8()
{
	uint8_t Work8 = GetByte(OpAddress);
	ICPU.Zero = Work8 & ICPU.Registers.AL;
	Work8 &= ~ICPU.Registers.AL;
	AddCycles(ONE_CYCLE);
	SetByte(Work8, OpAddress);
	ICPU.OpenBus = Work8;
}
#endif
