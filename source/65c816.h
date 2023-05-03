#ifndef CHIMERASNES_65C816_H_
#define CHIMERASNES_65C816_H_

#define AL   A.B.l
#define AH   A.B.h
#define XL   X.B.l
#define XH   X.B.h
#define YL   Y.B.l
#define YH   Y.B.h
#define SL   S.B.l
#define SH   S.B.h
#define DL   D.B.l
#define DH   D.B.h
#define PL   P.B.l
#define PH   P.B.h
#define PBPC PC.xPBPC
#define PCw  PC.W.xPC
#define PCh  PC.B.xPCh
#define PCl  PC.B.xPCl
#define PB   PC.B.xPB

enum
{
	CARRY       = 1 << 0,
	ZERO        = 1 << 1,
	IRQ         = 1 << 2,
	DECIMAL     = 1 << 3,
	INDEX_FLAG  = 1 << 4,
	MEMORY_FLAG = 1 << 5,
	OVERFLOW    = 1 << 6,
	NEGATIVE    = 1 << 7,
	EMULATION   = 1 << 8
};

#define CheckCarry()     (ICPU.Carry)
#define SetCarry()       (ICPU.Carry = 1)
#define ClearCarry()     (ICPU.Carry = 0)
#define CheckZero()      (ICPU.Zero == 0)
#define SetZero()        (ICPU.Zero =  0)
#define ClearZero()      (ICPU.Zero =  1)
#define CheckIRQ()       (ICPU.Registers.PL &   IRQ)
#define SetIRQ()         (ICPU.Registers.PL |=  IRQ)
#define ClearIRQ()       (ICPU.Registers.PL &= ~IRQ)
#define CheckDecimal()   (ICPU.Registers.PL &   DECIMAL)
#define SetDecimal()     (ICPU.Registers.PL |=  DECIMAL)
#define ClearDecimal()   (ICPU.Registers.PL &= ~DECIMAL)
#define CheckIndex()     (ICPU.Registers.PL &   INDEX_FLAG)
#define SetIndex()       (ICPU.Registers.PL |=  INDEX_FLAG)
#define ClearIndex()     (ICPU.Registers.PL &= ~INDEX_FLAG)
#define CheckMemory()    (ICPU.Registers.PL &   MEMORY_FLAG)
#define SetMemory()      (ICPU.Registers.PL |=  MEMORY_FLAG)
#define ClearMemory()    (ICPU.Registers.PL &= ~MEMORY_FLAG)
#define CheckOverflow()  (ICPU.Overflow)
#define SetOverflow()    (ICPU.Overflow = 1)
#define ClearOverflow()  (ICPU.Overflow = 0)
#define CheckNegative()  (ICPU.Negative & 0x80)
#define SetNegative()    (ICPU.Negative = 0x80)
#define ClearNegative()  (ICPU.Negative = 0)
#define SetFlags(f)      (ICPU.Registers.P.W |=  (f))
#define ClearFlags(f)    (ICPU.Registers.P.W &= ~(f))
#define CheckEmulation() (ICPU.Registers.P.W & EMULATION)

typedef union
{
	struct
	{
	#ifdef MSB_FIRST
		uint8_t h, l;
	#else
		uint8_t l, h;
	#endif
	} B;

	uint16_t W;
} pair;

typedef union
{
	struct
	{
	#ifdef MSB_FIRST
		uint8_t z, xPB, xPCh, xPCl;
	#else
		uint8_t xPCl, xPCh, xPB, z;
	#endif
	} B;

	struct
	{
	#ifdef MSB_FIRST
		uint16_t d, xPC;
	#else
		uint16_t xPC, d;
	#endif
	} W;

	uint32_t xPBPC;
} PC_t;

typedef struct
{
	uint8_t  DB;
	uint8_t  _SRegisters_PAD1;
	pair     A;
	pair     D;
	pair     P;
	pair     S;
	pair     X;
	pair     Y;
	PC_t     PC;
} SRegisters;
#endif
