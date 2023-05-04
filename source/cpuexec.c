#include "chisnes.h"
#include "memmap.h"
#include "cpuops.h"
#include "ppu.h"
#include "cpuexec.h"
#include "gfx.h"
#include "apu.h"
#include "dma.h"
#include "fxemu.h"
#include "sa1.h"
#include "spc7110.h"
#include "cpumacro.h"
#include "cpuaddr.h"

#define MAIN_LOOP(HBLANK_PROCESSING, SA1_MAIN_LOOP)                  \
{                                                                    \
	uint8_t Work8;                                                   \
	                                                                 \
	do                                                               \
	{                                                                \
		do                                                           \
		{                                                            \
			if (CPU.Flags)                                           \
			{                                                        \
				if (CPU.Flags & NMI_FLAG)                            \
				{                                                    \
					if (--CPU.NMICycleCount == 0)                    \
					{                                                \
						CPU.Flags &= ~NMI_FLAG;                      \
						                                             \
						if (CPU.WaitingForInterrupt)                 \
						{                                            \
							CPU.WaitingForInterrupt = false;         \
							ICPU.Registers.PCw++;                    \
						}                                            \
						                                             \
						OpcodeNMI();                                 \
					}                                                \
				}                                                    \
				                                                     \
				if (CPU.Flags & IRQ_PENDING_FLAG)                    \
				{                                                    \
					if (CPU.IRQCycleCount == 0)                      \
					{                                                \
						if (CPU.WaitingForInterrupt)                 \
						{                                            \
							CPU.WaitingForInterrupt = false;         \
							ICPU.Registers.PCw++;                    \
						}                                            \
						                                             \
						if (!CPU.IRQActive)                          \
							CPU.Flags &= ~IRQ_PENDING_FLAG;          \
						else if (!CheckIRQ())                        \
							OpcodeIRQ();                             \
					}                                                \
					else if (--CPU.IRQCycleCount == 0 && CheckIRQ()) \
						CPU.IRQCycleCount = 1;                       \
				}                                                    \
				                                                     \
				if (CPU.Flags & SCAN_KEYS_FLAG)                      \
					break;                                           \
			}                                                        \
			                                                         \
			CPU.PCAtOpcodeStart = ICPU.Registers.PCw;                \
			CPU.Cycles += CPU.MemSpeed;                              \
			READ_PC_BYTE(Work8);                                     \
			(*ICPU.Opcodes[Work8].Opcode)();                         \
			SA1_MAIN_LOOP;                                           \
			                                                         \
			if (CPU.Cycles >= CPU.NextEvent)                         \
				HBLANK_PROCESSING;                                   \
			                                                         \
			APU_EXECUTE();                                           \
			                                                         \
			if (finishedFrame)                                       \
				break;                                               \
		} while (true);                                              \
		                                                             \
		IAPU.Registers.PC = IAPU.PC - IAPU.RAM;                      \
		                                                             \
		if (!finishedFrame)                                          \
		{                                                            \
			PackStatus();                                            \
			APUPackStatus();                                         \
			CPU.Flags &= ~SCAN_KEYS_FLAG;                            \
		}                                                            \
		else                                                         \
		{                                                            \
			finishedFrame = false;                                   \
			break;                                                   \
		}                                                            \
	} while (!finishedFrame);                                        \
}

/* This is a CatSFC modification inspired by a Snes9x-Euphoria modification.
 * The emulator selects a main loop based on the chips used in an entire frame.
 * This avoids the constant SA1.Executing and Settings.Chip == GSU checks. */
static void MainLoop_SA1()
{
	MAIN_LOOP(DoHBlankProcessing_NoSFX(),
		if (SA1.Executing)
			SA1MainLoop()
	)
}

static void MainLoop_SuperFX()
{
	MAIN_LOOP(DoHBlankProcessing_SFX(), )
}

static void MainLoop_Fast()
{
	MAIN_LOOP(DoHBlankProcessing_NoSFX(), )
}

void SetMainLoop()
{
	if (Settings.Chip == SA_1)
		MainLoop = &MainLoop_SA1;
	else if (Settings.Chip == GSU)
		MainLoop = &MainLoop_SuperFX;
	else
		MainLoop = &MainLoop_Fast;
}

void SetIRQSource(uint32_t source)
{
	CPU.IRQActive |= source;
	CPU.Flags |= IRQ_PENDING_FLAG;
	CPU.IRQCycleCount = 3;

	if (CPU.WaitingForInterrupt) /* Force IRQ to trigger immediately after WAI - Final Fantasy Mystic Quest crashes without this. */
	{
		CPU.IRQCycleCount = 0;
		CPU.WaitingForInterrupt = false;
		ICPU.Registers.PCw++;
	}
}

void ClearIRQSource(uint32_t source)
{
	CPU.IRQActive &= ~source;

	if (!CPU.IRQActive)
		CPU.Flags &= ~IRQ_PENDING_FLAG;
}

/* This is a CatSFC modification inspired by a Snes9x-Euphoria modification.
 * The emulator selects an HBlank processor based on the chips used in an
 * entire frame. This avoids the constant Settings.Chip == GSU checks. */
#define DO_HBLANK_PROCESSING(SUPERFX_EXEC)                                                                                            \
{                                                                                                                                     \
	int32_t i;                                                                                                                        \
	CPU.WaitCounter++;                                                                                                                \
	                                                                                                                                  \
	switch (CPU.WhichEvent)                                                                                                           \
	{                                                                                                                                 \
		case HBLANK_START_EVENT:                                                                                                      \
			if (IPPU.HDMA && CPU.V_Counter <= PPU.ScreenHeight)                                                                       \
				IPPU.HDMA = DoHDMA(IPPU.HDMA);                                                                                        \
			                                                                                                                          \
			break;                                                                                                                    \
		case HBLANK_END_EVENT:                                                                                                        \
			SUPERFX_EXEC;                                                                                                             \
			CPU.Cycles -= Settings.H_Max;                                                                                             \
			                                                                                                                          \
			if (IAPU.Executing)                                                                                                       \
				APU.Cycles -= Settings.H_Max;                                                                                         \
			else                                                                                                                      \
				APU.Cycles = 0;                                                                                                       \
			                                                                                                                          \
			CPU.NextEvent = -1;                                                                                                       \
			                                                                                                                          \
			if (++CPU.V_Counter >= (Settings.PAL ? SNES_MAX_PAL_VCOUNTER : SNES_MAX_NTSC_VCOUNTER))                                   \
			{                                                                                                                         \
				CPU.V_Counter = 0;                                                                                                    \
				Memory.FillRAM[0x213F] ^= 0x80;                                                                                       \
				PPU.RangeTimeOver = 0;                                                                                                \
				CPU.NMIActive = false;                                                                                                \
				CPU.Flags |= SCAN_KEYS_FLAG;                                                                                          \
				StartHDMA();                                                                                                          \
			}                                                                                                                         \
			                                                                                                                          \
			if (PPU.VTimerEnabled && !PPU.HTimerEnabled && CPU.V_Counter == PPU.IRQVBeamPos)                                          \
				SetIRQSource(PPU_V_BEAM_IRQ_SOURCE);                                                                                  \
			                                                                                                                          \
			if (CPU.V_Counter == PPU.ScreenHeight + FIRST_VISIBLE_LINE) /* Start of V-blank */                                        \
			{                                                                                                                         \
				EndScreenRefresh();                                                                                                   \
				IPPU.HDMA = 0;                                                                                                        \
				PPU.ForcedBlanking = (Memory.FillRAM[0x2100] >> 7) & 1; /* Bits 7 and 6 of $4212 are computed when read in GetPPU. */ \
				                                                                                                                      \
				if (!PPU.ForcedBlanking)                                                                                              \
				{                                                                                                                     \
					uint8_t tmp = 0;                                                                                                  \
					PPU.OAMAddr = PPU.SavedOAMAddr;                                                                                   \
					                                                                                                                  \
					if (PPU.OAMPriorityRotation)                                                                                      \
						tmp = (PPU.OAMAddr & 0xFE) >> 1;                                                                              \
					                                                                                                                  \
					if ((PPU.OAMFlip & 1) || PPU.FirstSprite != tmp)                                                                  \
					{                                                                                                                 \
						PPU.FirstSprite = tmp;                                                                                        \
						IPPU.OBJChanged = true;                                                                                       \
					}                                                                                                                 \
					                                                                                                                  \
					PPU.OAMFlip = 0;                                                                                                  \
				}                                                                                                                     \
				                                                                                                                      \
				Memory.FillRAM[0x4210] = 0x80 | Model->_5A22;                                                                         \
				                                                                                                                      \
				if (Memory.FillRAM[0x4200] & 0x80)                                                                                    \
				{                                                                                                                     \
					CPU.Flags |= NMI_FLAG;                                                                                            \
					CPU.NMICycleCount = CPU.NMITriggerPoint;                                                                          \
				}                                                                                                                     \
			}                                                                                                                         \
			                                                                                                                          \
			if (CPU.V_Counter == PPU.ScreenHeight + 3)                                                                                \
				UpdateJoypads();                                                                                                      \
			                                                                                                                          \
			if (CPU.V_Counter == FIRST_VISIBLE_LINE)                                                                                  \
			{                                                                                                                         \
				Memory.FillRAM[0x4210] = Model->_5A22;                                                                                \
				CPU.Flags &= ~NMI_FLAG;                                                                                               \
				StartScreenRefresh();                                                                                                 \
			}                                                                                                                         \
			                                                                                                                          \
			if (CPU.V_Counter >= FIRST_VISIBLE_LINE && CPU.V_Counter < PPU.ScreenHeight + FIRST_VISIBLE_LINE)                         \
				RenderLine(CPU.V_Counter - FIRST_VISIBLE_LINE);                                                                       \
			                                                                                                                          \
			if (APU.TimerEnabled[2])                                                                                                  \
			{                                                                                                                         \
				APU.Timer[2] += 4;                                                                                                    \
				                                                                                                                      \
				while (APU.Timer[2] >= APU.TimerTarget[2])                                                                            \
				{                                                                                                                     \
					IAPU.RAM[0xff] = (IAPU.RAM[0xff] + 1) & 0xf;                                                                      \
					APU.Timer[2] -= APU.TimerTarget[2];                                                                               \
					IAPU.WaitCounter++;                                                                                               \
					IAPU.Executing = Settings.APUEnabled;                                                                             \
				}                                                                                                                     \
			}                                                                                                                         \
			                                                                                                                          \
			if (!(CPU.V_Counter & 1))                                                                                                 \
				break;                                                                                                                \
			                                                                                                                          \
			for (i = 0; i < 2; ++i)                                                                                                   \
			{                                                                                                                         \
				if (APU.TimerEnabled[i])                                                                                              \
				{                                                                                                                     \
					APU.Timer[i]++;                                                                                                   \
					                                                                                                                  \
					if (APU.Timer[i] >= APU.TimerTarget[i])                                                                           \
					{                                                                                                                 \
						IAPU.RAM[0xfd + i] = (IAPU.RAM[0xfd + i] + 1) & 0xf;                                                          \
						APU.Timer[i] = 0;                                                                                             \
						IAPU.WaitCounter++;                                                                                           \
						IAPU.Executing = Settings.APUEnabled;                                                                         \
					}                                                                                                                 \
				}                                                                                                                     \
			}                                                                                                                         \
			                                                                                                                          \
			break;                                                                                                                    \
		case HTIMER_BEFORE_EVENT:                                                                                                     \
		case HTIMER_AFTER_EVENT:                                                                                                      \
			if (PPU.HTimerEnabled && (!PPU.VTimerEnabled || CPU.V_Counter == PPU.IRQVBeamPos))                                        \
				SetIRQSource(PPU_H_BEAM_IRQ_SOURCE);                                                                                  \
			                                                                                                                          \
			break;                                                                                                                    \
	}                                                                                                                                 \
	                                                                                                                                  \
	Reschedule();                                                                                                                     \
}

void DoHBlankProcessing_SFX()
	DO_HBLANK_PROCESSING(SuperFXExec())

void DoHBlankProcessing_NoSFX()
	DO_HBLANK_PROCESSING()
