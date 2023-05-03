#include "chisnes.h"
#include "memmap.h"
#include "ppu.h"
#include "cpuexec.h"
#include "sa1.h"

#define CPU                          SA1
#define ICPU                         SA1
#define GetByte                      SA1GetByte
#define GetWord                      SA1GetWord
#define SetByte                      SA1SetByte
#define SetWord                      SA1SetWord
#define SetPCBase                    SA1SetPCBase
#define OpcodesM1X1                  SA1OpcodesM1X1
#define OpcodesM1X0                  SA1OpcodesM1X0
#define OpcodesM0X1                  SA1OpcodesM0X1
#define OpcodesM0X0                  SA1OpcodesM0X0
#define OpcodesE1                    SA1OpcodesE1
#define OpcodeIRQ                    SA1OpcodeIRQ
#define OpcodeNMI                    SA1OpcodeNMI
#define UnpackStatus                 SA1UnpackStatus
#define PackStatus                   SA1PackStatus
#define FixCycles                    SA1FixCycles
#define Immediate8                   SA1Immediate8
#define Immediate16                  SA1Immediate16
#define Relative                     SA1Relative
#define RelativeLong                 SA1RelativeLong
#define Absolute                     SA1Absolute
#define AbsoluteLong                 SA1AbsoluteLong
#define AbsoluteIndirect             SA1AbsoluteIndirect
#define AbsoluteIndirectLong         SA1AbsoluteIndirectLong
#define AbsoluteIndexedIndirect      SA1AbsoluteIndexedIndirect
#define Direct                       SA1Direct
#define DirectIndirectIndexed        SA1DirectIndirectIndexed
#define DirectIndirectIndexedLong    SA1DirectIndirectIndexedLong
#define DirectIndexedIndirect        SA1DirectIndexedIndirect
#define DirectIndexedX               SA1DirectIndexedX
#define DirectIndexedY               SA1DirectIndexedY
#define AbsoluteIndexedX             SA1AbsoluteIndexedX
#define AbsoluteIndexedY             SA1AbsoluteIndexedY
#define AbsoluteLongIndexedX         SA1AbsoluteLongIndexedX
#define DirectIndirect               SA1DirectIndirect
#define DirectIndirectLong           SA1DirectIndirectLong
#define StackRelative                SA1StackRelative
#define StackRelativeIndirectIndexed SA1StackRelativeIndirectIndexed

#define SA1_OPCODES

#include "cpuops.c"

void SA1MainLoop()
{
	uint8_t Work8;
	int32_t i;

	if (SA1.Flags & IRQ_PENDING_FLAG)
	{
		if (SA1.IRQActive)
		{
			if (SA1.WaitingForInterrupt)
			{
				SA1.WaitingForInterrupt = false;
				SA1.Registers.PCw++;
			}

			if (!SA1CheckIRQ())
				SA1OpcodeIRQ();
		}
		else
			SA1.Flags &= ~IRQ_PENDING_FLAG;
	}

	for (i = 0; i < 3 && SA1.Executing; i++)
	{
		SA1.PCAtOpcodeStart = SA1.Registers.PCw;
		READ_PC_BYTE(Work8);
		(*SA1.Opcodes[Work8].Opcode)();
	}
}
