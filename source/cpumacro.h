#ifndef CHIMERASNES_CPUMACRO_H_
#define CHIMERASNES_CPUMACRO_H_

#include <retro_inline.h>

#ifdef SA1_OPCODES
	#define AddCycles(cycles) {}
#else
	#define AddCycles(cycles) CPU.Cycles += cycles
#endif

static INLINE void SetZN16(uint16_t Work16)
{
	ICPU.Zero = (bool) Work16;
	ICPU.Negative = (uint8_t) (Work16 >> 8);
}

static INLINE void SetZN8(uint8_t Work8)
{
	ICPU.Zero = (bool) Work8;
	ICPU.Negative = Work8;
}

static INLINE void ADC16(uint16_t Work16)
{
	if (CheckDecimal())
	{
		uint32_t carry  = CheckCarry();
		uint32_t result = (ICPU.Registers.A.W & 0x000f) + (Work16 & 0x000f) + carry;

		if (result > 0x0009)
			result += 0x0006;

		carry = (result > 0x000f);
		result = (ICPU.Registers.A.W & 0x00f0) + (Work16 & 0x00f0) + (result & 0x000f) + carry * 0x10;

		if (result > 0x009f)
			result += 0x0060;

		carry = (result > 0x00ff);
		result = (ICPU.Registers.A.W & 0x0f00) + (Work16 & 0x0f00) + (result & 0x00ff) + carry * 0x100;

		if (result > 0x09ff)
			result += 0x0600;

		carry = (result > 0x0fff);
		result = (ICPU.Registers.A.W & 0xf000) + (Work16 & 0xf000) + (result & 0x0fff) + carry * 0x1000;

		if ((ICPU.Registers.A.W & 0x8000) == (Work16 & 0x8000) && (ICPU.Registers.A.W & 0x8000) != (result & 0x8000))
			SetOverflow();
		else
			ClearOverflow();

		if (result > 0x9fff)
			result += 0x6000;

		if (result > 0xffff)
			SetCarry();
		else
			ClearCarry();

		ICPU.Registers.A.W = result & 0xffff;
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

static INLINE void ADC8(uint8_t Work8)
{
	if (CheckDecimal())
	{
		uint32_t carry = CheckCarry();
		uint32_t result = (ICPU.Registers.AL & 0x0f) + (Work8 & 0x0f) + carry;

		if (result > 0x09)
			result += 0x06;

		carry = (result > 0x0f);
		result = (ICPU.Registers.AL & 0xf0) + (Work8 & 0xf0) + (result & 0x0f) + (carry * 0x10);

		if ((ICPU.Registers.AL & 0x80) == (Work8 & 0x80) && (ICPU.Registers.AL & 0x80) != (result & 0x80))
			SetOverflow();
		else
			ClearOverflow();

		if (result > 0x9f)
			result += 0x60;

		if (result > 0xff)
			SetCarry();
		else
			ClearCarry();

		ICPU.Registers.AL = result & 0xff;
	}
	else
	{
		uint16_t Ans16 = ICPU.Registers.AL + Work8 + CheckCarry();
		ICPU.Carry = (Ans16 > 0xff);

		if (~(ICPU.Registers.AL ^ Work8) & (Work8 ^ (uint8_t) Ans16) & 0x80)
			SetOverflow();
		else
			ClearOverflow();

		ICPU.Registers.AL = (uint8_t) Ans16;
	}

	SetZN8(ICPU.Registers.AL);
}

static INLINE void AND16(uint16_t Work16)
{
	ICPU.Registers.A.W &= Work16;
	SetZN16(ICPU.Registers.A.W);
}

static INLINE void AND8(uint8_t Work8)
{
	ICPU.Registers.AL &= Work8;
	SetZN8(ICPU.Registers.AL);
}

static INLINE void ASL16(uint32_t OpAddress, wrap_t w)
{
	uint16_t Work16 = GetWord(OpAddress, w);
	ICPU.Carry = (bool) (Work16 & 0x8000);
	Work16 <<= 1;
	AddCycles(Settings.OneCycle);
	SetWord(Work16, OpAddress, w, WRITE_10);
	ICPU.OpenBus = Work16 & 0xff;
	SetZN16(Work16);
}

static INLINE void ASL8(uint32_t OpAddress)
{
	uint8_t Work8 = GetByte(OpAddress);
	ICPU.Carry = (bool) (Work8 & 0x80);
	Work8 <<= 1;
	AddCycles(Settings.OneCycle);
	SetByte(Work8, OpAddress);
	ICPU.OpenBus = Work8;
	SetZN8(Work8);
}

static INLINE void BIT16(uint16_t Work16)
{
	ICPU.Overflow = (bool) (Work16 & 0x4000);
	ICPU.Negative = (uint8_t) (Work16 >> 8);
	ICPU.Zero = (bool) (Work16 & ICPU.Registers.A.W);
}

static INLINE void BIT8(uint8_t Work8)
{
	ICPU.Overflow = (bool) (Work8 & 0x40);
	ICPU.Negative = Work8;
	ICPU.Zero = Work8 & ICPU.Registers.AL;
}

static INLINE void CMP16(uint16_t val)
{
	int32_t Int32 = (int32_t) ICPU.Registers.A.W - (int32_t) val;
	ICPU.Carry = (Int32 >= 0);
	SetZN16((uint16_t) Int32);
}

static INLINE void CMP8(uint8_t val)
{
	int16_t Int16 = (int16_t) ICPU.Registers.AL - (int16_t) val;
	ICPU.Carry = (Int16 >= 0);
	SetZN8((uint8_t) Int16);
}

static INLINE void CPX16(uint16_t val)
{
	int32_t Int32 = (int32_t) ICPU.Registers.X.W - (int32_t) val;
	ICPU.Carry = (Int32 >= 0);
	SetZN16((uint16_t) Int32);
}

static INLINE void CPX8(uint8_t val)
{
	int16_t Int16 = (int16_t) ICPU.Registers.XL - (int16_t) val;
	ICPU.Carry = (Int16 >= 0);
	SetZN8((uint8_t) Int16);
}

static INLINE void CPY16(uint16_t val)
{
	int32_t Int32 = (int32_t) ICPU.Registers.Y.W - (int32_t) val;
	ICPU.Carry = (Int32 >= 0);
	SetZN16((uint16_t) Int32);
}

static INLINE void CPY8(uint8_t val)
{
	int16_t Int16 = (int16_t) ICPU.Registers.YL - (int16_t) val;
	ICPU.Carry = (Int16 >= 0);
	SetZN8((uint8_t) Int16);
}

static INLINE void DEC16(uint32_t OpAddress, wrap_t w)
{
	uint16_t Work16;
	CPU.WaitPC = 0;
	Work16 = GetWord(OpAddress, w) - 1;
	AddCycles(Settings.OneCycle);
	SetWord(Work16, OpAddress, w, WRITE_10);
	ICPU.OpenBus = Work16 & 0xff;
	SetZN16(Work16);
}

static INLINE void DEC8(uint32_t OpAddress)
{
	uint8_t Work8;
	CPU.WaitPC = 0;
	Work8 = GetByte(OpAddress) - 1;
	AddCycles(Settings.OneCycle);
	SetByte(Work8, OpAddress);
	ICPU.OpenBus = Work8;
	SetZN8(Work8);
}

static INLINE void EOR16(uint16_t val)
{
	ICPU.Registers.A.W ^= val;
	SetZN16(ICPU.Registers.A.W);
}

static INLINE void EOR8(uint8_t val)
{
	ICPU.Registers.AL ^= val;
	SetZN8(ICPU.Registers.AL);
}

static INLINE void INC16(uint32_t OpAddress, wrap_t w)
{
	uint16_t Work16;
	CPU.WaitPC = 0;
	Work16 = GetWord(OpAddress, w) + 1;
	AddCycles(Settings.OneCycle);
	SetWord(Work16, OpAddress, w, WRITE_10);
	ICPU.OpenBus = Work16 & 0xff;
	SetZN16(Work16);
}

static INLINE void INC8(uint32_t OpAddress)
{
	uint8_t Work8;
	CPU.WaitPC = 0;
	Work8 = GetByte(OpAddress) + 1;
	AddCycles(Settings.OneCycle);
	SetByte(Work8, OpAddress);
	ICPU.OpenBus = Work8;
	SetZN8(Work8);
}

static INLINE void LDA16(uint16_t val)
{
	ICPU.Registers.A.W = val;
	SetZN16(ICPU.Registers.A.W);
}

static INLINE void LDA8(uint8_t val)
{
	ICPU.Registers.AL = val;
	SetZN8(ICPU.Registers.AL);
}

static INLINE void LDX16(uint16_t val)
{
	ICPU.Registers.X.W = val;
	SetZN16(ICPU.Registers.X.W);
}

static INLINE void LDX8(uint8_t val)
{
	ICPU.Registers.XL = val;
	SetZN8(ICPU.Registers.XL);
}

static INLINE void LDY16(uint16_t val)
{
	ICPU.Registers.Y.W = val;
	SetZN16(ICPU.Registers.Y.W);
}

static INLINE void LDY8(uint8_t val)
{
	ICPU.Registers.YL = val;
	SetZN8(ICPU.Registers.YL);
}

static INLINE void LSR16(uint32_t OpAddress, wrap_t w)
{
	uint16_t Work16 = GetWord(OpAddress, w);
	ICPU.Carry = Work16 & 1;
	Work16 >>= 1;
	AddCycles(Settings.OneCycle);
	SetWord(Work16, OpAddress, w, WRITE_10);
	ICPU.OpenBus = Work16 & 0xff;
	SetZN16(Work16);
}

static INLINE void LSR8(uint32_t OpAddress)
{
	uint8_t Work8 = GetByte(OpAddress);
	ICPU.Carry = Work8 & 1;
	Work8 >>= 1;
	AddCycles(Settings.OneCycle);
	SetByte(Work8, OpAddress);
	ICPU.OpenBus = Work8;
	SetZN8(Work8);
}

static INLINE void ORA16(uint16_t val)
{
	ICPU.Registers.A.W |= val;
	SetZN16(ICPU.Registers.A.W);
}

static INLINE void ORA8(uint8_t val)
{
	ICPU.Registers.AL |= val;
	SetZN8(ICPU.Registers.AL);
}

static INLINE void ROL16(uint32_t OpAddress, wrap_t w)
{
	uint32_t Work32 = (((uint32_t) GetWord(OpAddress, w)) << 1) | CheckCarry();
	ICPU.Carry = (Work32 > 0xffff);
	AddCycles(Settings.OneCycle);
	SetWord((uint16_t) Work32, OpAddress, w, WRITE_10);
	ICPU.OpenBus = Work32 & 0xff;
	SetZN16((uint16_t) Work32);
}

static INLINE void ROL8(uint32_t OpAddress)
{
	uint16_t Work16 = (((uint16_t) GetByte(OpAddress)) << 1) | CheckCarry();
	ICPU.Carry = (Work16 > 0xff);
	AddCycles(Settings.OneCycle);
	SetByte((uint8_t) Work16, OpAddress);
	ICPU.OpenBus = Work16 & 0xff;
	SetZN8((uint8_t) Work16);
}

static INLINE void ROR16(uint32_t OpAddress, wrap_t w)
{
	uint32_t Work32 = ((uint32_t) GetWord(OpAddress, w)) | (((uint32_t) CheckCarry()) << 16);
	ICPU.Carry = Work32 & 1;
	Work32 >>= 1;
	AddCycles(Settings.OneCycle);
	SetWord((uint16_t) Work32, OpAddress, w, WRITE_10);
	ICPU.OpenBus = Work32 & 0xff;
	SetZN16((uint16_t) Work32);
}

static INLINE void ROR8(uint32_t OpAddress)
{
	uint16_t Work16 = ((uint16_t) GetByte(OpAddress)) | (((uint16_t) CheckCarry()) << 8);
	ICPU.Carry = (bool) (Work16 & 1);
	Work16 >>= 1;
	AddCycles(Settings.OneCycle);
	SetByte((uint8_t) Work16, OpAddress);
	ICPU.OpenBus = Work16 & 0xff;
	SetZN8((uint8_t) Work16);
}

static INLINE void SBC16(uint16_t Work16)
{
	if (CheckDecimal())
	{
		int32_t result;
		int32_t carry = CheckCarry();
		Work16 ^= 0xffff;
		result = (ICPU.Registers.A.W & 0x000f) + (Work16 & 0x000f) + carry;

		if (result < 0x0010)
			result -= 0x0006;

		carry = (result > 0x000f);
		result = (ICPU.Registers.A.W & 0x00f0) + (Work16 & 0x00f0) + (result & 0x000f) + carry * 0x10;

		if (result < 0x0100)
			result -= 0x0060;

		carry = (result > 0x00ff);
		result = (ICPU.Registers.A.W & 0x0f00) + (Work16 & 0x0f00) + (result & 0x00ff) + carry * 0x100;

		if (result < 0x1000)
			result -= 0x0600;

		carry = (result > 0x0fff);
		result = (ICPU.Registers.A.W & 0xf000) + (Work16 & 0xf000) + (result & 0x0fff) + carry * 0x1000;

		if (((ICPU.Registers.A.W ^ Work16) & 0x8000) == 0 && ((ICPU.Registers.A.W ^ result) & 0x8000))
			SetOverflow();
		else
			ClearOverflow();

		if (result <= 0xffff)
			result -= 0x6000;

		if (result > 0xffff)
			SetCarry();
		else
			ClearCarry();

		ICPU.Registers.A.W = result & 0xffff;
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

static INLINE void SBC8(uint8_t Work8)
{
	if (CheckDecimal())
	{
		int32_t result;
		int32_t carry = CheckCarry();
		Work8 ^= 0xff;
		result = (ICPU.Registers.AL & 0x0f) + (Work8 & 0x0f) + carry;

		if (result < 0x10)
			result -= 0x06;

		carry = (result > 0x0f);
		result = (ICPU.Registers.AL & 0xf0) + (Work8 & 0xf0) + (result & 0x0f) + carry * 0x10;

		if ((ICPU.Registers.AL & 0x80) == (Work8 & 0x80) && (ICPU.Registers.AL & 0x80) != (result & 0x80))
			SetOverflow();
		else
			ClearOverflow();

		if (result <= 0xff)
			result -= 0x60;

		if (result > 0xff)
			SetCarry();
		else
			ClearCarry();

		ICPU.Registers.AL = result & 0xff;
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

static INLINE void STA16(uint32_t OpAddress, wrap_t w)
{
	SetWord(ICPU.Registers.A.W, OpAddress, w, WRITE_01);
	ICPU.OpenBus = ICPU.Registers.AH;
}

static INLINE void STA8(uint32_t OpAddress)
{
	SetByte(ICPU.Registers.AL, OpAddress);
	ICPU.OpenBus = ICPU.Registers.AL;
}

static INLINE void STX16(uint32_t OpAddress, wrap_t w)
{
	SetWord(ICPU.Registers.X.W, OpAddress, w, WRITE_01);
	ICPU.OpenBus = ICPU.Registers.XH;
}

static INLINE void STX8(uint32_t OpAddress)
{
	SetByte(ICPU.Registers.XL, OpAddress);
	ICPU.OpenBus = ICPU.Registers.XL;
}

static INLINE void STY16(uint32_t OpAddress, wrap_t w)
{
	SetWord(ICPU.Registers.Y.W, OpAddress, w, WRITE_01);
	ICPU.OpenBus = ICPU.Registers.YH;
}

static INLINE void STY8(uint32_t OpAddress)
{
	SetByte(ICPU.Registers.YL, OpAddress);
	ICPU.OpenBus = ICPU.Registers.YL;
}

static INLINE void STZ16(uint32_t OpAddress, wrap_t w)
{
	SetWord(0, OpAddress, w, WRITE_01);
	ICPU.OpenBus = 0;
}

static INLINE void STZ8(uint32_t OpAddress)
{
	SetByte(0, OpAddress);
	ICPU.OpenBus = 0;
}

static INLINE void TSB16(uint32_t OpAddress, wrap_t w)
{
	uint16_t Work16 = GetWord(OpAddress, w);
	ICPU.Zero = (bool) (Work16 & ICPU.Registers.A.W);
	Work16 |= ICPU.Registers.A.W;
	AddCycles(Settings.OneCycle);
	SetWord(Work16, OpAddress, w, WRITE_10);
	ICPU.OpenBus = Work16 & 0xff;
}

static INLINE void TSB8(uint32_t OpAddress)
{
	uint8_t Work8 = GetByte(OpAddress);
	ICPU.Zero = Work8 & ICPU.Registers.AL;
	Work8 |= ICPU.Registers.AL;
	AddCycles(Settings.OneCycle);
	SetByte(Work8, OpAddress);
	ICPU.OpenBus = Work8;
}

static INLINE void TRB16(uint32_t OpAddress, wrap_t w)
{
	uint16_t Work16 = GetWord(OpAddress, w);
	ICPU.Zero = (bool) (Work16 & ICPU.Registers.A.W);
	Work16 &= ~ICPU.Registers.A.W;
	AddCycles(Settings.OneCycle);
	SetWord(Work16, OpAddress, w, WRITE_10);
	ICPU.OpenBus = Work16 & 0xff;
}

static INLINE void TRB8(uint32_t OpAddress)
{
	uint8_t Work8 = GetByte(OpAddress);
	ICPU.Zero = Work8 & ICPU.Registers.AL;
	Work8 &= ~ICPU.Registers.AL;
	AddCycles(Settings.OneCycle);
	SetByte(Work8, OpAddress);
	ICPU.OpenBus = Work8;
}
#endif
