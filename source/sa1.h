#ifndef CHIMERASNES_SA1_H_
#define CHIMERASNES_SA1_H_

#include "cpuexec.h"

typedef struct
{
	uint8_t  DB;
	uint8_t  PB;
	uint16_t PC;
	pair     A;
	pair     D;
	pair     P;
	pair     S;
	pair     X;
	pair     Y;
} SSA1Registers;

typedef struct
{
	uint8_t       in_char_dma             : 3;
	uint8_t       variable_bit_pos        : 4;
	bool          Carry                   : 1;
	bool          CumulativeOverflow      : 1;
	bool          Executing               : 1;
	bool          Overflow                : 1;
	bool          Waiting                 : 1;
	bool          WaitingForInterrupt     : 1;
	bool          Zero                    : 1;
	uint8_t       arithmetic_op           : 2;
	bool          UseVirtualBitmapFormat2 : 1;
	bool          NMIActive               : 1;
	int8_t        _SSA1_PAD1              : 6;
	uint8_t       Negative;
	uint8_t       IRQActive;
	uint8_t       OpenBus;
	int16_t       op1;
	int16_t       op2;
	int16_t       _SSA1_PAD2              : 16;
	int32_t       _SSA1_PAD3              : 32;
	uint32_t      Flags;
	uint32_t      ShiftedPB;
	uint32_t      ShiftedDB;
	uint32_t      WaitCounter;
	int64_t       sum;
	uint8_t*      BWRAM;
	uint8_t*      PC;
	uint8_t*      PCAtOpcodeStart;
	uint8_t*      PCBase;
	uint8_t*      WaitAddress;
	uint8_t*      WaitByteAddress1;
	uint8_t*      WaitByteAddress2;
	SOpcodes*     Opcodes;
	uint8_t*      Map[MEMMAP_NUM_BLOCKS];
	uint8_t*      WriteMap[MEMMAP_NUM_BLOCKS];
	SSA1Registers Registers;
} SSA1;

#define SA1CheckIRQ()       (SA1.Registers.PL  & IRQ)
#define SA1CheckIndex()     (SA1.Registers.PL  & INDEX_FLAG)
#define SA1CheckMemory()    (SA1.Registers.PL  & MEMORY_FLAG)
#define SA1CheckEmulation() (SA1.Registers.P.W & EMULATION)
#define SA1SetFlags(f)      (SA1.Registers.P.W |=  (f))
#define SA1ClearFlags(f)    (SA1.Registers.P.W &= ~(f))

extern SSA1     SA1;
extern SOpcodes SA1OpcodesE1[256];
extern SOpcodes SA1OpcodesM1X1[256];
extern SOpcodes SA1OpcodesM1X0[256];
extern SOpcodes SA1OpcodesM0X1[256];
extern SOpcodes SA1OpcodesM0X0[256];

uint8_t  SA1GetByte(uint32_t address);
void     SA1SetByte(uint8_t byte, uint32_t address);
uint16_t SA1GetWord(uint32_t address);
void     SA1SetWord(uint16_t Word, uint32_t address);
void     SA1SetPCBase(uint32_t address);
uint8_t  GetSA1(uint32_t address);
void     SetSA1(uint8_t byte, uint32_t address);
void     SA1Init();
void     SA1MainLoop();

#define DMA_IRQ_SOURCE   (1 << 5)
#define TIMER_IRQ_SOURCE (1 << 6)
#define SNES_IRQ_SOURCE  (1 << 7)

static INLINE void SA1UnpackStatus()
{
	SA1.Zero     = !(SA1.Registers.PL & ZERO);
	SA1.Negative =  (SA1.Registers.PL & NEGATIVE);
	SA1.Carry    =  (SA1.Registers.PL & CARRY);
	SA1.Overflow =  (SA1.Registers.PL & OVERFLOW) >> 6;
}

static INLINE void SA1PackStatus()
{
	SA1.Registers.PL &= ~(ZERO | NEGATIVE | CARRY | OVERFLOW);
	SA1.Registers.PL |= SA1.Carry | ((!SA1.Zero) << 1) | (SA1.Negative & 0x80) | (SA1.Overflow << 6);
}

static INLINE void SA1FixCycles()
{
	if (SA1CheckEmulation())
	{
		SA1.Opcodes = SA1OpcodesE1;
	}
	else if (SA1CheckMemory())
	{
		if (SA1CheckIndex())
			SA1.Opcodes = SA1OpcodesM1X1;
		else
			SA1.Opcodes = SA1OpcodesM1X0;
	}
	else if (SA1CheckIndex())
		SA1.Opcodes = SA1OpcodesM0X1;
	else
		SA1.Opcodes = SA1OpcodesM0X0;
}
#endif
