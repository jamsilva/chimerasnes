#include "chisnes.h"
#include "apu.h"
#include "cheats.h"
#include "cpuexec.h"
#include "dma.h"
#include "dsp.h"
#include "fxemu.h"
#include "memmap.h"
#include "obc1.h"
#include "ppu.h"
#include "sa1.h"
#include "sdd1.h"
#include "spc7110.h"
#include "srtc.h"
#include "bsx.h"

void ResetCPU()
{
	ICPU.Registers.PBPC = GetWord(0xfffc, WRAP_NONE);
	ICPU.Registers.D.W = 0;
	ICPU.Registers.DB = 0;
	ICPU.Registers.SH = 1;
	ICPU.Registers.SL = 0xff;
	ICPU.Registers.XH = 0;
	ICPU.Registers.YH = 0;
	ICPU.Registers.P.W = 0;
	ICPU.ShiftedPB = 0;
	ICPU.ShiftedDB = 0;
	SetFlags(MEMORY_FLAG | INDEX_FLAG | IRQ | EMULATION);
	ClearFlags(DECIMAL);
	CPU.BranchSkip = false;
	CPU.NMIActive = false;
	CPU.IRQActive = false;
	CPU.WaitingForInterrupt = false;
	CPU.InDMA = false;
	CPU.PCBase = NULL;
	CPU.PCAtOpcodeStart = 0;
	CPU.WaitPC = 0;
	CPU.WaitCounter = 1;
	CPU.V_Counter = 0;
	CPU.Cycles = 182; /* This is the cycle count just after the jump to the Reset Vector. */
	CPU.WhichEvent = HBLANK_START_EVENT;
	CPU.NextEvent = Settings.HBlankStart;
	CPU.MemSpeed = Settings.SlowOneCycle;
	CPU.MemSpeedx2 = Settings.SlowOneCycle * 2;
	CPU.FastROMSpeed = Settings.SlowOneCycle;
	CPU.SRAMModified = false;
	SetPCBase(ICPU.Registers.PCw);
	ICPU.Opcodes = OpcodesE1;
	ICPU.OpLengths = OpLengthsM1X1;
	CPU.NMICycleCount = 0;
	CPU.IRQCycleCount = 0;
	UnpackStatus();
}

static void CommonReset()
{
	memset(Memory.VRAM, 0x00, 0x10000);

	if ((Settings.Chip & BS) == BS)
		ResetBSX();
	else if ((Settings.Chip & DSP) == DSP)
		ResetDSP();
	else if (Settings.Chip == GSU)
		FxReset(&SuperFX);
	else if (Settings.Chip == SA_1)
		SA1Init();
	else if (Settings.Chip == S_DD1)
		ResetSDD1();
	else if ((Settings.Chip & SPC7110) == SPC7110)
		ResetSPC7110();
	else if (Settings.Chip == CX_4)
		InitCX4();
	else if (Settings.Chip == OBC_1)
		ResetOBC1();
	else if (Settings.Chip == S_RTC)
		ResetSRTC();

	ResetCPU();
	ResetDMA();
	ResetAPU();
}

void Reset()
{
	memset(Memory.RAM, 0x55, 0x20000);
	memset(Memory.FillRAM, 0, 0x8000);
	ResetPPU();
	CommonReset();
}

void SoftReset()
{
	SoftResetPPU();
	CommonReset();
}
