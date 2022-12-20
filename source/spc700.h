#ifndef CHIMERASNES_SPC700_H_
#define CHIMERASNES_SPC700_H_

enum
{
	APU_CARRY       = 1 << 0,
	APU_ZERO        = 1 << 1,
	APU_INTERRUPT   = 1 << 2,
	APU_HALF_CARRY  = 1 << 3,
	APU_BREAK       = 1 << 4,
	APU_DIRECT_PAGE = 1 << 5,
	APU_OVERFLOW    = 1 << 6,
	APU_NEGATIVE    = 1 << 7
};

#define APUCheckZero()       (!IAPU.Zero)
#define APUCheckNegative()   (IAPU.Zero & 0x80)
#define APUCheckCarry()      (IAPU.Carry)
#define APUClearCarry()      (IAPU.Carry = 0)
#define APUSetCarry()        (IAPU.Carry = 1)
#define APUCheckInterrupt()  (IAPU.Registers.P &   APU_INTERRUPT)
#define APUSetInterrupt()    (IAPU.Registers.P |=  APU_INTERRUPT)
#define APUClearInterrupt()  (IAPU.Registers.P &= ~APU_INTERRUPT)
#define APUCheckHalfCarry()  (IAPU.Registers.P &   APU_HALF_CARRY)
#define APUSetHalfCarry()    (IAPU.Registers.P |=  APU_HALF_CARRY)
#define APUClearHalfCarry()  (IAPU.Registers.P &= ~APU_HALF_CARRY)
#define APUCheckBreak()      (IAPU.Registers.P &   APU_BREAK)
#define APUSetBreak()        (IAPU.Registers.P |=  APU_BREAK)
#define APUClearBreak()      (IAPU.Registers.P &= ~APU_BREAK)
#define APUCheckDirectPage() (IAPU.Registers.P &   APU_DIRECT_PAGE)
#define APUSetDirectPage()   (IAPU.Registers.P |=  APU_DIRECT_PAGE)
#define APUClearDirectPage() (IAPU.Registers.P &= ~APU_DIRECT_PAGE)
#define APUCheckOverflow()   (IAPU.Overflow)
#define APUSetOverflow()     (IAPU.Overflow = 1)
#define APUClearOverflow()   (IAPU.Overflow = 0)

typedef union
{
	struct
	{
#ifdef MSB_FIRST
		uint8_t Y, A;
#else
		uint8_t A, Y;
#endif
	} B;

	uint16_t W;
} YAndA;

typedef struct
{
	int8_t   _SAPURegisters_PAD1 : 8;
	uint8_t  P;
	uint8_t  X;
	uint8_t  S;
	uint16_t PC;
	YAndA    YA;
} SAPURegisters;
#endif
