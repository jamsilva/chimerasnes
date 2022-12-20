#include "chisnes.h"
#include "spc700.h"
#include "memmap.h"
#include "display.h"
#include "cpuexec.h"
#include "apu.h"

int8_t   Int8  = 0;
int16_t  Int16 = 0;
int32_t  Int32 = 0;
uint8_t  W1;
uint8_t  W2;
uint8_t  Work8  = 0;
uint16_t Work16 = 0;
uint32_t Work32 = 0;

#define OP1 IAPU.PC[1]
#define OP2 IAPU.PC[2]

#define APUShutdown()                                                                        \
	if (Settings.Shutdown && (IAPU.PC == IAPU.WaitAddress1 || IAPU.PC == IAPU.WaitAddress2)) \
	{                                                                                        \
		if (IAPU.WaitCounter == 0)                                                           \
		{                                                                                    \
			if (APU.Cycles < EXT.NextAPUTimerPos && EXT.NextAPUTimerPos < CPU.Cycles)        \
				APU.Cycles = EXT.NextAPUTimerPos;                                            \
			else if (APU.Cycles < CPU.Cycles)                                                \
				APU.Cycles = CPU.Cycles;                                                     \
			                                                                                 \
			IAPU.APUExecuting = false;                                                       \
		}                                                                                    \
		else if (IAPU.WaitCounter >= 2)                                                      \
			IAPU.WaitCounter = 1;                                                            \
		else                                                                                 \
			IAPU.WaitCounter--;                                                              \
	}

#define APUSetZN8(b) \
	IAPU.Zero = (b)

#define APUSetZN16(w) \
	IAPU.Zero = ((w) != 0) | ((w) >> 8)

#define TCALL(n)                                                                                                 \
	{                                                                                                            \
		PushW(IAPU.PC - IAPU.RAM + 1);                                                                           \
		IAPU.PC = IAPU.RAM + APUGetByte(0xffc0 + ((15 - n) << 1)) + (APUGetByte(0xffc1 + ((15 - n) << 1)) << 8); \
	}

#define SBC(a, b)                                                            \
	Int16 = (int16_t) (a) - (int16_t) (b) + (int16_t) (APUCheckCarry()) - 1; \
	IAPU.Carry = Int16 >= 0;                                                 \
	                                                                         \
	if ((((a) ^ (b)) & 0x80) && (((a) ^ (uint8_t) Int16) & 0x80))            \
		APUSetOverflow();                                                    \
	else                                                                     \
		APUClearOverflow();                                                  \
	                                                                         \
	APUSetHalfCarry();                                                       \
	                                                                         \
	if (((a) ^ (b) ^ (uint8_t) Int16) & 0x10)                                \
		APUClearHalfCarry();                                                 \
	                                                                         \
	(a) = (uint8_t) Int16;                                                   \
	APUSetZN8((uint8_t) Int16)

#define ADC(a, b)                                       \
	Work16  = (a) + (b) + APUCheckCarry();              \
	IAPU.Carry = Work16 >= 0x100;                       \
	                                                    \
	if (~((a) ^ (b)) & ((b) ^ (uint8_t) Work16) & 0x80) \
		APUSetOverflow();                               \
	else                                                \
		APUClearOverflow();                             \
	                                                    \
	APUClearHalfCarry();                                \
	                                                    \
	if (((a) ^ (b) ^ (uint8_t) Work16) & 0x10)          \
		APUSetHalfCarry();                              \
	                                                    \
	(a) = (uint8_t) Work16;                             \
	APUSetZN8((uint8_t) Work16)

#define CMP(a, b)                          \
	Int16 = (int16_t) (a) - (int16_t) (b); \
	IAPU.Carry = Int16 >= 0;               \
	APUSetZN8((uint8_t) Int16)

#define ASL(b)                     \
	IAPU.Carry = ((b) &0x80) != 0; \
	(b) <<= 1;                     \
	APUSetZN8(b)

#define LSR(b)           \
	IAPU.Carry = (b) &1; \
	(b) >>= 1;           \
	APUSetZN8(b)

#define ROL(b)                             \
	Work16 = ((b) << 1) | APUCheckCarry(); \
	IAPU.Carry = Work16 >= 0x100;          \
	(b) = (uint8_t) Work16;                \
	APUSetZN8(b)

#define ROR(b)                                        \
	Work16 = (b) | ((uint16_t) APUCheckCarry() << 8); \
	IAPU.Carry = (uint8_t) Work16 & 1;                \
	Work16 >>= 1;                                     \
	(b) = (uint8_t) Work16;                           \
	APUSetZN8(b)

#define Push(b)                             \
	IAPU.RAM[0x100 + IAPU.Registers.S] = b; \
	IAPU.Registers.S--

#define Pop(b)          \
	IAPU.Registers.S++; \
	(b) = IAPU.RAM[0x100 + IAPU.Registers.S]

#ifdef MSB_FIRST
	#define PushW(w)                                     \
		IAPU.RAM[0xff + IAPU.Registers.S]  = w;          \
		IAPU.RAM[0x100 + IAPU.Registers.S] = ((w) >> 8); \
		IAPU.Registers.S -= 2

	#define PopW(w)                                         \
		IAPU.Registers.S += 2;                              \
		                                                    \
		if (IAPU.Registers.S == 0)                          \
			(w) = IAPU.RAM[0x1ff] | (IAPU.RAM[0x100] << 8); \
		else                                                \
			(w) = IAPU.RAM[0xff + IAPU.Registers.S] + (IAPU.RAM[0x100 + IAPU.Registers.S] << 8)
#else
	#define PushW(w)                                               \
		if (IAPU.Registers.S == 0)                                 \
		{                                                          \
			IAPU.RAM[0x1ff] = (w);                                 \
			IAPU.RAM[0x100] = ((w) >> 8);                          \
		}                                                          \
		else                                                       \
			*(uint16_t*) (IAPU.RAM + 0xff + IAPU.Registers.S) = w; \
		                                                           \
		IAPU.Registers.S -= 2

	#define PopW(w)                                         \
		IAPU.Registers.S += 2;                              \
		                                                    \
		if (IAPU.Registers.S == 0)                          \
			(w) = IAPU.RAM[0x1ff] | (IAPU.RAM[0x100] << 8); \
		else                                                \
			(w) = *(uint16_t*) (IAPU.RAM + 0xff + IAPU.Registers.S)
#endif

#define Relative() \
	Int8 = OP1;    \
	Int16 = (int16_t) (IAPU.PC + 2 - IAPU.RAM) + Int8;

#define Relative2() \
	Int8 = OP2;     \
	Int16 = (int16_t) (IAPU.PC + 3 - IAPU.RAM) + Int8;

#ifdef MSB_FIRST
	#define IndexedXIndirect() \
		IAPU.Address = IAPU.DirectPage[(OP1 + IAPU.Registers.X) & 0xff] + (IAPU.DirectPage[(OP1 + IAPU.Registers.X + 1) & 0xff] << 8);

	#define Absolute() \
		IAPU.Address = OP1 + (OP2 << 8);

	#define AbsoluteX() \
		IAPU.Address = OP1 + (OP2 << 8) + IAPU.Registers.X;

	#define AbsoluteY() \
		IAPU.Address = OP1 + (OP2 << 8) + IAPU.Registers.YA.B.Y;

	#define MemBit()                              \
		IAPU.Address = OP1 + (OP2 << 8);          \
		IAPU.Bit = (int8_t) (IAPU.Address >> 13); \
		IAPU.Address &= 0x1fff;

	#define IndirectIndexedY() \
		IAPU.Address = IAPU.DirectPage[OP1] + (IAPU.DirectPage[OP1 + 1] << 8) + IAPU.Registers.YA.B.Y;
#else
	#define IndexedXIndirect() \
		IAPU.Address = *(uint16_t*) (IAPU.DirectPage + ((OP1 + IAPU.Registers.X) & 0xff));

	#define Absolute() \
		IAPU.Address = *(uint16_t*) (IAPU.PC + 1);

	#define AbsoluteX() \
		IAPU.Address = *(uint16_t*) (IAPU.PC + 1) + IAPU.Registers.X;

	#define AbsoluteY() \
		IAPU.Address = *(uint16_t*) (IAPU.PC + 1) + IAPU.Registers.YA.B.Y;

	#define MemBit()                               \
		IAPU.Address = *(uint16_t*) (IAPU.PC + 1); \
		IAPU.Bit = (uint8_t) (IAPU.Address >> 13); \
		IAPU.Address &= 0x1fff;

	#define IndirectIndexedY() \
		IAPU.Address = *(uint16_t*) (IAPU.DirectPage + OP1) + IAPU.Registers.YA.B.Y;
#endif

void Apu00() /* NOP */
{
	IAPU.PC++;
}

void Apu01()
{
	TCALL(0);
}

void Apu11()
{
	TCALL(1);
}

void Apu21()
{
	TCALL(2);
}

void Apu31()
{
	TCALL(3);
}

void Apu41()
{
	TCALL(4);
}

void Apu51()
{
	TCALL(5);
}

void Apu61()
{
	TCALL(6);
}

void Apu71()
{
	TCALL(7);
}

void Apu81()
{
	TCALL(8);
}

void Apu91()
{
	TCALL(9);
}

void ApuA1()
{
	TCALL(10);
}

void ApuB1()
{
	TCALL(11);
}

void ApuC1()
{
	TCALL(12);
}

void ApuD1()
{
	TCALL(13);
}

void ApuE1()
{
	TCALL(14);
}

void ApuF1()
{
	TCALL(15);
}

void Apu3F() /* CALL absolute */
{
	Absolute();
	/* 0xB6f for Star Fox 2 */
	PushW(IAPU.PC + 3 - IAPU.RAM);
	IAPU.PC = IAPU.RAM + IAPU.Address;
}

void Apu4F() /* PCALL $XX */
{
	Work8 = OP1;
	PushW(IAPU.PC + 2 - IAPU.RAM);
	IAPU.PC = IAPU.RAM + 0xff00 + Work8;
}

#define SET(b)                                                     \
	APUSetByteDP((uint8_t) (APUGetByteDP(OP1) | (1 << (b))), OP1); \
	IAPU.PC += 2

void Apu02()
{
	SET(0);
}

void Apu22()
{
	SET(1);
}

void Apu42()
{
	SET(2);
}

void Apu62()
{
	SET(3);
}

void Apu82()
{
	SET(4);
}

void ApuA2()
{
	SET(5);
}

void ApuC2()
{
	SET(6);
}

void ApuE2()
{
	SET(7);
}

#define CLR(b)                                                      \
	APUSetByteDP((uint8_t) (APUGetByteDP(OP1) & ~(1 << (b))), OP1); \
	IAPU.PC += 2;

void Apu12()
{
	CLR(0);
}

void Apu32()
{
	CLR(1);
}

void Apu52()
{
	CLR(2);
}

void Apu72()
{
	CLR(3);
}

void Apu92()
{
	CLR(4);
}

void ApuB2()
{
	CLR(5);
}

void ApuD2()
{
	CLR(6);
}

void ApuF2()
{
	CLR(7);
}

#define BBS(b)                                 \
	Work8 = OP1;                               \
	Relative2();                               \
	                                           \
	if (APUGetByteDP(Work8) & (1 << (b)))      \
	{                                          \
		IAPU.PC = IAPU.RAM + (uint16_t) Int16; \
		APU.Cycles += (IAPU.OneCycle << 1);    \
	}                                          \
	else                                       \
		IAPU.PC += 3

void Apu03()
{
	BBS(0);
}

void Apu23()
{
	BBS(1);
}

void Apu43()
{
	BBS(2);
}

void Apu63()
{
	BBS(3);
}

void Apu83()
{
	BBS(4);
}

void ApuA3()
{
	BBS(5);
}

void ApuC3()
{
	BBS(6);
}

void ApuE3()
{
	BBS(7);
}

#define BBC(b)                                 \
	Work8 = OP1;                               \
	Relative2();                               \
	                                           \
	if (!(APUGetByteDP(Work8) & (1 << (b))))   \
	{                                          \
		IAPU.PC = IAPU.RAM + (uint16_t) Int16; \
		APU.Cycles += (IAPU.OneCycle << 1);    \
	}                                          \
	else                                       \
		IAPU.PC += 3

void Apu13()
{
	BBC(0);
}

void Apu33()
{
	BBC(1);
}

void Apu53()
{
	BBC(2);
}

void Apu73()
{
	BBC(3);
}

void Apu93()
{
	BBC(4);
}

void ApuB3()
{
	BBC(5);
}

void ApuD3()
{
	BBC(6);
}

void ApuF3()
{
	BBC(7);
}

void Apu04() /* OR A,dp */
{
	IAPU.Registers.YA.B.A |= APUGetByteDP(OP1);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 2;
}

void Apu05() /* OR A,abs */
{
	Absolute();
	IAPU.Registers.YA.B.A |= APUGetByte(IAPU.Address);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 3;
}

void Apu06() /* OR A,(X) */
{
	IAPU.Registers.YA.B.A |= APUGetByteDP(IAPU.Registers.X);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC++;
}

void Apu07() /* OR A,(dp+X) */
{
	IndexedXIndirect();
	IAPU.Registers.YA.B.A |= APUGetByte(IAPU.Address);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 2;
}

void Apu08() /* OR A,#00 */
{
	IAPU.Registers.YA.B.A |= OP1;
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 2;
}

void Apu09() /* OR dp(dest),dp(src) */
{
	Work8 = APUGetByteDP(OP1);
	Work8 |= APUGetByteDP(OP2);
	APUSetByteDP(Work8, OP2);
	APUSetZN8(Work8);
	IAPU.PC += 3;
}

void Apu14() /* OR A,dp+X */
{
	IAPU.Registers.YA.B.A |= APUGetByteDP(OP1 + IAPU.Registers.X);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 2;
}

void Apu15() /* OR A,abs+X */
{
	AbsoluteX();
	IAPU.Registers.YA.B.A |= APUGetByte(IAPU.Address);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 3;
}

void Apu16() /* OR A,abs+Y */
{
	AbsoluteY();
	IAPU.Registers.YA.B.A |= APUGetByte(IAPU.Address);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 3;
}

void Apu17() /* OR A,(dp)+Y */
{
	IndirectIndexedY();
	IAPU.Registers.YA.B.A |= APUGetByte(IAPU.Address);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 2;
}

void Apu18() /* OR dp,#00 */
{
	Work8 = OP1;
	Work8 |= APUGetByteDP(OP2);
	APUSetByteDP(Work8, OP2);
	APUSetZN8(Work8);
	IAPU.PC += 3;
}

void Apu19() /* OR (X),(Y) */
{
	Work8 = APUGetByteDP(IAPU.Registers.X) | APUGetByteDP(IAPU.Registers.YA.B.Y);
	APUSetZN8(Work8);
	APUSetByteDP(Work8, IAPU.Registers.X);
	IAPU.PC++;
}

void Apu0A() /* OR1 C,membit */
{
	MemBit();

	if (!APUCheckCarry())
		if (APUGetByte(IAPU.Address) & (1 << IAPU.Bit))
			APUSetCarry();

	IAPU.PC += 3;
}

void Apu2A() /* OR1 C,not membit */
{
	MemBit();

	if (!APUCheckCarry())
		if (!(APUGetByte(IAPU.Address) & (1 << IAPU.Bit)))
			APUSetCarry();

	IAPU.PC += 3;
}

void Apu4A() /* AND1 C,membit */
{
	MemBit();

	if (APUCheckCarry())
		if (!(APUGetByte(IAPU.Address) & (1 << IAPU.Bit)))
			APUClearCarry();

	IAPU.PC += 3;
}

void Apu6A() /* AND1 C, not membit */
{
	MemBit();

	if (APUCheckCarry())
		if ((APUGetByte(IAPU.Address) & (1 << IAPU.Bit)))
			APUClearCarry();

	IAPU.PC += 3;
}

void Apu8A() /* EOR1 C, membit */
{
	MemBit();

	if (APUGetByte(IAPU.Address) & (1 << IAPU.Bit))
	{
		if (APUCheckCarry())
			APUClearCarry();
		else
			APUSetCarry();
	}

	IAPU.PC += 3;
}

void ApuAA() /* MOV1 C,membit */
{
	MemBit();

	if (APUGetByte(IAPU.Address) & (1 << IAPU.Bit))
		APUSetCarry();
	else
		APUClearCarry();

	IAPU.PC += 3;
}

void ApuCA() /* MOV1 membit,C */
{
	MemBit();

	if (APUCheckCarry())
		APUSetByte(APUGetByte(IAPU.Address) | (1 << IAPU.Bit), IAPU.Address);
	else
		APUSetByte(APUGetByte(IAPU.Address) & ~(1 << IAPU.Bit), IAPU.Address);

	IAPU.PC += 3;
}

void ApuEA() /* NOT1 membit */
{
	MemBit();
	APUSetByte(APUGetByte(IAPU.Address) ^ (1 << IAPU.Bit), IAPU.Address);
	IAPU.PC += 3;
}

void Apu0B() /* ASL dp */
{
	Work8 = APUGetByteDP(OP1);
	ASL(Work8);
	APUSetByteDP(Work8, OP1);
	IAPU.PC += 2;
}

void Apu0C() /* ASL abs */
{
	Absolute();
	Work8 = APUGetByte(IAPU.Address);
	ASL(Work8);
	APUSetByte(Work8, IAPU.Address);
	IAPU.PC += 3;
}

void Apu1B() /* ASL dp+X */
{
	Work8 = APUGetByteDP(OP1 + IAPU.Registers.X);
	ASL(Work8);
	APUSetByteDP(Work8, OP1 + IAPU.Registers.X);
	IAPU.PC += 2;
}

void Apu1C() /* ASL A */
{
	ASL(IAPU.Registers.YA.B.A);
	IAPU.PC++;
}

void Apu0D() /* PUSH PSW */
{
	APUPackStatus();
	Push(IAPU.Registers.P);
	IAPU.PC++;
}

void Apu2D() /* PUSH A */
{
	Push(IAPU.Registers.YA.B.A);
	IAPU.PC++;
}

void Apu4D() /* PUSH X */
{
	Push(IAPU.Registers.X);
	IAPU.PC++;
}

void Apu6D() /* PUSH Y */
{
	Push(IAPU.Registers.YA.B.Y);
	IAPU.PC++;
}

void Apu8E() /* POP PSW */
{
	Pop(IAPU.Registers.P);
	APUUnpackStatus();

	if (APUCheckDirectPage())
		IAPU.DirectPage = IAPU.RAM + 0x100;
	else
		IAPU.DirectPage = IAPU.RAM;

	IAPU.PC++;
}

void ApuAE() /* POP A */
{
	Pop(IAPU.Registers.YA.B.A);
	IAPU.PC++;
}

void ApuCE() /* POP X */
{
	Pop(IAPU.Registers.X);
	IAPU.PC++;
}

void ApuEE() /* POP Y */
{
	Pop(IAPU.Registers.YA.B.Y);
	IAPU.PC++;
}

void Apu0E() /* TSET1 abs */
{
	Absolute();
	Work8 = APUGetByte(IAPU.Address);
	APUSetByte(Work8 | IAPU.Registers.YA.B.A, IAPU.Address);
	Work8 = IAPU.Registers.YA.B.A - Work8;
	APUSetZN8(Work8);
	IAPU.PC += 3;
}

void Apu4E() /* TCLR1 abs */
{
	Absolute();
	Work8 = APUGetByte(IAPU.Address);
	APUSetByte(Work8 & ~IAPU.Registers.YA.B.A, IAPU.Address);
	Work8 = IAPU.Registers.YA.B.A - Work8;
	APUSetZN8(Work8);
	IAPU.PC += 3;
}

void Apu0F() /* BRK */
{
	PushW(IAPU.PC + 1 - IAPU.RAM);
	APUPackStatus();
	Push(IAPU.Registers.P);
	APUSetBreak();
	APUClearInterrupt();
	IAPU.PC = IAPU.RAM + APUGetByte(0xffde) + (APUGetByte(0xffdf) << 8);
}

void ApuEF_FF() /* SLEEP / STOP */
{
	APU.TimerEnabled[0] = APU.TimerEnabled[1] = APU.TimerEnabled[2] = false;
	IAPU.APUExecuting                                               = false;
}

void Apu10() /* BPL */
{
	Relative();

	if (!APUCheckNegative())
	{
		IAPU.PC = IAPU.RAM + (uint16_t) Int16;
		APU.Cycles += (IAPU.OneCycle << 1);
		APUShutdown();
	}
	else
		IAPU.PC += 2;
}

void Apu30() /* BMI */
{
	Relative();

	if (APUCheckNegative())
	{
		IAPU.PC = IAPU.RAM + (uint16_t) Int16;
		APU.Cycles += (IAPU.OneCycle << 1);
		APUShutdown();
	}
	else
		IAPU.PC += 2;
}

void Apu90() /* BCC */
{
	Relative();

	if (!APUCheckCarry())
	{
		IAPU.PC = IAPU.RAM + (uint16_t) Int16;
		APU.Cycles += (IAPU.OneCycle << 1);
		APUShutdown();
	}
	else
		IAPU.PC += 2;
}

void ApuB0() /* BCS */
{
	Relative();

	if (APUCheckCarry())
	{
		IAPU.PC = IAPU.RAM + (uint16_t) Int16;
		APU.Cycles += (IAPU.OneCycle << 1);
		APUShutdown();
	}
	else
		IAPU.PC += 2;
}

void ApuD0() /* BNE */
{
	Relative();

	if (!APUCheckZero())
	{
		IAPU.PC = IAPU.RAM + (uint16_t) Int16;
		APU.Cycles += (IAPU.OneCycle << 1);
		APUShutdown();
	}
	else
		IAPU.PC += 2;
}

void ApuF0() /* BEQ */
{
	Relative();

	if (APUCheckZero())
	{
		IAPU.PC = IAPU.RAM + (uint16_t) Int16;
		APU.Cycles += (IAPU.OneCycle << 1);
		APUShutdown();
	}
	else
		IAPU.PC += 2;
}

void Apu50() /* BVC */
{
	Relative();

	if (!APUCheckOverflow())
	{
		IAPU.PC = IAPU.RAM + (uint16_t) Int16;
		APU.Cycles += (IAPU.OneCycle << 1);
	}
	else
		IAPU.PC += 2;
}

void Apu70() /* BVS */
{
	Relative();

	if (APUCheckOverflow())
	{
		IAPU.PC = IAPU.RAM + (uint16_t) Int16;
		APU.Cycles += (IAPU.OneCycle << 1);
	}
	else
		IAPU.PC += 2;
}

void Apu2F() /* BRA */
{
	Relative();
	IAPU.PC = IAPU.RAM + (uint16_t) Int16;
}

void Apu80() /* SETC */
{
	APUSetCarry();
	IAPU.PC++;
}

void ApuED() /* NOTC */
{
	IAPU.Carry ^= 1;
	IAPU.PC++;
}

void Apu40() /* SETP */
{
	APUSetDirectPage();
	IAPU.DirectPage = IAPU.RAM + 0x100;
	IAPU.PC++;
}

void Apu1A() /* DECW dp */
{
	Work16 = APUGetByteDP(OP1) + (APUGetByteDP(OP1 + 1) << 8) - 1;
	APUSetByteDP((uint8_t) Work16, OP1);
	APUSetByteDP(Work16 >> 8, OP1 + 1);
	APUSetZN16(Work16);
	IAPU.PC += 2;
}

void Apu5A() /* CMPW YA,dp */
{
	Work16     = APUGetByteDP(OP1) + (APUGetByteDP(OP1 + 1) << 8);
	Int32      = (int32_t) IAPU.Registers.YA.W - (int32_t) Work16;
	IAPU.Carry = Int32 >= 0;
	APUSetZN16((uint16_t) Int32);
	IAPU.PC += 2;
}

void Apu3A() /* INCW dp */
{
	Work16 = APUGetByteDP(OP1) + (APUGetByteDP(OP1 + 1) << 8) + 1;
	APUSetByteDP((uint8_t) Work16, OP1);
	APUSetByteDP(Work16 >> 8, OP1 + 1);
	APUSetZN16(Work16);
	IAPU.PC += 2;
}

void Apu7A() /* ADDW YA,dp */
{
	Work16     = APUGetByteDP(OP1) + (APUGetByteDP(OP1 + 1) << 8);
	Work32     = (uint32_t) IAPU.Registers.YA.W + Work16;
	IAPU.Carry = Work32 >= 0x10000;

	if (~(IAPU.Registers.YA.W ^ Work16) & (Work16 ^ (uint16_t) Work32) & 0x8000)
		APUSetOverflow();
	else
		APUClearOverflow();

	APUClearHalfCarry();

	if ((IAPU.Registers.YA.W ^ Work16 ^ (uint16_t) Work32) & 0x1000)
		APUSetHalfCarry();

	IAPU.Registers.YA.W = (uint16_t) Work32;
	APUSetZN16(IAPU.Registers.YA.W);
	IAPU.PC += 2;
}

void Apu9A() /* SUBW YA,dp */
{
	Work16 = APUGetByteDP(OP1) + (APUGetByteDP(OP1 + 1) << 8);
	Int32  = (int32_t) IAPU.Registers.YA.W - (int32_t) Work16;
	APUClearHalfCarry();
	IAPU.Carry = Int32 >= 0;

	if (((IAPU.Registers.YA.W ^ Work16) & 0x8000) && ((IAPU.Registers.YA.W ^ (uint16_t) Int32) & 0x8000))
		APUSetOverflow();
	else
		APUClearOverflow();

	APUSetHalfCarry();

	if ((IAPU.Registers.YA.W ^ Work16 ^ (uint16_t) Int32) & 0x1000)
		APUClearHalfCarry();

	IAPU.Registers.YA.W = (uint16_t) Int32;
	APUSetZN16(IAPU.Registers.YA.W);
	IAPU.PC += 2;
}

void ApuBA() /* MOVW YA,dp */
{
	IAPU.Registers.YA.B.A = APUGetByteDP(OP1);
	IAPU.Registers.YA.B.Y = APUGetByteDP(OP1 + 1);
	APUSetZN16(IAPU.Registers.YA.W);
	IAPU.PC += 2;
}

void ApuDA() /* MOVW dp,YA */
{
	APUSetByteDP(IAPU.Registers.YA.B.A, OP1);
	APUSetByteDP(IAPU.Registers.YA.B.Y, OP1 + 1);
	IAPU.PC += 2;
}

void Apu64() /* CMP A,dp */
{
	Work8 = APUGetByteDP(OP1);
	CMP(IAPU.Registers.YA.B.A, Work8);
	IAPU.PC += 2;
}

void Apu65() /* CMP A,abs */
{
	Absolute();
	Work8 = APUGetByte(IAPU.Address);
	CMP(IAPU.Registers.YA.B.A, Work8);
	IAPU.PC += 3;
}

void Apu66() /* CMP A,(X) */
{
	Work8 = APUGetByteDP(IAPU.Registers.X);
	CMP(IAPU.Registers.YA.B.A, Work8);
	IAPU.PC++;
}

void Apu67() /* CMP A,(dp+X) */
{
	IndexedXIndirect();
	Work8 = APUGetByte(IAPU.Address);
	CMP(IAPU.Registers.YA.B.A, Work8);
	IAPU.PC += 2;
}

void Apu68() /* CMP A,#00 */
{
	Work8 = OP1;
	CMP(IAPU.Registers.YA.B.A, Work8);
	IAPU.PC += 2;
}

void Apu69() /* CMP dp(dest), dp(src) */
{
	W1    = APUGetByteDP(OP1);
	Work8 = APUGetByteDP(OP2);
	CMP(Work8, W1);
	IAPU.PC += 3;
}

void Apu74() /* CMP A, dp+X */
{
	Work8 = APUGetByteDP(OP1 + IAPU.Registers.X);
	CMP(IAPU.Registers.YA.B.A, Work8);
	IAPU.PC += 2;
}

void Apu75() /* CMP A,abs+X */
{
	AbsoluteX();
	Work8 = APUGetByte(IAPU.Address);
	CMP(IAPU.Registers.YA.B.A, Work8);
	IAPU.PC += 3;
}

void Apu76() /* CMP A, abs+Y */
{
	AbsoluteY();
	Work8 = APUGetByte(IAPU.Address);
	CMP(IAPU.Registers.YA.B.A, Work8);
	IAPU.PC += 3;
}

void Apu77() /* CMP A,(dp)+Y */
{
	IndirectIndexedY();
	Work8 = APUGetByte(IAPU.Address);
	CMP(IAPU.Registers.YA.B.A, Work8);
	IAPU.PC += 2;
}

void Apu78() /* CMP dp,#00 */
{
	Work8 = OP1;
	W1    = APUGetByteDP(OP2);
	CMP(W1, Work8);
	IAPU.PC += 3;
}

void Apu79() /* CMP (X),(Y) */
{
	W1    = APUGetByteDP(IAPU.Registers.X);
	Work8 = APUGetByteDP(IAPU.Registers.YA.B.Y);
	CMP(W1, Work8);
	IAPU.PC++;
}

void Apu1E() /* CMP X,abs */
{
	Absolute();
	Work8 = APUGetByte(IAPU.Address);
	CMP(IAPU.Registers.X, Work8);
	IAPU.PC += 3;
}

void Apu3E() /* CMP X,dp */
{
	Work8 = APUGetByteDP(OP1);
	CMP(IAPU.Registers.X, Work8);
	IAPU.PC += 2;
}

void ApuC8() /* CMP X,#00 */
{
	CMP(IAPU.Registers.X, OP1);
	IAPU.PC += 2;
}

void Apu5E() /* CMP Y,abs */
{
	Absolute();
	Work8 = APUGetByte(IAPU.Address);
	CMP(IAPU.Registers.YA.B.Y, Work8);
	IAPU.PC += 3;
}

void Apu7E() /* CMP Y,dp */
{
	Work8 = APUGetByteDP(OP1);
	CMP(IAPU.Registers.YA.B.Y, Work8);
	IAPU.PC += 2;
}

void ApuAD() /* CMP Y,#00 */
{
	Work8 = OP1;
	CMP(IAPU.Registers.YA.B.Y, Work8);
	IAPU.PC += 2;
}

void Apu1F() /* JMP (abs+X) */
{
	Absolute();
	IAPU.PC = IAPU.RAM + APUGetByte(IAPU.Address + IAPU.Registers.X) + (APUGetByte(IAPU.Address + IAPU.Registers.X + 1) << 8);
}

void Apu5F() /* JMP abs */
{
	Absolute();
	IAPU.PC = IAPU.RAM + IAPU.Address;
}

void Apu20() /* CLRP */
{
	APUClearDirectPage();
	IAPU.DirectPage = IAPU.RAM;
	IAPU.PC++;
}

void Apu60() /* CLRC */
{
	APUClearCarry();
	IAPU.PC++;
}

void ApuE0() /* CLRV */
{
	APUClearHalfCarry();
	APUClearOverflow();
	IAPU.PC++;
}

void Apu24() /* AND A,dp */
{
	IAPU.Registers.YA.B.A &= APUGetByteDP(OP1);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 2;
}

void Apu25() /* AND A,abs */
{
	Absolute();
	IAPU.Registers.YA.B.A &= APUGetByte(IAPU.Address);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 3;
}

void Apu26() /* AND A,(X) */
{
	IAPU.Registers.YA.B.A &= APUGetByteDP(IAPU.Registers.X);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC++;
}

void Apu27() /* AND A,(dp+X) */
{
	IndexedXIndirect();
	IAPU.Registers.YA.B.A &= APUGetByte(IAPU.Address);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 2;
}

void Apu28() /* AND A,#00 */
{
	IAPU.Registers.YA.B.A &= OP1;
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 2;
}

void Apu29() /* AND dp(dest),dp(src) */
{
	Work8 = APUGetByteDP(OP1);
	Work8 &= APUGetByteDP(OP2);
	APUSetByteDP(Work8, OP2);
	APUSetZN8(Work8);
	IAPU.PC += 3;
}

void Apu34() /* AND A,dp+X */
{
	IAPU.Registers.YA.B.A &= APUGetByteDP(OP1 + IAPU.Registers.X);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 2;
}

void Apu35() /* AND A,abs+X */
{
	AbsoluteX();
	IAPU.Registers.YA.B.A &= APUGetByte(IAPU.Address);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 3;
}

void Apu36() /* AND A,abs+Y */
{
	AbsoluteY();
	IAPU.Registers.YA.B.A &= APUGetByte(IAPU.Address);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 3;
}

void Apu37() /* AND A,(dp)+Y */
{
	IndirectIndexedY();
	IAPU.Registers.YA.B.A &= APUGetByte(IAPU.Address);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 2;
}

void Apu38() /* AND dp,#00 */
{
	Work8 = OP1;
	Work8 &= APUGetByteDP(OP2);
	APUSetByteDP(Work8, OP2);
	APUSetZN8(Work8);
	IAPU.PC += 3;
}

void Apu39() /* AND (X),(Y) */
{
	Work8 = APUGetByteDP(IAPU.Registers.X) & APUGetByteDP(IAPU.Registers.YA.B.Y);
	APUSetZN8(Work8);
	APUSetByteDP(Work8, IAPU.Registers.X);
	IAPU.PC++;
}

void Apu2B() /* ROL dp */
{
	Work8 = APUGetByteDP(OP1);
	ROL(Work8);
	APUSetByteDP(Work8, OP1);
	IAPU.PC += 2;
}

void Apu2C() /* ROL abs */
{
	Absolute();
	Work8 = APUGetByte(IAPU.Address);
	ROL(Work8);
	APUSetByte(Work8, IAPU.Address);
	IAPU.PC += 3;
}

void Apu3B() /* ROL dp+X */
{
	Work8 = APUGetByteDP(OP1 + IAPU.Registers.X);
	ROL(Work8);
	APUSetByteDP(Work8, OP1 + IAPU.Registers.X);
	IAPU.PC += 2;
}

void Apu3C() /* ROL A */
{
	ROL(IAPU.Registers.YA.B.A);
	IAPU.PC++;
}

void Apu2E() /* CBNE dp,rel */
{
	Work8 = OP1;
	Relative2();

	if (APUGetByteDP(Work8) != IAPU.Registers.YA.B.A)
	{
		IAPU.PC = IAPU.RAM + (uint16_t) Int16;
		APU.Cycles += (IAPU.OneCycle << 1);
		APUShutdown();
	}
	else
		IAPU.PC += 3;
}

void ApuDE() /* CBNE dp+X,rel */
{
	Work8 = OP1 + IAPU.Registers.X;
	Relative2();

	if (APUGetByteDP(Work8) != IAPU.Registers.YA.B.A)
	{
		IAPU.PC = IAPU.RAM + (uint16_t) Int16;
		APU.Cycles += (IAPU.OneCycle << 1);
		APUShutdown();
	}
	else
		IAPU.PC += 3;
}

void Apu3D() /* INC X */
{
	IAPU.Registers.X++;
	APUSetZN8(IAPU.Registers.X);
	IAPU.WaitCounter++;
	IAPU.PC++;
}

void ApuFC() /* INC Y */
{
	IAPU.Registers.YA.B.Y++;
	APUSetZN8(IAPU.Registers.YA.B.Y);
	IAPU.WaitCounter++;
	IAPU.PC++;
}

void Apu1D() /* DEC X */
{
	IAPU.Registers.X--;
	APUSetZN8(IAPU.Registers.X);
	IAPU.WaitCounter++;
	IAPU.PC++;
}

void ApuDC() /* DEC Y */
{
	IAPU.Registers.YA.B.Y--;
	APUSetZN8(IAPU.Registers.YA.B.Y);
	IAPU.WaitCounter++;
	IAPU.PC++;
}

void ApuAB() /* INC dp */
{
	Work8 = APUGetByteDP(OP1) + 1;
	APUSetByteDP(Work8, OP1);
	APUSetZN8(Work8);
	IAPU.WaitCounter++;
	IAPU.PC += 2;
}

void ApuAC() /* INC abs */
{
	Absolute();
	Work8 = APUGetByte(IAPU.Address) + 1;
	APUSetByte(Work8, IAPU.Address);
	APUSetZN8(Work8);
	IAPU.WaitCounter++;
	IAPU.PC += 3;
}

void ApuBB() /* INC dp+X */
{
	Work8 = APUGetByteDP(OP1 + IAPU.Registers.X) + 1;
	APUSetByteDP(Work8, OP1 + IAPU.Registers.X);
	APUSetZN8(Work8);
	IAPU.WaitCounter++;
	IAPU.PC += 2;
}

void ApuBC() /* INC A */
{
	IAPU.Registers.YA.B.A++;
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.WaitCounter++;
	IAPU.PC++;
}

void Apu8B() /* DEC dp */
{
	Work8 = APUGetByteDP(OP1) - 1;
	APUSetByteDP(Work8, OP1);
	APUSetZN8(Work8);
	IAPU.WaitCounter++;
	IAPU.PC += 2;
}

void Apu8C() /* DEC abs */
{
	Absolute();
	Work8 = APUGetByte(IAPU.Address) - 1;
	APUSetByte(Work8, IAPU.Address);
	APUSetZN8(Work8);
	IAPU.WaitCounter++;
	IAPU.PC += 3;
}

void Apu9B() /* DEC dp+X */
{
	Work8 = APUGetByteDP(OP1 + IAPU.Registers.X) - 1;
	APUSetByteDP(Work8, OP1 + IAPU.Registers.X);
	APUSetZN8(Work8);
	IAPU.WaitCounter++;
	IAPU.PC += 2;
}

void Apu9C() /* DEC A */
{
	IAPU.Registers.YA.B.A--;
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.WaitCounter++;
	IAPU.PC++;
}

void Apu44() /* EOR A,dp */
{
	IAPU.Registers.YA.B.A ^= APUGetByteDP(OP1);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 2;
}

void Apu45() /* EOR A,abs */
{
	Absolute();
	IAPU.Registers.YA.B.A ^= APUGetByte(IAPU.Address);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 3;
}

void Apu46() /* EOR A,(X) */
{
	IAPU.Registers.YA.B.A ^= APUGetByteDP(IAPU.Registers.X);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC++;
}

void Apu47() /* EOR A,(dp+X) */
{
	IndexedXIndirect();
	IAPU.Registers.YA.B.A ^= APUGetByte(IAPU.Address);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 2;
}

void Apu48() /* EOR A,#00 */
{
	IAPU.Registers.YA.B.A ^= OP1;
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 2;
}

void Apu49() /* EOR dp(dest),dp(src) */
{
	Work8 = APUGetByteDP(OP1);
	Work8 ^= APUGetByteDP(OP2);
	APUSetByteDP(Work8, OP2);
	APUSetZN8(Work8);
	IAPU.PC += 3;
}

void Apu54() /* EOR A,dp+X */
{
	IAPU.Registers.YA.B.A ^= APUGetByteDP(OP1 + IAPU.Registers.X);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 2;
}

void Apu55() /* EOR A,abs+X */
{
	AbsoluteX();
	IAPU.Registers.YA.B.A ^= APUGetByte(IAPU.Address);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 3;
}

void Apu56() /* EOR A,abs+Y */
{
	AbsoluteY();
	IAPU.Registers.YA.B.A ^= APUGetByte(IAPU.Address);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 3;
}

void Apu57() /* EOR A,(dp)+Y */
{
	IndirectIndexedY();
	IAPU.Registers.YA.B.A ^= APUGetByte(IAPU.Address);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 2;
}

void Apu58() /* EOR dp,#00 */
{
	Work8 = OP1;
	Work8 ^= APUGetByteDP(OP2);
	APUSetByteDP(Work8, OP2);
	APUSetZN8(Work8);
	IAPU.PC += 3;
}

void Apu59() /* EOR (X),(Y) */
{
	Work8 = APUGetByteDP(IAPU.Registers.X) ^ APUGetByteDP(IAPU.Registers.YA.B.Y);
	APUSetZN8(Work8);
	APUSetByteDP(Work8, IAPU.Registers.X);
	IAPU.PC++;
}

void Apu4B() /* LSR dp */
{
	Work8 = APUGetByteDP(OP1);
	LSR(Work8);
	APUSetByteDP(Work8, OP1);
	IAPU.PC += 2;
}

void Apu4C() /* LSR abs */
{
	Absolute();
	Work8 = APUGetByte(IAPU.Address);
	LSR(Work8);
	APUSetByte(Work8, IAPU.Address);
	IAPU.PC += 3;
}

void Apu5B() /* LSR dp+X */
{
	Work8 = APUGetByteDP(OP1 + IAPU.Registers.X);
	LSR(Work8);
	APUSetByteDP(Work8, OP1 + IAPU.Registers.X);
	IAPU.PC += 2;
}

void Apu5C() /* LSR A */
{
	LSR(IAPU.Registers.YA.B.A);
	IAPU.PC++;
}

void Apu7D() /* MOV A,X */
{
	IAPU.Registers.YA.B.A = IAPU.Registers.X;
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC++;
}

void ApuDD() /* MOV A,Y */
{
	IAPU.Registers.YA.B.A = IAPU.Registers.YA.B.Y;
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC++;
}

void Apu5D() /* MOV X,A */
{
	IAPU.Registers.X = IAPU.Registers.YA.B.A;
	APUSetZN8(IAPU.Registers.X);
	IAPU.PC++;
}

void ApuFD() /* MOV Y,A */
{
	IAPU.Registers.YA.B.Y = IAPU.Registers.YA.B.A;
	APUSetZN8(IAPU.Registers.YA.B.Y);
	IAPU.PC++;
}

void Apu9D() /* MOV X,SP */
{
	IAPU.Registers.X = IAPU.Registers.S;
	APUSetZN8(IAPU.Registers.X);
	IAPU.PC++;
}

void ApuBD() /* MOV SP,X */
{
	IAPU.Registers.S = IAPU.Registers.X;
	IAPU.PC++;
}

void Apu6B() /* ROR dp */
{
	Work8 = APUGetByteDP(OP1);
	ROR(Work8);
	APUSetByteDP(Work8, OP1);
	IAPU.PC += 2;
}

void Apu6C() /* ROR abs */
{
	Absolute();
	Work8 = APUGetByte(IAPU.Address);
	ROR(Work8);
	APUSetByte(Work8, IAPU.Address);
	IAPU.PC += 3;
}

void Apu7B() /* ROR dp+X */
{
	Work8 = APUGetByteDP(OP1 + IAPU.Registers.X);
	ROR(Work8);
	APUSetByteDP(Work8, OP1 + IAPU.Registers.X);
	IAPU.PC += 2;
}

void Apu7C() /* ROR A */
{
	ROR(IAPU.Registers.YA.B.A);
	IAPU.PC++;
}

void Apu6E() /* DBNZ dp,rel */
{
	Work8 = OP1;
	Relative2();
	W1 = APUGetByteDP(Work8) - 1;
	APUSetByteDP(W1, Work8);

	if (W1 != 0)
	{
		IAPU.PC = IAPU.RAM + (uint16_t) Int16;
		APU.Cycles += (IAPU.OneCycle << 1);
	}
	else
		IAPU.PC += 3;
}

void ApuFE() /* DBNZ Y,rel */
{
	Relative();
	IAPU.Registers.YA.B.Y--;

	if (IAPU.Registers.YA.B.Y != 0)
	{
		IAPU.PC = IAPU.RAM + (uint16_t) Int16;
		APU.Cycles += (IAPU.OneCycle << 1);
	}
	else
		IAPU.PC += 2;
}

void Apu6F() /* RET */
{
	PopW(IAPU.Registers.PC);
	IAPU.PC = IAPU.RAM + IAPU.Registers.PC;
}

void Apu7F() /* RETI */
{
	Pop(IAPU.Registers.P);
	APUUnpackStatus();
	PopW(IAPU.Registers.PC);
	IAPU.PC = IAPU.RAM + IAPU.Registers.PC;
}

void Apu84() /* ADC A,dp */
{
	Work8 = APUGetByteDP(OP1);
	ADC(IAPU.Registers.YA.B.A, Work8);
	IAPU.PC += 2;
}

void Apu85() /* ADC A, abs */
{
	Absolute();
	Work8 = APUGetByte(IAPU.Address);
	ADC(IAPU.Registers.YA.B.A, Work8);
	IAPU.PC += 3;
}

void Apu86() /* ADC A,(X) */
{
	Work8 = APUGetByteDP(IAPU.Registers.X);
	ADC(IAPU.Registers.YA.B.A, Work8);
	IAPU.PC++;
}

void Apu87() /* ADC A,(dp+X) */
{
	IndexedXIndirect();
	Work8 = APUGetByte(IAPU.Address);
	ADC(IAPU.Registers.YA.B.A, Work8);
	IAPU.PC += 2;
}

void Apu88() /* ADC A,#00 */
{
	Work8 = OP1;
	ADC(IAPU.Registers.YA.B.A, Work8);
	IAPU.PC += 2;
}

void Apu89() /* ADC dp(dest),dp(src) */
{
	Work8 = APUGetByteDP(OP1);
	W1    = APUGetByteDP(OP2);
	ADC(W1, Work8);
	APUSetByteDP(W1, OP2);
	IAPU.PC += 3;
}

void Apu94() /* ADC A,dp+X */
{
	Work8 = APUGetByteDP(OP1 + IAPU.Registers.X);
	ADC(IAPU.Registers.YA.B.A, Work8);
	IAPU.PC += 2;
}

void Apu95() /* ADC A, abs+X */
{
	AbsoluteX();
	Work8 = APUGetByte(IAPU.Address);
	ADC(IAPU.Registers.YA.B.A, Work8);
	IAPU.PC += 3;
}

void Apu96() /* ADC A, abs+Y */
{
	AbsoluteY();
	Work8 = APUGetByte(IAPU.Address);
	ADC(IAPU.Registers.YA.B.A, Work8);
	IAPU.PC += 3;
}

void Apu97() /* ADC A, (dp)+Y */
{
	IndirectIndexedY();
	Work8 = APUGetByte(IAPU.Address);
	ADC(IAPU.Registers.YA.B.A, Work8);
	IAPU.PC += 2;
}

void Apu98() /* ADC dp,#00 */
{
	Work8 = OP1;
	W1    = APUGetByteDP(OP2);
	ADC(W1, Work8);
	APUSetByteDP(W1, OP2);
	IAPU.PC += 3;
}

void Apu99() /* ADC (X),(Y) */
{
	W1    = APUGetByteDP(IAPU.Registers.X);
	Work8 = APUGetByteDP(IAPU.Registers.YA.B.Y);
	ADC(W1, Work8);
	APUSetByteDP(W1, IAPU.Registers.X);
	IAPU.PC++;
}

void Apu8D() /* MOV Y,#00 */
{
	IAPU.Registers.YA.B.Y = OP1;
	APUSetZN8(IAPU.Registers.YA.B.Y);
	IAPU.PC += 2;
}

void Apu8F() /* MOV dp,#00 */
{
	Work8 = OP1;
	APUSetByteDP(Work8, OP2);
	IAPU.PC += 3;
}

void Apu9E() /* DIV YA,X */
{
	uint32_t i, yva, x;

	if ((IAPU.Registers.X & 0x0f) <= (IAPU.Registers.YA.B.Y & 0x0f))
		APUSetHalfCarry();
	else
		APUClearHalfCarry();

	yva = IAPU.Registers.YA.W;
	x = IAPU.Registers.X << 9;

	for (i = 0; i < 9; ++i)
	{
		yva <<= 1;

		if (yva & 0x20000)
			yva = (yva & 0x1ffff) | 1;

		if (yva >= x)
			yva ^= 1;

		if (yva & 1)
			yva = (yva - x) & 0x1ffff;
	}

	if (yva & 0x100)
		APUSetOverflow();
	else
		APUClearOverflow();

	IAPU.Registers.YA.B.Y = (yva >> 9) & 0xff;
	IAPU.Registers.YA.B.A = yva & 0xff;
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC++;
}

void Apu9F() /* XCN A */
{
	IAPU.Registers.YA.B.A = (IAPU.Registers.YA.B.A >> 4) | (IAPU.Registers.YA.B.A << 4);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC++;
}

void ApuA4() /* SBC A, dp */
{
	Work8 = APUGetByteDP(OP1);
	SBC(IAPU.Registers.YA.B.A, Work8);
	IAPU.PC += 2;
}

void ApuA5() /* SBC A, abs */
{
	Absolute();
	Work8 = APUGetByte(IAPU.Address);
	SBC(IAPU.Registers.YA.B.A, Work8);
	IAPU.PC += 3;
}

void ApuA6() /* SBC A, (X) */
{
	Work8 = APUGetByteDP(IAPU.Registers.X);
	SBC(IAPU.Registers.YA.B.A, Work8);
	IAPU.PC++;
}

void ApuA7() /* SBC A,(dp+X) */
{
	IndexedXIndirect();
	Work8 = APUGetByte(IAPU.Address);
	SBC(IAPU.Registers.YA.B.A, Work8);
	IAPU.PC += 2;
}

void ApuA8() /* SBC A,#00 */
{
	Work8 = OP1;
	SBC(IAPU.Registers.YA.B.A, Work8);
	IAPU.PC += 2;
}

void ApuA9() /* SBC dp(dest), dp(src) */
{
	Work8 = APUGetByteDP(OP1);
	W1    = APUGetByteDP(OP2);
	SBC(W1, Work8);
	APUSetByteDP(W1, OP2);
	IAPU.PC += 3;
}

void ApuB4() /* SBC A, dp+X */
{
	Work8 = APUGetByteDP(OP1 + IAPU.Registers.X);
	SBC(IAPU.Registers.YA.B.A, Work8);
	IAPU.PC += 2;
}

void ApuB5() /* SBC A,abs+X */
{
	AbsoluteX();
	Work8 = APUGetByte(IAPU.Address);
	SBC(IAPU.Registers.YA.B.A, Work8);
	IAPU.PC += 3;
}

void ApuB6() /* SBC A,abs+Y */
{
	AbsoluteY();
	Work8 = APUGetByte(IAPU.Address);
	SBC(IAPU.Registers.YA.B.A, Work8);
	IAPU.PC += 3;
}

void ApuB7() /* SBC A,(dp)+Y */
{
	IndirectIndexedY();
	Work8 = APUGetByte(IAPU.Address);
	SBC(IAPU.Registers.YA.B.A, Work8);
	IAPU.PC += 2;
}

void ApuB8() /* SBC dp,#00 */
{
	Work8 = OP1;
	W1    = APUGetByteDP(OP2);
	SBC(W1, Work8);
	APUSetByteDP(W1, OP2);
	IAPU.PC += 3;
}

void ApuB9() /* SBC (X),(Y) */
{
	W1    = APUGetByteDP(IAPU.Registers.X);
	Work8 = APUGetByteDP(IAPU.Registers.YA.B.Y);
	SBC(W1, Work8);
	APUSetByteDP(W1, IAPU.Registers.X);
	IAPU.PC++;
}

void ApuAF() /* MOV (X)+, A */
{
	APUSetByteDP(IAPU.Registers.YA.B.A, IAPU.Registers.X++);
	IAPU.PC++;
}

void ApuBE() /* DAS */
{
	if (IAPU.Registers.YA.B.A > 0x99 || !IAPU.Carry)
	{
		IAPU.Registers.YA.B.A -= 0x60;
		APUClearCarry();
	}
	else
		APUSetCarry();

	if ((IAPU.Registers.YA.B.A & 0x0f) > 9 || !APUCheckHalfCarry())
		IAPU.Registers.YA.B.A -= 6;

	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC++;
}

void ApuBF() /* MOV A,(X)+ */
{
	IAPU.Registers.YA.B.A = APUGetByteDP(IAPU.Registers.X++);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC++;
}

void ApuC0() /* DI */
{
	APUClearInterrupt();
	IAPU.PC++;
}

void ApuA0() /* EI */
{
	APUSetInterrupt();
	IAPU.PC++;
}

void ApuC4() /* MOV dp,A */
{
	APUSetByteDP(IAPU.Registers.YA.B.A, OP1);
	IAPU.PC += 2;
}

void ApuC5() /* MOV abs,A */
{
	Absolute();
	APUSetByte(IAPU.Registers.YA.B.A, IAPU.Address);
	IAPU.PC += 3;
}

void ApuC6() /* MOV (X), A */
{
	APUSetByteDP(IAPU.Registers.YA.B.A, IAPU.Registers.X);
	IAPU.PC++;
}

void ApuC7() /* MOV (dp+X),A */
{
	IndexedXIndirect();
	APUSetByte(IAPU.Registers.YA.B.A, IAPU.Address);
	IAPU.PC += 2;
}

void ApuC9() /* MOV abs,X */
{
	Absolute();
	APUSetByte(IAPU.Registers.X, IAPU.Address);
	IAPU.PC += 3;
}

void ApuCB() /* MOV dp,Y */
{
	APUSetByteDP(IAPU.Registers.YA.B.Y, OP1);
	IAPU.PC += 2;
}

void ApuCC() /* MOV abs,Y */
{
	Absolute();
	APUSetByte(IAPU.Registers.YA.B.Y, IAPU.Address);
	IAPU.PC += 3;
}

void ApuCD() /* MOV X,#00 */
{
	IAPU.Registers.X = OP1;
	APUSetZN8(IAPU.Registers.X);
	IAPU.PC += 2;
}

void ApuCF() /* MUL YA */
{
	IAPU.Registers.YA.W = (uint16_t) IAPU.Registers.YA.B.A * IAPU.Registers.YA.B.Y;
	APUSetZN8(IAPU.Registers.YA.B.Y);
	IAPU.PC++;
}

void ApuD4() /* MOV dp+X, A */
{
	APUSetByteDP(IAPU.Registers.YA.B.A, OP1 + IAPU.Registers.X);
	IAPU.PC += 2;
}

void ApuD5() /* MOV abs+X,A */
{
	AbsoluteX();
	APUSetByte(IAPU.Registers.YA.B.A, IAPU.Address);
	IAPU.PC += 3;
}

void ApuD6() /* MOV abs+Y,A */
{
	AbsoluteY();
	APUSetByte(IAPU.Registers.YA.B.A, IAPU.Address);
	IAPU.PC += 3;
}

void ApuD7() /* MOV (dp)+Y,A */
{
	IndirectIndexedY();
	APUSetByte(IAPU.Registers.YA.B.A, IAPU.Address);
	IAPU.PC += 2;
}

void ApuD8() /* MOV dp,X */
{
	APUSetByteDP(IAPU.Registers.X, OP1);
	IAPU.PC += 2;
}

void ApuD9() /* MOV dp+Y,X */
{
	APUSetByteDP(IAPU.Registers.X, OP1 + IAPU.Registers.YA.B.Y);
	IAPU.PC += 2;
}

void ApuDB() /* MOV dp+X,Y */
{
	APUSetByteDP(IAPU.Registers.YA.B.Y, OP1 + IAPU.Registers.X);
	IAPU.PC += 2;
}

void ApuDF() /* DAA */
{
	if (IAPU.Registers.YA.B.A > 0x99 || IAPU.Carry)
	{
		IAPU.Registers.YA.B.A += 0x60;
		APUSetCarry();
	}
	else
		APUClearCarry();

	if ((IAPU.Registers.YA.B.A & 0x0f) > 9 || APUCheckHalfCarry())
		IAPU.Registers.YA.B.A += 6;

	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC++;
}

void ApuE4() /* MOV A, dp */
{
	IAPU.Registers.YA.B.A = APUGetByteDP(OP1);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 2;
}

void ApuE5() /* MOV A,abs */
{
	Absolute();
	IAPU.Registers.YA.B.A = APUGetByte(IAPU.Address);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 3;
}

void ApuE6() /* MOV A,(X) */
{
	IAPU.Registers.YA.B.A = APUGetByteDP(IAPU.Registers.X);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC++;
}

void ApuE7() /* MOV A,(dp+X) */
{
	IndexedXIndirect();
	IAPU.Registers.YA.B.A = APUGetByte(IAPU.Address);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 2;
}

void ApuE8() /* MOV A,#00 */
{
	IAPU.Registers.YA.B.A = OP1;
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 2;
}

void ApuE9() /* MOV X, abs */
{
	Absolute();
	IAPU.Registers.X = APUGetByte(IAPU.Address);
	APUSetZN8(IAPU.Registers.X);
	IAPU.PC += 3;
}

void ApuEB() /* MOV Y,dp */
{
	IAPU.Registers.YA.B.Y = APUGetByteDP(OP1);
	APUSetZN8(IAPU.Registers.YA.B.Y);
	IAPU.PC += 2;
}

void ApuEC() /* MOV Y,abs */
{
	Absolute();
	IAPU.Registers.YA.B.Y = APUGetByte(IAPU.Address);
	APUSetZN8(IAPU.Registers.YA.B.Y);
	IAPU.PC += 3;
}

void ApuF4() /* MOV A, dp+X */
{
	IAPU.Registers.YA.B.A = APUGetByteDP(OP1 + IAPU.Registers.X);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 2;
}

void ApuF5() /* MOV A, abs+X */
{
	AbsoluteX();
	IAPU.Registers.YA.B.A = APUGetByte(IAPU.Address);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 3;
}

void ApuF6() /* MOV A, abs+Y */
{
	AbsoluteY();
	IAPU.Registers.YA.B.A = APUGetByte(IAPU.Address);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 3;
}

void ApuF7() /* MOV A, (dp)+Y */
{
	IndirectIndexedY();
	IAPU.Registers.YA.B.A = APUGetByte(IAPU.Address);
	APUSetZN8(IAPU.Registers.YA.B.A);
	IAPU.PC += 2;
}

void ApuF8() /* MOV X,dp */
{
	IAPU.Registers.X = APUGetByteDP(OP1);
	APUSetZN8(IAPU.Registers.X);
	IAPU.PC += 2;
}

void ApuF9() /* MOV X,dp+Y */
{
	IAPU.Registers.X = APUGetByteDP(OP1 + IAPU.Registers.YA.B.Y);
	APUSetZN8(IAPU.Registers.X);
	IAPU.PC += 2;
}

void ApuFA() /* MOV dp(dest),dp(src) */
{
	APUSetByteDP(APUGetByteDP(OP1), OP2);
	IAPU.PC += 3;
}

void ApuFB() /* MOV Y,dp+X */
{
	IAPU.Registers.YA.B.Y = APUGetByteDP(OP1 + IAPU.Registers.X);
	APUSetZN8(IAPU.Registers.YA.B.Y);
	IAPU.PC += 2;
}

void (*ApuOpcodes[256])() =
{
	Apu00, Apu01, Apu02, Apu03, Apu04, Apu05, Apu06, Apu07, Apu08, Apu09, Apu0A, Apu0B, Apu0C, Apu0D, Apu0E, Apu0F,
	Apu10, Apu11, Apu12, Apu13, Apu14, Apu15, Apu16, Apu17, Apu18, Apu19, Apu1A, Apu1B, Apu1C, Apu1D, Apu1E, Apu1F,
	Apu20, Apu21, Apu22, Apu23, Apu24, Apu25, Apu26, Apu27, Apu28, Apu29, Apu2A, Apu2B, Apu2C, Apu2D, Apu2E, Apu2F,
	Apu30, Apu31, Apu32, Apu33, Apu34, Apu35, Apu36, Apu37, Apu38, Apu39, Apu3A, Apu3B, Apu3C, Apu3D, Apu3E, Apu3F,
	Apu40, Apu41, Apu42, Apu43, Apu44, Apu45, Apu46, Apu47, Apu48, Apu49, Apu4A, Apu4B, Apu4C, Apu4D, Apu4E, Apu4F,
	Apu50, Apu51, Apu52, Apu53, Apu54, Apu55, Apu56, Apu57, Apu58, Apu59, Apu5A, Apu5B, Apu5C, Apu5D, Apu5E, Apu5F,
	Apu60, Apu61, Apu62, Apu63, Apu64, Apu65, Apu66, Apu67, Apu68, Apu69, Apu6A, Apu6B, Apu6C, Apu6D, Apu6E, Apu6F,
	Apu70, Apu71, Apu72, Apu73, Apu74, Apu75, Apu76, Apu77, Apu78, Apu79, Apu7A, Apu7B, Apu7C, Apu7D, Apu7E, Apu7F,
	Apu80, Apu81, Apu82, Apu83, Apu84, Apu85, Apu86, Apu87, Apu88, Apu89, Apu8A, Apu8B, Apu8C, Apu8D, Apu8E, Apu8F,
	Apu90, Apu91, Apu92, Apu93, Apu94, Apu95, Apu96, Apu97, Apu98, Apu99, Apu9A, Apu9B, Apu9C, Apu9D, Apu9E, Apu9F,
	ApuA0, ApuA1, ApuA2, ApuA3, ApuA4, ApuA5, ApuA6, ApuA7, ApuA8, ApuA9, ApuAA, ApuAB, ApuAC, ApuAD, ApuAE, ApuAF,
	ApuB0, ApuB1, ApuB2, ApuB3, ApuB4, ApuB5, ApuB6, ApuB7, ApuB8, ApuB9, ApuBA, ApuBB, ApuBC, ApuBD, ApuBE, ApuBF,
	ApuC0, ApuC1, ApuC2, ApuC3, ApuC4, ApuC5, ApuC6, ApuC7, ApuC8, ApuC9, ApuCA, ApuCB, ApuCC, ApuCD, ApuCE, ApuCF,
	ApuD0, ApuD1, ApuD2, ApuD3, ApuD4, ApuD5, ApuD6, ApuD7, ApuD8, ApuD9, ApuDA, ApuDB, ApuDC, ApuDD, ApuDE, ApuDF,
	ApuE0, ApuE1, ApuE2, ApuE3, ApuE4, ApuE5, ApuE6, ApuE7, ApuE8, ApuE9, ApuEA, ApuEB, ApuEC, ApuED, ApuEE, ApuEF_FF,
	ApuF0, ApuF1, ApuF2, ApuF3, ApuF4, ApuF5, ApuF6, ApuF7, ApuF8, ApuF9, ApuFA, ApuFB, ApuFC, ApuFD, ApuFE, ApuEF_FF
};
