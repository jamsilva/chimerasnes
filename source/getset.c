#include "ppu.h"
#include "cpuexec.h"
#include "dsp.h"
#include "sa1.h"
#include "spc7110.h"
#include "obc1.h"
#include "seta.h"
#include "xband.h"

uint8_t GetByte(uint32_t Address)
{
	int32_t  block;
	uint8_t* GetAddress = Memory.Map[block = (Address >> MEMMAP_SHIFT) & MEMMAP_MASK];

	if ((intptr_t) GetAddress != MAP_CPU || !CPU.InDMA)
		CPU.Cycles += Memory.MemorySpeed[block];

	if (GetAddress >= (uint8_t*) MAP_LAST)
	{
		if (Memory.BlockIsRAM[block])
			CPU.WaitAddress = CPU.PCAtOpcodeStart;

		return GetAddress[Address & 0xffff];
	}

	switch ((intptr_t) GetAddress)
	{
		case MAP_PPU:
			return GetPPU(Address & 0xffff);
		case MAP_CPU:
			return GetCPU(Address & 0xffff);
		case MAP_DSP:
			return GetDSP(Address & 0xffff);
		case MAP_SA1RAM:
		case MAP_LOROM_SRAM:
			/*Address & 0x7FFF - offset into bank
			 *Address & 0xFF0000 - bank
			 *bank >> 1 | offset = s-ram address, unbound
			 *unbound & SRAMMask = Sram offset */
			return Memory.SRAM[(((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & Memory.SRAMMask];
		case MAP_RONLY_SRAM:
		case MAP_HIROM_SRAM:
			return Memory.SRAM[((Address & 0x7fff) - 0x6000 + ((Address & 0xf0000) >> 3)) & Memory.SRAMMask];
		case MAP_BWRAM:
			return Memory.BWRAM[(Address & 0x7fff) - 0x6000];
		case MAP_CX4:
			return GetCX4(Address & 0xffff);
		case MAP_SPC7110_ROM:
			return GetSPC7110Byte(Address);
		case MAP_SPC7110_DRAM:
			return GetSPC7110(0x4800);
		case MAP_OBC_RAM:
			return GetOBC1(Address & 0xffff);
		case MAP_SETA_DSP:
			return GetSETA(Address);
		case MAP_XBAND:
			return GetXBAND(Address);
		default:
			return ICPU.OpenBus;
	}
}

uint16_t GetWord(uint32_t Address)
{
	int32_t  block;
	uint8_t* GetAddress;

	if ((Address & 0x0fff) == 0x0fff)
	{
		ICPU.OpenBus = GetByte(Address);
		return ICPU.OpenBus | (GetByte(Address + 1) << 8);
	}

	GetAddress = Memory.Map[block = (Address >> MEMMAP_SHIFT) & MEMMAP_MASK];

	if ((intptr_t) GetAddress != MAP_CPU || !CPU.InDMA)
		CPU.Cycles += Memory.MemorySpeed[block] << 1;

	if (GetAddress >= (uint8_t*) MAP_LAST)
	{
		if (Memory.BlockIsRAM[block])
			CPU.WaitAddress = CPU.PCAtOpcodeStart;
	#ifdef MSB_FIRST
		return GetAddress[Address & 0xffff] | (GetAddress[(Address & 0xffff) + 1] << 8);
	#else
		return *(uint16_t*) (GetAddress + (Address & 0xffff));
	#endif
	}

	switch ((intptr_t) GetAddress)
	{
		case MAP_PPU:
			return GetPPU(Address & 0xffff) | (GetPPU((Address + 1) & 0xffff) << 8);
		case MAP_CPU:
			return GetCPU(Address & 0xffff) | (GetCPU((Address + 1) & 0xffff) << 8);
		case MAP_DSP:
			return GetDSP(Address & 0xffff) | (GetDSP((Address + 1) & 0xffff) << 8);
		case MAP_SA1RAM:
		case MAP_LOROM_SRAM:
			/*Address & 0x7FFF - offset into bank
			 * Address & 0xFF0000 - bank
			 * bank >> 1 | offset = s-ram address, unbound
			 * unbound & SRAMMask = Sram offset */
			return Memory.SRAM[(((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & Memory.SRAMMask] | (Memory.SRAM[((((Address + 1) & 0xff0000) >> 1) | ((Address + 1) & 0x7fff)) & Memory.SRAMMask] << 8);
		case MAP_HIROM_SRAM:
		case MAP_RONLY_SRAM:
			return Memory.SRAM[((Address & 0x7fff) - 0x6000 + ((Address & 0xf0000) >> 3)) & Memory.SRAMMask] | (Memory.SRAM[(((Address + 1) & 0x7fff) - 0x6000 + (((Address + 1) & 0xf0000) >> 3)) & Memory.SRAMMask] << 8);
		case MAP_BWRAM:
		#ifdef MSB_FIRST
			return Memory.BWRAM[(Address & 0x7fff) - 0x6000] | (Memory.BWRAM[((Address + 1) & 0x7fff) - 0x6000] << 8);
		#else
			return *(uint16_t*) (Memory.BWRAM + ((Address & 0x7fff) - 0x6000));
		#endif
		case MAP_CX4:
			return GetCX4(Address & 0xffff) | (GetCX4((Address + 1) & 0xffff) << 8);
		case MAP_SPC7110_ROM:
			return GetSPC7110Byte(Address) | (GetSPC7110Byte(Address + 1) << 8);
		case MAP_SPC7110_DRAM:
			return GetSPC7110(0x4800) | (GetSPC7110(0x4800) << 8);
		case MAP_OBC_RAM:
			return GetOBC1(Address & 0xffff) | (GetOBC1((Address + 1) & 0xffff) << 8);
		case MAP_SETA_DSP:
			return GetSETA(Address) | (GetSETA((Address + 1)) << 8);
		case MAP_XBAND:
			return GetXBAND(Address) | (GetXBAND(Address + 1) << 8);
		default:
			return ICPU.OpenBus | (ICPU.OpenBus << 8);
	}
}

void SetByte(uint8_t Byte, uint32_t Address)
{
	int32_t  block;
	uint8_t* SetAddress = Memory.WriteMap[block = ((Address >> MEMMAP_SHIFT) & MEMMAP_MASK)];
	CPU.WaitAddress     = NULL;

	if (SetAddress >= (uint8_t*) MAP_LAST)
	{
		if ((intptr_t) SetAddress != MAP_CPU || !CPU.InDMA)
			CPU.Cycles += Memory.MemorySpeed[block];

		SetAddress += Address & 0xffff;

		if ((Settings.Chip == SA_1) && (SetAddress == SA1.WaitByteAddress1 || SetAddress == SA1.WaitByteAddress2))
		{
			SA1.Executing   = SA1.Opcodes != NULL;
			SA1.WaitCounter = 0;
		}

		*SetAddress = Byte;
		return;
	}

	switch ((intptr_t) SetAddress)
	{
		case MAP_PPU:
			SetPPU(Byte, Address & 0xffff);
			return;
		case MAP_CPU:
			SetCPU(Byte, Address & 0xffff);
			return;
		case MAP_DSP:
			SetDSP(Byte, Address & 0xffff);
			return;
		case MAP_LOROM_SRAM:
			if (Memory.SRAMMask)
			{
				Memory.SRAM[(((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & Memory.SRAMMask] = Byte;
				CPU.SRAMModified = true;
			}

			return;
		case MAP_HIROM_SRAM:
			if (Memory.SRAMMask)
			{
				Memory.SRAM[((Address & 0x7fff) - 0x6000 + ((Address & 0xf0000) >> 3)) & Memory.SRAMMask] = Byte;
				CPU.SRAMModified = true;
			}

			return;
		case MAP_BWRAM:
			Memory.BWRAM[(Address & 0x7fff) - 0x6000] = Byte;
			CPU.SRAMModified = true;
			return;
		case MAP_SA1RAM:
			Memory.SRAM[Address & 0xffff] = Byte;
			SA1.Executing = !SA1.Waiting;
			return;
		case MAP_CX4:
			SetCX4(Byte, Address & 0xffff);
			return;
		case MAP_OBC_RAM:
			SetOBC1(Byte, Address & 0xffff);
			return;
		case MAP_SETA_DSP:
			SetSETA(Byte, Address);
			return;
		case MAP_XBAND:
			SetXBAND(Byte, Address);
			return;
		default:
			return;
	}
}

void SetWord(uint16_t Word, uint32_t Address)
{
	int32_t block;
	uint8_t* SetAddress;

	if ((Address & 0x0fff) == 0x0fff)
	{
		SetByte(Word & 0x00ff, Address);
		SetByte(Word >> 8, Address + 1);
		return;
	}

	CPU.WaitAddress = NULL;
	SetAddress = Memory.WriteMap[block = ((Address >> MEMMAP_SHIFT) & MEMMAP_MASK)];

	if (SetAddress >= (uint8_t*) MAP_LAST)
	{
		if ((intptr_t) SetAddress != MAP_CPU || !CPU.InDMA)
			CPU.Cycles += Memory.MemorySpeed[block] << 1;

		SetAddress += Address & 0xffff;

		if ((Settings.Chip == SA_1) && (SetAddress == SA1.WaitByteAddress1 || SetAddress == SA1.WaitByteAddress2))
		{
			SA1.Executing   = SA1.Opcodes != NULL;
			SA1.WaitCounter = 0;
		}

	#ifdef MSB_FIRST
		SetAddress[0] = (uint8_t) Word;
		SetAddress[1] = Word >> 8;
	#else
		*(uint16_t*) SetAddress = Word;
	#endif

		return;
	}

	switch ((intptr_t) SetAddress)
	{
		case MAP_PPU:
			SetPPU(Word & 0x00ff, Address & 0xffff);
			SetPPU(Word >> 8, (Address + 1) & 0xffff);
			return;
		case MAP_CPU:
			SetCPU(Word & 0x00ff, Address & 0xffff);
			SetCPU(Word >> 8, (Address + 1) & 0xffff);
			return;
		case MAP_DSP:
			SetDSP(Word & 0x00ff, Address & 0xffff);
			SetDSP(Word >> 8, (Address + 1) & 0xffff);
			return;
		case MAP_LOROM_SRAM:
			if (Memory.SRAMMask)
			{
				Memory.SRAM[(((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & Memory.SRAMMask] = (uint8_t) Word;
				Memory.SRAM[((((Address + 1) & 0xff0000) >> 1) | ((Address + 1) & 0x7fff)) & Memory.SRAMMask] = Word >> 8;
				CPU.SRAMModified = true;
			}

			return;
		case MAP_HIROM_SRAM:
			if (Memory.SRAMMask)
			{
				Memory.SRAM[((Address & 0x7fff) - 0x6000 + ((Address & 0xf0000) >> 3)) & Memory.SRAMMask] = (uint8_t) Word;
				Memory.SRAM[(((Address + 1) & 0x7fff) - 0x6000 + (((Address + 1) & 0xf0000) >> 3)) & Memory.SRAMMask] = Word >> 8;
				CPU.SRAMModified = true;
			}

			return;
		case MAP_BWRAM:
		#ifdef MSB_FIRST
			Memory.BWRAM[(Address & 0x7fff) - 0x6000] = (uint8_t) Word;
			Memory.BWRAM[((Address + 1) & 0x7fff) - 0x6000] = (uint8_t) (Word >> 8);
		#else
			*(uint16_t*) (Memory.BWRAM + ((Address & 0x7fff) - 0x6000)) = Word;
		#endif

			CPU.SRAMModified = true;
			return;
		case MAP_SA1RAM:
			Memory.SRAM[Address & 0xffff] = (uint8_t) Word;
			Memory.SRAM[(Address + 1) & 0xffff] = (uint8_t) (Word >> 8);
			SA1.Executing = !SA1.Waiting;
			return;
		case MAP_CX4:
			SetCX4(Word & 0xff, Address & 0xffff);
			SetCX4((uint8_t) (Word >> 8), (Address + 1) & 0xffff);
			return;
		case MAP_OBC_RAM:
			SetOBC1(Word & 0xff, Address & 0xFFFF);
			SetOBC1((uint8_t) (Word >> 8), (Address + 1) & 0xffff);
			return;
		case MAP_SETA_DSP:
			SetSETA(Word & 0x00ff, Address);
			SetSETA(Word >> 8, Address + 1);
			return;
		case MAP_XBAND:
			SetXBAND(Word & 0x00ff, Address);
			SetXBAND((uint8_t) (Word >> 8), Address + 1);
			return;
		default:
			return;
	}
}

uint8_t* GetBasePointer(uint32_t Address)
{
	uint8_t* GetAddress = Memory.Map[(Address >> MEMMAP_SHIFT) & MEMMAP_MASK];

	if (GetAddress >= (uint8_t*) MAP_LAST)
		return GetAddress;

	if (((Settings.Chip & SPC7110) == SPC7110) && ((Address & 0x7FFFFF) == 0x4800))
		return s7r.bank50;

	switch ((intptr_t) GetAddress)
	{
		case MAP_SPC7110_DRAM:
			return s7r.bank50;
		case MAP_SPC7110_ROM:
			return GetBasePointerSPC7110(Address);
		case MAP_PPU: /*just a guess, but it looks like this should match the CPU as a source. */
		case MAP_CPU: /*fixes Ogre Battle's green lines */
		case MAP_OBC_RAM:
			return Memory.FillRAM;
		case MAP_DSP:
			return Memory.FillRAM - 0x6000;
		case MAP_CX4:
			return Memory.CX4RAM - 0x6000;
		case MAP_SA1RAM:
		case MAP_LOROM_SRAM:
		case MAP_SETA_DSP:
			return Memory.SRAM;
		case MAP_BWRAM:
			return Memory.BWRAM - 0x6000;
		case MAP_HIROM_SRAM:
			return Memory.SRAM - 0x6000;
		case MAP_XBAND:
			return GetBasePointerXBAND(Address);
		default:
			return NULL;
	}
}

uint8_t* GetMemPointer(uint32_t Address)
{
	uint8_t* GetAddress = Memory.Map[(Address >> MEMMAP_SHIFT) & MEMMAP_MASK];
	if (GetAddress >= (uint8_t*) MAP_LAST)
		return GetAddress + (Address & 0xffff);

	if (((Settings.Chip & SPC7110) == SPC7110) && ((Address & 0x7FFFFF) == 0x4800))
		return s7r.bank50;

	switch ((intptr_t) GetAddress)
	{
		case MAP_SPC7110_DRAM:
			return &s7r.bank50[Address & 0xffff];
		case MAP_PPU:
			return Memory.FillRAM + (Address & 0xffff);
		case MAP_CPU:
			return Memory.FillRAM + (Address & 0xffff);
		case MAP_DSP:
			return Memory.FillRAM - 0x6000 + (Address & 0xffff);
		case MAP_SA1RAM:
		case MAP_LOROM_SRAM:
			return Memory.SRAM + (Address & 0xffff);
		case MAP_BWRAM:
			return Memory.BWRAM - 0x6000 + (Address & 0xffff);
		case MAP_HIROM_SRAM:
			return Memory.SRAM - 0x6000 + (Address & 0xffff);
		case MAP_CX4:
			return Memory.CX4RAM - 0x6000 + (Address & 0xffff);
		case MAP_OBC_RAM:
			return GetMemPointerOBC1(Address);
		case MAP_SETA_DSP:
			return Memory.SRAM + ((Address & 0xffff) & Memory.SRAMMask);
		case MAP_XBAND:
			return GetMemPointerXBAND(Address);
		default:
			return NULL;
	}
}

void SetPCBase(uint32_t Address)
{
	int32_t  block      = (Address >> MEMMAP_SHIFT) & MEMMAP_MASK;
	uint8_t* GetAddress = Memory.Map[block];
	CPU.MemSpeed        = Memory.MemorySpeed[block];
	CPU.MemSpeedx2      = CPU.MemSpeed << 1;

	if (GetAddress >= (uint8_t*) MAP_LAST)
	{
		CPU.PCBase = GetAddress;
		CPU.PC = CPU.PCBase + (Address & 0xffff);
		return;
	}

	switch ((intptr_t) GetAddress)
	{
		case MAP_PPU:
		case MAP_CPU:
			CPU.PCBase = Memory.FillRAM;
			break;
		case MAP_DSP:
			CPU.PCBase = Memory.FillRAM - 0x6000;
			break;
		case MAP_BWRAM:
			CPU.PCBase = Memory.BWRAM - 0x6000;
			break;
		case MAP_HIROM_SRAM:
			CPU.PCBase = Memory.SRAM - 0x6000;
			break;
		case MAP_CX4:
			CPU.PCBase = Memory.CX4RAM - 0x6000;
			break;
		case MAP_XBAND:
			CPU.PCBase = GetBasePointerXBAND(Address);
			break;
		default:
			CPU.PCBase = Memory.SRAM;
			break;
	}

	CPU.PC = CPU.PCBase + (Address & 0xffff);
}
