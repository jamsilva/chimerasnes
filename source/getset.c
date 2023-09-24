#include "apu.h"
#include "ppu.h"
#include "cpuexec.h"
#include "cx4.h"
#include "dsp.h"
#include "sa1.h"
#include "spc7110.h"
#include "obc1.h"
#include "seta.h"
#include "bsx.h"
#include "xband.h"

static INLINE int32_t memory_speed(uint32_t address)
{
	int32_t block = (address & 0xffffff) >> MEMMAP_SHIFT;
	return Memory.MemorySpeed[block];
}

static INLINE void AddNumCyclesInMemAccess(int32_t cycles)
{
	if (CPU.InDMA || (Settings.MetroidHack && IPPU.HDMA))
		return;

	CPU.Cycles += cycles;
	HBlankProcessingLoop();
}

static INLINE void AddCyclesHack(int32_t cycles)
{
	if (Settings.BSXHack && CPU.InDMA && CPU.WhichEvent == HBLANK_START_EVENT)
		CPU.Cycles += cycles; /* This is a hack but allows some BSX games to work */
	else
		AddNumCyclesInMemAccess(cycles);
}

static INLINE void AddCyclesInMemAccess(uint32_t address)
{
	AddNumCyclesInMemAccess(memory_speed(address));
}

static INLINE void AddCyclesX2InMemAccess(uint32_t address)
{
	AddNumCyclesInMemAccess(memory_speed(address) << 1);
}

uint8_t GetByte(uint32_t Address)
{
	uint8_t  byte;
	int32_t  block = (Address & 0xffffff) >> MEMMAP_SHIFT;
	uint8_t* GetAddress = Memory.Map[block];

	if (GetAddress >= (uint8_t *) MAP_LAST)
	{
		if (Memory.BlockIsRAM[block])
			CPU.WaitPC = CPU.PCAtOpcodeStart;

		byte = GetAddress[Address & 0xffff];
		AddCyclesInMemAccess(Address);
		return byte;
	}

	switch ((intptr_t) GetAddress)
	{
		case MAP_CPU:
			byte = GetCPU(Address & 0xffff);
			AddCyclesInMemAccess(Address);
			return byte;
		case MAP_PPU:
			if (CPU.InDMA && (Address & 0xff00) == 0x2100)
				return ICPU.OpenBus;

			byte = GetPPU(Address & 0xffff);
			AddCyclesInMemAccess(Address);
			return byte;
		case MAP_LOROM_SRAM:
		case MAP_SA1RAM:
			/* Address & 0x7FFF  : offset into bank
			 * Address & 0xFF0000: bank
			 * bank >> 1 | offset: SRAM address, unbound
			 * unbound & SRAMMask: SRAM offset */
			byte = Memory.SRAM[(((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & Memory.SRAMMask];
			AddCyclesInMemAccess(Address);
			return byte;
		case MAP_HIROM_SRAM:
		case MAP_RONLY_SRAM:
			byte = Memory.SRAM[((Address & 0x7fff) - 0x6000 + ((Address & 0x1f0000) >> 3)) & Memory.SRAMMask];
			AddCyclesInMemAccess(Address);
			return byte;
		case MAP_BWRAM:
			byte = Memory.BWRAM[(Address & 0x7fff) - 0x6000];
			AddCyclesInMemAccess(Address);
			return byte;
		case MAP_DSP:
			byte = GetDSP(Address & 0xffff);
			AddCyclesInMemAccess(Address);
			return byte;
		case MAP_SPC7110_ROM:
			byte = GetSPC7110Byte(Address);
			AddCyclesInMemAccess(Address);
			return byte;
		case MAP_SPC7110_DRAM:
			byte = GetSPC7110(0x4800);
			AddCyclesInMemAccess(Address);
			return byte;
		case MAP_CX4:
			byte = GetCX4(Address & 0xffff);
			AddCyclesInMemAccess(Address);
			return byte;
		case MAP_OBC_RAM:
			byte = GetOBC1(Address & 0xffff);
			AddCyclesInMemAccess(Address);
			return byte;
		case MAP_SETA_DSP:
			byte = GetSETA(Address);
			AddCyclesInMemAccess(Address);
			return byte;
		case MAP_BSX:
			byte = GetBSX(Address);
			AddCyclesInMemAccess(Address);
			return byte;
		case MAP_XBAND:
			byte = GetXBAND(Address);
			AddCyclesInMemAccess(Address);
			return byte;
		case MAP_NONE:
		default:
			byte = ICPU.OpenBus;
			AddCyclesInMemAccess(Address);
			return byte;
	}
}

uint16_t GetWord(uint32_t Address, wrap_t w)
{
	uint16_t word;
	int32_t  block;
	uint8_t* GetAddress;
	uint32_t mask = MEMMAP_MASK & (w == WRAP_PAGE ? 0xff : (w == WRAP_BANK ? 0xffff : 0xffffff));

	if ((Address & mask) == mask)
	{
		PC_t a;
		ICPU.OpenBus = word = GetByte(Address);

		switch (w)
		{
			case WRAP_PAGE:
				a.xPBPC = Address;
				a.B.xPCl++;
				return word | (GetByte(a.xPBPC) << 8);
			case WRAP_BANK:
				a.xPBPC = Address;
				a.W.xPC++;
				return word | (GetByte(a.xPBPC) << 8);
			case WRAP_NONE:
			default:
				return word | (GetByte(Address + 1) << 8);
		}
	}

	block = (Address & 0xffffff) >> MEMMAP_SHIFT;
	GetAddress = Memory.Map[block];

	if (GetAddress >= (uint8_t *) MAP_LAST)
	{
		if (Memory.BlockIsRAM[block])
			CPU.WaitPC = CPU.PCAtOpcodeStart;

		word = READ_WORD(GetAddress + (Address & 0xffff));
		AddCyclesX2InMemAccess(Address);
		return word;
	}

	switch ((intptr_t) GetAddress)
	{
		case MAP_CPU:
			word  = GetCPU(Address & 0xffff);
			AddCyclesInMemAccess(Address);
			word |= GetCPU((Address + 1) & 0xffff) << 8;
			AddCyclesInMemAccess(Address);
			return word;
		case MAP_PPU:
			if (CPU.InDMA)
			{
				word = ICPU.OpenBus = GetByte(Address);
				return (word | (GetByte(Address + 1) << 8));
			}

			word  = GetPPU(Address & 0xffff);
			AddCyclesInMemAccess(Address);
			word |= GetPPU((Address + 1) & 0xffff) << 8;
			AddCyclesInMemAccess(Address);
			return word;
		case MAP_LOROM_SRAM:
		case MAP_SA1RAM:
			if (Memory.SRAMMask >= MEMMAP_MASK)
				word = READ_WORD(Memory.SRAM + ((((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & Memory.SRAMMask));
			else
				word = Memory.SRAM[(((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & Memory.SRAMMask] | (Memory.SRAM[((((Address + 1) & 0xff0000) >> 1) | ((Address + 1) & 0x7fff)) & Memory.SRAMMask] << 8);
			AddCyclesX2InMemAccess(Address);
			return word;
		case MAP_HIROM_SRAM:
		case MAP_RONLY_SRAM:
			if (Memory.SRAMMask >= MEMMAP_MASK)
				word = READ_WORD(Memory.SRAM + (((Address & 0x7fff) - 0x6000 + ((Address & 0x1f0000) >> 3)) & Memory.SRAMMask));
			else
				word = Memory.SRAM[((Address & 0x7fff) - 0x6000 + ((Address & 0x1f0000) >> 3)) & Memory.SRAMMask] | (Memory.SRAM[(((Address + 1) & 0x7fff) - 0x6000 + (((Address + 1) & 0x1f0000) >> 3)) & Memory.SRAMMask] << 8);
			AddCyclesX2InMemAccess(Address);
			return word;
		case MAP_BWRAM:
			word = READ_WORD(Memory.BWRAM + ((Address & 0x7fff) - 0x6000));
			AddCyclesX2InMemAccess(Address);
			return word;
		case MAP_DSP:
			word  = GetDSP(Address & 0xffff);
			AddCyclesInMemAccess(Address);
			word |= GetDSP((Address + 1) & 0xffff) << 8;
			AddCyclesInMemAccess(Address);
			return word;
		case MAP_SPC7110_ROM:
			word  = GetSPC7110Byte(Address);
			AddCyclesInMemAccess(Address);
			word |= GetSPC7110Byte(Address + 1) << 8;
			AddCyclesInMemAccess(Address);
			return word;
		case MAP_SPC7110_DRAM:
			word  = GetSPC7110(0x4800);
			AddCyclesInMemAccess(Address);
			word |= GetSPC7110(0x4800) << 8;
			AddCyclesInMemAccess(Address);
			return word;
		case MAP_CX4:
			word  = GetCX4(Address & 0xffff);
			AddCyclesInMemAccess(Address);
			word |= GetCX4((Address + 1) & 0xffff) << 8;
			AddCyclesInMemAccess(Address);
			return word;
		case MAP_OBC_RAM:
			word  = GetOBC1(Address & 0xffff);
			AddCyclesInMemAccess(Address);
			word |= GetOBC1((Address + 1) & 0xffff) << 8;
			AddCyclesInMemAccess(Address);
			return word;
		case MAP_SETA_DSP:
			word  = GetSETA(Address);
			AddCyclesInMemAccess(Address);
			word |= GetSETA(Address + 1) << 8;
			AddCyclesInMemAccess(Address);
			return word;
		case MAP_BSX:
			word  = GetBSX(Address);
			AddCyclesInMemAccess(Address);
			word |= GetBSX(Address + 1) << 8;
			AddCyclesInMemAccess(Address);
			return word;
		case MAP_XBAND:
			word  = GetXBAND(Address);
			AddCyclesInMemAccess(Address);
			word |= GetXBAND(Address + 1) << 8;
			AddCyclesInMemAccess(Address);
			return word;
		case MAP_NONE:
		default:
			word = ICPU.OpenBus | (ICPU.OpenBus << 8);
			AddCyclesX2InMemAccess(Address);
			return word;
	}
}

void SetByte(uint8_t Byte, uint32_t Address)
{
	int32_t  block = (Address & 0xffffff) >> MEMMAP_SHIFT;
	uint8_t* SetAddress = Memory.WriteMap[block];
	CPU.WaitPC = 0;

	if (SetAddress >= (uint8_t *) MAP_LAST)
	{
		SetAddress += Address & 0xffff;

		if ((Settings.Chip == SA_1) && (SetAddress == SA1.WaitByteAddress1 || SetAddress == SA1.WaitByteAddress2))
		{
			SA1.Executing = (SA1.Opcodes != NULL);
			SA1.WaitCounter = 0;
		}

		*SetAddress = Byte;
		AddCyclesHack(memory_speed(Address));
		return;
	}

	switch ((intptr_t) SetAddress)
	{
		case MAP_CPU:
			SetCPU(Byte, Address & 0xffff);
			AddCyclesInMemAccess(Address);
			return;
		case MAP_PPU:
			if (CPU.InDMA && (Address & 0xff00) == 0x2100)
				return;

			SetPPU(Byte, Address & 0xffff);
			AddCyclesInMemAccess(Address);
			return;
		case MAP_LOROM_SRAM:
			if (Memory.SRAMMask)
			{
				Memory.SRAM[(((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & Memory.SRAMMask] = Byte;
				CPU.SRAMModified = true;
			}

			AddCyclesInMemAccess(Address);
			return;
		case MAP_HIROM_SRAM:
			if (Memory.SRAMMask)
			{
				Memory.SRAM[((Address & 0x7fff) - 0x6000 + ((Address & 0x1f0000) >> 3)) & Memory.SRAMMask] = Byte;
				CPU.SRAMModified = true;
			}

			AddCyclesInMemAccess(Address);
			return;
		case MAP_BWRAM:
			Memory.BWRAM[(Address & 0x7fff) - 0x6000] = Byte;
			CPU.SRAMModified = true;
			AddCyclesInMemAccess(Address);
			return;
		case MAP_SA1RAM:
			Memory.SRAM[Address & 0xffff] = Byte;
			SA1.Executing = !SA1.Waiting;
			AddCyclesInMemAccess(Address);
			return;
		case MAP_DSP:
			SetDSP(Byte, Address & 0xffff);
			AddCyclesInMemAccess(Address);
			return;
		case MAP_CX4:
			SetCX4(Byte, Address & 0xffff);
			AddCyclesInMemAccess(Address);
			return;
		case MAP_OBC_RAM:
			SetOBC1(Byte, Address & 0xffff);
			AddCyclesInMemAccess(Address);
			return;
		case MAP_SETA_DSP:
			SetSETA(Byte, Address);
			AddCyclesInMemAccess(Address);
			return;
		case MAP_BSX:
			SetBSX(Byte, Address);
			AddCyclesInMemAccess(Address);
			return;
		case MAP_XBAND:
			SetXBAND(Byte, Address);
			AddCyclesInMemAccess(Address);
			return;
		case MAP_NONE:
		default:
			AddCyclesInMemAccess(Address);
			return;
	}
}

void SetWord(uint16_t Word, uint32_t Address, wrap_t w, writeorder_t o)
{
	int32_t  block;
	uint8_t* SetAddress;
	uint32_t mask = MEMMAP_MASK & (w == WRAP_PAGE ? 0xff : (w == WRAP_BANK ? 0xffff : 0xffffff));

	if ((Address & mask) == mask)
	{
		PC_t a;

		if (!o)
			SetByte((uint8_t) Word, Address);

		switch (w)
		{
			case WRAP_PAGE:
				a.xPBPC = Address;
				a.B.xPCl++;
				SetByte(Word >> 8, a.xPBPC);
				break;
			case WRAP_BANK:
				a.xPBPC = Address;
				a.W.xPC++;
				SetByte(Word >> 8, a.xPBPC);
				break;
			case WRAP_NONE:
			default:
				SetByte(Word >> 8, Address + 1);
				break;
		}

		if (o)
			SetByte((uint8_t) Word, Address);

		return;
	}

	CPU.WaitPC = 0;
	block = (Address & 0xffffff) >> MEMMAP_SHIFT;
	SetAddress = Memory.WriteMap[block];

	if (SetAddress >= (uint8_t *) MAP_LAST)
	{
		SetAddress += Address & 0xffff;

		if ((Settings.Chip == SA_1) && (SetAddress == SA1.WaitByteAddress1 || SetAddress == SA1.WaitByteAddress2))
		{
			SA1.Executing = (SA1.Opcodes != NULL);
			SA1.WaitCounter = 0;
		}

		WRITE_WORD(SetAddress, Word);
		AddCyclesHack(memory_speed(Address) << 1);
		return;
	}

	switch ((intptr_t) SetAddress)
	{
		case MAP_CPU:
			if (o)
			{
				SetCPU(Word >> 8, (Address + 1) & 0xffff);
				AddCyclesInMemAccess(Address);
				SetCPU((uint8_t) Word, Address & 0xffff);
			}
			else
			{
				SetCPU((uint8_t) Word, Address & 0xffff);
				AddCyclesInMemAccess(Address);
				SetCPU(Word >> 8, (Address + 1) & 0xffff);
			}

			AddCyclesInMemAccess(Address);
			return;
		case MAP_PPU:
			if (CPU.InDMA)
			{
				if ((Address & 0xff00) != 0x2100)
					SetPPU((uint8_t) Word, Address & 0xffff);

				if (((Address + 1) & 0xff00) != 0x2100)
					SetPPU(Word >> 8, (Address + 1) & 0xffff);

				return;
			}

			if (o)
			{
				SetPPU(Word >> 8, (Address + 1) & 0xffff);
				AddCyclesInMemAccess(Address);
				SetPPU((uint8_t) Word, Address & 0xffff);
			}
			else
			{
				SetPPU((uint8_t) Word, Address & 0xffff);
				AddCyclesInMemAccess(Address);
				SetPPU(Word >> 8, (Address + 1) & 0xffff);
			}

			AddCyclesInMemAccess(Address);
			return;
		case MAP_LOROM_SRAM:
			if (Memory.SRAMMask)
			{
				if (Memory.SRAMMask >= MEMMAP_MASK)
				{
					WRITE_WORD(Memory.SRAM + ((((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & Memory.SRAMMask), Word);
				}
				else
				{
					Memory.SRAM[(((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & Memory.SRAMMask] = (uint8_t) Word;
					Memory.SRAM[((((Address + 1) & 0xff0000) >> 1) | ((Address + 1) & 0x7fff)) & Memory.SRAMMask] = Word >> 8;
				}

				CPU.SRAMModified = true;
			}

			AddCyclesX2InMemAccess(Address);
			return;
		case MAP_HIROM_SRAM:
			if (Memory.SRAMMask)
			{
				if (Memory.SRAMMask >= MEMMAP_MASK)
				{
					WRITE_WORD(Memory.SRAM + (((Address & 0x7fff) - 0x6000 + ((Address & 0x1f0000) >> 3)) & Memory.SRAMMask), Word);
				}
				else
				{
					Memory.SRAM[((Address & 0x7fff) - 0x6000 + ((Address & 0x1f0000) >> 3)) & Memory.SRAMMask] = (uint8_t) Word;
					Memory.SRAM[(((Address + 1) & 0x7fff) - 0x6000 + (((Address + 1) & 0x1f0000) >> 3)) & Memory.SRAMMask] = Word >> 8;
				}

				CPU.SRAMModified = true;
			}

			AddCyclesX2InMemAccess(Address);
			return;
		case MAP_BWRAM:
			WRITE_WORD(Memory.BWRAM + (Address & 0x7fff) - 0x6000, Word);
			CPU.SRAMModified = true;
			AddCyclesX2InMemAccess(Address);
			return;
		case MAP_SA1RAM:
			WRITE_WORD(Memory.SRAM + (Address & 0xffff), Word);
			SA1.Executing = !SA1.Waiting;
			AddCyclesX2InMemAccess(Address);
			return;
		case MAP_DSP:
			if (o)
			{
				SetDSP(Word >> 8, (Address + 1) & 0xffff);
				AddCyclesInMemAccess(Address);
				SetDSP((uint8_t) Word, Address & 0xffff);
			}
			else
			{
				SetDSP((uint8_t) Word, Address & 0xffff);
				AddCyclesInMemAccess(Address);
				SetDSP(Word >> 8, (Address + 1) & 0xffff);
			}

			AddCyclesInMemAccess(Address);
			return;
		case MAP_CX4:
			if (o)
			{
				SetCX4(Word >> 8, (Address + 1) & 0xffff);
				AddCyclesInMemAccess(Address);
				SetCX4((uint8_t) Word, Address & 0xffff);
			}
			else
			{
				SetCX4((uint8_t) Word, Address & 0xffff);
				AddCyclesInMemAccess(Address);
				SetCX4(Word >> 8, (Address + 1) & 0xffff);
			}

			AddCyclesInMemAccess(Address);
			return;
		case MAP_OBC_RAM:
			if (o)
			{
				SetOBC1(Word >> 8, (Address + 1) & 0xffff);
				AddCyclesInMemAccess(Address);
				SetOBC1((uint8_t) Word, Address & 0xffff);
			}
			else
			{
				SetOBC1((uint8_t) Word, Address & 0xffff);
				AddCyclesInMemAccess(Address);
				SetOBC1(Word >> 8, (Address + 1) & 0xffff);
			}

			AddCyclesInMemAccess(Address);
			return;
		case MAP_SETA_DSP:
			if (o)
			{
				SetSETA(Word >> 8, Address + 1);
				AddCyclesInMemAccess(Address);
				SetSETA((uint8_t) Word, Address);
			}
			else
			{
				SetSETA((uint8_t) Word, Address);
				AddCyclesInMemAccess(Address);
				SetSETA(Word >> 8, Address + 1);
			}

			AddCyclesInMemAccess(Address);
			return;
		case MAP_BSX:
			if (o)
			{
				SetBSX(Word >> 8, Address + 1);
				AddCyclesInMemAccess(Address);
				SetBSX((uint8_t) Word, Address);
			}
			else
			{
				SetBSX((uint8_t) Word, Address);
				AddCyclesInMemAccess(Address);
				SetBSX(Word >> 8, Address + 1);
			}

			AddCyclesInMemAccess(Address);
			return;
		case MAP_XBAND:
			if (o)
			{
				SetXBAND(Word >> 8, Address + 1);
				AddCyclesInMemAccess(Address);
				SetXBAND((uint8_t) Word, Address);
			}
			else
			{
				SetXBAND((uint8_t) Word, Address);
				AddCyclesInMemAccess(Address);
				SetXBAND(Word >> 8, Address + 1);
			}

			AddCyclesInMemAccess(Address);
			return;
		case MAP_NONE:
		default:
			AddCyclesX2InMemAccess(Address);
			return;
	}
}

uint8_t* GetBasePointer(uint32_t Address)
{
	uint8_t* GetAddress = Memory.Map[(Address & 0xffffff) >> MEMMAP_SHIFT];

	if (GetAddress >= (uint8_t*) MAP_LAST)
		return GetAddress;

	if (((Settings.Chip & SPC7110) == SPC7110) && ((Address & 0x7FFFFF) == 0x4800))
		return s7r.bank50;

	switch ((intptr_t) GetAddress)
	{
		case MAP_CPU: /* just a guess, but it looks like this should match the CPU as a source. */
		case MAP_PPU: /* fixes Ogre Battle's green lines */
			return Memory.FillRAM;
		case MAP_LOROM_SRAM:
			if((Memory.SRAMMask & MEMMAP_MASK) != MEMMAP_MASK)
				return NULL;

			return Memory.SRAM;
		case MAP_HIROM_SRAM:
			if ((Memory.SRAMMask & MEMMAP_MASK) != MEMMAP_MASK)
				return NULL;

			return Memory.SRAM - 0x6000;
		case MAP_BWRAM:
			return Memory.BWRAM - 0x6000;
		case MAP_SA1RAM:
			return Memory.SRAM;
		case MAP_DSP:
			return Memory.FillRAM - 0x6000;
		case MAP_SPC7110_DRAM:
			return s7r.bank50;
		case MAP_SPC7110_ROM:
			return GetBasePointerSPC7110(Address);
		case MAP_CX4:
			return GetBasePointerCX4(Address & 0xffff);
		case MAP_OBC_RAM:
			return GetBasePointerOBC1(Address & 0xffff);
		case MAP_SETA_DSP:
			return Memory.SRAM;
		case MAP_BSX:
			return GetBasePointerBSX(Address);
		case MAP_XBAND:
			return GetBasePointerXBAND(Address);
		case MAP_NONE:
		default:
			return NULL;
	}
}

uint8_t* GetMemPointer(uint32_t Address)
{
	uint8_t* GetAddress = Memory.Map[(Address & 0xffffff) >> MEMMAP_SHIFT];

	if (GetAddress >= (uint8_t*) MAP_LAST)
		return GetAddress + (Address & 0xffff);

	if (((Settings.Chip & SPC7110) == SPC7110) && ((Address & 0x7FFFFF) == 0x4800))
		return s7r.bank50;

	switch ((intptr_t) GetAddress)
	{
		case MAP_CPU:
			return Memory.FillRAM + (Address & 0x7fff);
		case MAP_PPU:
			return Memory.FillRAM + (Address & 0x7fff);
		case MAP_LOROM_SRAM:
			if((Memory.SRAMMask & MEMMAP_MASK) != MEMMAP_MASK)
				return NULL;

			return Memory.SRAM + (Address & 0x7fff);
		case MAP_HIROM_SRAM:
			if((Memory.SRAMMask & MEMMAP_MASK) != MEMMAP_MASK)
				return NULL;

			return Memory.SRAM + (Address & 0x7fff) - 0x6000;
		case MAP_BWRAM:
			return Memory.BWRAM + (Address & 0x7fff) - 0x6000;
		case MAP_SA1RAM:
			return Memory.SRAM + (Address & 0xffff);
		case MAP_DSP:
			return Memory.FillRAM + (Address & 0x7fff) - 0x6000;
		case MAP_SPC7110_DRAM:
			return &s7r.bank50[Address & 0x7fff];
		case MAP_SPC7110_ROM:
			return GetBasePointerSPC7110(Address) + (Address & 0xffff);
		case MAP_CX4:
			return GetMemPointerCX4(Address & 0xffff);
		case MAP_OBC_RAM:
			return GetMemPointerOBC1(Address & 0xffff);
		case MAP_SETA_DSP:
			return Memory.SRAM + ((Address & 0x7fff) & Memory.SRAMMask);
		case MAP_XBAND:
			return GetMemPointerXBAND(Address);
		case MAP_NONE:
		default:
			return NULL;
	}
}

void SetPCBase(uint32_t Address)
{
	ICPU.Registers.PBPC = Address & 0xffffff;
	ICPU.ShiftedPB = Address & 0xff0000;
	CPU.MemSpeed = memory_speed(Address);
	CPU.MemSpeedx2 = CPU.MemSpeed << 1;
	CPU.PCBase = GetBasePointer(Address);
}
