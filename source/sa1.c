#include "chisnes.h"
#include "cpuexec.h"
#include "ppu.h"
#include "sa1.h"

static void SA1CharConv2();
static void SA1DMA();
static void SA1ReadVariableLengthData(bool inc, bool no_shift);

void SA1Init()
{
	SA1.NMIActive           = false;
	SA1.IRQActive           = 0;
	SA1.WaitingForInterrupt = false;
	SA1.Waiting             = false;
	SA1.Flags               = 0;
	SA1.Executing           = false;
	SA1.WaitCounter         = 0;
	memset(&Memory.FillRAM[0x2200], 0, 0x200);
	Memory.FillRAM[0x2200]  = 0x20;
	Memory.FillRAM[0x2220]  = 0x00;
	Memory.FillRAM[0x2221]  = 0x01;
	Memory.FillRAM[0x2222]  = 0x02;
	Memory.FillRAM[0x2223]  = 0x03;
	Memory.FillRAM[0x2228]  = 0x0f;
	SA1.op1                 = 0;
	SA1.op2                 = 0;
	SA1.arithmetic_op       = 0;
	SA1.sum                 = 0;
	SA1.CumulativeOverflow  = false;
	SA1.Opcodes             = NULL;
	SA1.in_char_dma         = 0;
}

static void SA1Reset()
{
	SA1.Registers.PBPC  = Memory.FillRAM[0x2203] | (Memory.FillRAM[0x2204] << 8);
	SA1.Registers.D.W   = 0;
	SA1.Registers.DB    = 0;
	SA1.Registers.SH    = 1;
	SA1.Registers.SL    = 0xFF;
	SA1.Registers.XH    = 0;
	SA1.Registers.YH    = 0;
	SA1.Registers.P.W   = 0;
	SA1.ShiftedPB       = 0;
	SA1.ShiftedDB       = 0;
	SA1SetFlags(MEMORY_FLAG | INDEX_FLAG | IRQ | EMULATION);
	SA1ClearFlags(DECIMAL);
	SA1.WaitingForInterrupt = false;
	SA1.PCBase = NULL;
	SA1SetPCBase(SA1.Registers.PBPC);
	SA1.Opcodes = SA1OpcodesM1X1;
	SA1.OpLengths = OpLengthsM1X1;
	SA1UnpackStatus();
	SA1FixCycles();
	SA1.Executing          = true;
	SA1.WaitCounter        = 0;
	SA1.BWRAM              = Memory.SRAM;
	Memory.FillRAM[0x2225] = 0;
}

static void SA1SetBWRAMMemMap(uint8_t val)
{
	int32_t c;

	if (val & 0x80)
	{
		for (c = 0; c < 0x400; c += 16)
		{
			SA1.Map[c + 6] = SA1.Map[c + 0x806] = (uint8_t*) MAP_BWRAM_BITMAP2;
			SA1.Map[c + 7] = SA1.Map[c + 0x807] = (uint8_t*) MAP_BWRAM_BITMAP2;
			SA1.WriteMap[c + 6] = SA1.WriteMap[c + 0x806] = (uint8_t*) MAP_BWRAM_BITMAP2;
			SA1.WriteMap[c + 7] = SA1.WriteMap[c + 0x807] = (uint8_t*) MAP_BWRAM_BITMAP2;
		}

		SA1.BWRAM = Memory.SRAM + (val & 0x7f) * 0x2000 / 4;
		return;
	}

	for (c = 0; c < 0x400; c += 16)
	{
		SA1.Map[c + 6] = SA1.Map[c + 0x806] = (uint8_t*) MAP_BWRAM;
		SA1.Map[c + 7] = SA1.Map[c + 0x807] = (uint8_t*) MAP_BWRAM;
		SA1.WriteMap[c + 6] = SA1.WriteMap[c + 0x806] = (uint8_t*) MAP_BWRAM;
		SA1.WriteMap[c + 7] = SA1.WriteMap[c + 0x807] = (uint8_t*) MAP_BWRAM;
	}

	SA1.BWRAM = Memory.SRAM + (val & 0x1f) * 0x2000;
}

uint8_t SA1GetByte(uint32_t address)
{
	uint8_t* GetAddress = SA1.Map[(address & 0xffffff) >> MEMMAP_SHIFT];

	if (GetAddress >= (uint8_t*) MAP_LAST)
		return GetAddress[address & 0xffff];

	switch ((intptr_t) GetAddress)
	{
		case MAP_PPU:
			return GetSA1(address & 0xffff);
		case MAP_LOROM_SRAM:
		case MAP_HIROM_SRAM:
		case MAP_SA1RAM:
			return Memory.SRAM[address & 0x3ffff];
		case MAP_BWRAM:
			return SA1.BWRAM[address & 0x1fff];
		case MAP_BWRAM_BITMAP:
			address -= 0x600000;

			if (SA1.UseVirtualBitmapFormat2)
				return (Memory.SRAM[(address >> 2) & 0x3ffff] >> ((address & 3) << 1)) & 3;
			else
				return (Memory.SRAM[(address >> 1) & 0x3ffff] >> ((address & 1) << 2)) & 15;
		case MAP_BWRAM_BITMAP2:
			address = (address & 0xffff) - 0x6000;

			if (SA1.UseVirtualBitmapFormat2)
				return (SA1.BWRAM[(address >> 2) & 0x3ffff] >> ((address & 3) << 1)) & 3;
			else
				return (SA1.BWRAM[(address >> 1) & 0x3ffff] >> ((address & 1) << 2)) & 15;
		default:
			return SA1.OpenBus;
	}
}

uint16_t SA1GetWord(uint32_t address, wrap_t w)
{
	PC_t a;
	SA1.OpenBus = SA1GetByte(address);

	switch (w)
	{
		case WRAP_PAGE:
			a.xPBPC = address;
			a.B.xPCl++;
			return SA1.OpenBus | (SA1GetByte(a.xPBPC) << 8);
		case WRAP_BANK:
			a.xPBPC = address;
			a.W.xPC++;
			return SA1.OpenBus | (SA1GetByte(a.xPBPC) << 8);
		case WRAP_NONE:
		default:
			return SA1.OpenBus | (SA1GetByte(address + 1) << 8);
	}
}

void SA1SetByte(uint8_t byte, uint32_t address)
{
	uint8_t* SetAddress = SA1.WriteMap[(address & 0xffffff) >> MEMMAP_SHIFT];
	CPU.WaitPC = 0;
	CPU.WaitCounter = 1;

	if (SetAddress >= (uint8_t*) MAP_LAST)
	{
		SetAddress[address & 0xffff] = byte;
		return;
	}

	switch ((intptr_t) SetAddress)
	{
		case MAP_PPU:
			SetSA1(byte, address & 0xffff);
			return;
		case MAP_LOROM_SRAM:
		case MAP_HIROM_SRAM:
		case MAP_SA1RAM:
			Memory.SRAM[address & 0x3ffff] = byte;
			return;
		case MAP_BWRAM:
			SA1.BWRAM[address & 0x1fff] = byte;
			return;
		case MAP_BWRAM_BITMAP:
			address -= 0x600000;

			if (SA1.UseVirtualBitmapFormat2)
			{
				uint8_t* ptr = &Memory.SRAM[(address >> 2) & 0x3ffff];
				*ptr &= ~(3 << ((address & 3) << 1));
				*ptr |= (byte & 3) << ((address & 3) << 1);
			}
			else
			{
				uint8_t* ptr = &Memory.SRAM[(address >> 1) & 0x3ffff];
				*ptr &= ~(15 << ((address & 1) << 2));
				*ptr |= (byte & 15) << ((address & 1) << 2);
			}

			return;
		case MAP_BWRAM_BITMAP2:
			address = (address & 0xffff) - 0x6000;

			if (SA1.UseVirtualBitmapFormat2)
			{
				uint8_t* ptr = &SA1.BWRAM[(address >> 2) & 0x3ffff];
				*ptr &= ~(3 << ((address & 3) << 1));
				*ptr |= (byte & 3) << ((address & 3) << 1);
			}
			else
			{
				uint8_t* ptr = &SA1.BWRAM[(address >> 1) & 0x3ffff];
				*ptr &= ~(15 << ((address & 1) << 2));
				*ptr |= (byte & 15) << ((address & 1) << 2);
			}

			return;
		default:
			return;
	}
}

void SA1SetWord(uint16_t Word, uint32_t address, wrap_t w, writeorder_t o)
{
	PC_t a;

	if (!o)
		SA1SetByte((uint8_t) Word, address);

	switch (w)
	{
		case WRAP_PAGE:
			a.xPBPC = address;
			a.B.xPCl++;
			SA1SetByte(Word >> 8, a.xPBPC);
			break;
		case WRAP_BANK:
			a.xPBPC = address;
			a.W.xPC++;
			SA1SetByte(Word >> 8, a.xPBPC);
			break;
		case WRAP_NONE:
			SA1SetByte(Word >> 8, address + 1);
			break;
	}

	if (o)
		SA1SetByte((uint8_t) Word, address);
}

void SA1SetPCBase(uint32_t address)
{
	uint8_t* GetAddress = SA1.Map[(address & 0xffffff) >> MEMMAP_SHIFT];
	SA1.Registers.PBPC = address & 0xffffff;
	SA1.ShiftedPB = address & 0xff0000;

	if (GetAddress >= (uint8_t*) MAP_LAST)
	{
		SA1.PCBase = GetAddress;
		return;
	}

	switch ((intptr_t) GetAddress)
	{
		case MAP_LOROM_SRAM:
			if ((Memory.SRAMMask & MEMMAP_MASK) != MEMMAP_MASK)
				SA1.PCBase = NULL;
			else
				SA1.PCBase = &Memory.SRAM[((((address & 0xff0000) >> 1) | (address & 0x7fff)) & Memory.SRAMMask) - (address & 0xffff)];

			return;
		case MAP_HIROM_SRAM:
			if ((Memory.SRAMMask & MEMMAP_MASK) != MEMMAP_MASK)
				SA1.PCBase = NULL;
			else
				SA1.PCBase = &Memory.SRAM[(((address & 0x7fff) - 0x6000 + ((address & 0xf0000) >> 3)) & Memory.SRAMMask) - (address & 0xffff)];

			return;
		case MAP_BWRAM:
			SA1.PCBase = &SA1.BWRAM[-0x6000 - (address & 0x8000)];
			return;
		case MAP_SA1RAM:
			SA1.PCBase = Memory.SRAM;
			return;
		default:
			SA1.PCBase = NULL;
			return;
	}
}

static void SetSA1MemMap(uint32_t which1, uint8_t map)
{
	int32_t c, i;
	int32_t start  = which1 * 0x100 + 0xc00;
	int32_t start2 = which1 * 0x200;

	if (which1 >= 2)
		start2 += 0x400;

	for (c = 0; c < 0x100; c += 16)
	{
		uint8_t* block = &Memory.ROM[(map & 7) * 0x100000 + (c << 12)];

		for (i = c; i < c + 16; i++)
			Memory.Map[start  + i] = SA1.Map[start  + i] = block;
	}

	if (map & 0x80)
		which1 = map;

	which1 &= 7;

	for (c = 0; c < 0x200; c += 16)
	{
		uint8_t* block = Memory.ROM + which1 * 0x100000 + (c << 11) - 0x8000;

		for (i = c + 8; i < c + 16; i++)
			Memory.Map[start2 + i] = SA1.Map[start2 + i] = block;
	}
}

uint8_t GetSA1(uint32_t address)
{
	switch (address)
	{
		case 0x2300:
			return (Memory.FillRAM[0x2209] & 0x5f) | (Memory.FillRAM[0x2300] & 0xa0);
		case 0x2301:
			return (Memory.FillRAM[0x2200] & 0x0f) | (Memory.FillRAM[0x2301] & 0xf0);
		case 0x2306:
			return (uint8_t)  SA1.sum;
		case 0x2307:
			return (uint8_t) (SA1.sum >> 8);
		case 0x2308:
			return (uint8_t) (SA1.sum >> 16);
		case 0x2309:
			return (uint8_t) (SA1.sum >> 24);
		case 0x230a:
			return (uint8_t) (SA1.sum >> 32);
		case 0x230b:
			return SA1.CumulativeOverflow ? 0x80 : 0;
		case 0x230c:
			return Memory.FillRAM[0x230c];
		case 0x230d:
		{
			uint8_t byte = Memory.FillRAM[0x230d];

			if (Memory.FillRAM[0x2258] & 0x80)
				SA1ReadVariableLengthData(true, false);

			return byte;
		}
		case 0x230e: /* version code register */
			return 0x01;
		default:
			break;
	}

	return Memory.FillRAM[address];
}

void SetSA1(uint8_t byte, uint32_t address)
{
	if (address < 0x2200 || address > 0x22ff)
		return;

	switch (address)
	{
		case 0x2200:
			SA1.Waiting = (bool) (byte & 0x60);

			if (!(byte & 0x20) && (Memory.FillRAM[0x2200] & 0x20))
				SA1Reset();

			if (byte & 0x80)
			{
				Memory.FillRAM[0x2301] |= 0x80;

				if (Memory.FillRAM[0x220a] & 0x80)
				{
					Memory.FillRAM[0x220b] &= ~0x80;
					SA1.Flags |= IRQ_FLAG;
					SA1.IRQActive |= SNES_IRQ_SOURCE;
					SA1.Executing = !SA1.Waiting && SA1.Opcodes;
				}
			}

			if (byte & 0x10)
			{
				Memory.FillRAM[0x2301] |= 0x10;

				if (Memory.FillRAM[0x220a] & 0x10)
					Memory.FillRAM[0x220b] &= ~0x10;
			}

			break;
		case 0x2201:
			if (((byte ^ Memory.FillRAM[0x2201]) & 0x80) && (Memory.FillRAM[0x2300] & byte & 0x80))
			{
				Memory.FillRAM[0x2202] &= ~0x80;
				SetIRQSource(SA1_IRQ_SOURCE);
			}

			if (((byte ^ Memory.FillRAM[0x2201]) & 0x20) && (Memory.FillRAM[0x2300] & byte & 0x20))
			{
				Memory.FillRAM[0x2202] &= ~0x20;
				SetIRQSource(SA1_DMA_IRQ_SOURCE);
			}

			break;
		case 0x2202:
			if (byte & 0x80)
			{
				Memory.FillRAM[0x2300] &= ~0x80;
				ClearIRQSource(SA1_IRQ_SOURCE);
			}

			if (byte & 0x20)
			{
				Memory.FillRAM[0x2300] &= ~0x20;
				ClearIRQSource(SA1_DMA_IRQ_SOURCE);
			}

			break;
		case 0x2209:
			if (byte & 0x80)
			{
				Memory.FillRAM[0x2300] |= 0x80;

				if (Memory.FillRAM[0x2201] &   0x80)
				{
					Memory.FillRAM[0x2202] &= ~0x80;
					SetIRQSource(SA1_IRQ_SOURCE);
				}
			}

			break;
		case 0x220a:
			if (((byte ^ Memory.FillRAM[0x220a]) & 0x80) && (Memory.FillRAM[0x2301] & byte & 0x80))
			{
				Memory.FillRAM[0x220b] &= ~0x80;
				SA1.Flags |= IRQ_FLAG;
				SA1.IRQActive |= SNES_IRQ_SOURCE;
			}

			if (((byte ^ Memory.FillRAM[0x220a]) & 0x40) && (Memory.FillRAM[0x2301] & byte & 0x40))
			{
				Memory.FillRAM[0x220b] &= ~0x40;
				SA1.Flags |= IRQ_FLAG;
				SA1.IRQActive |= TIMER_IRQ_SOURCE;
			}

			if (((byte ^ Memory.FillRAM[0x220a]) & 0x20) && (Memory.FillRAM[0x2301] & byte & 0x20))
			{
				Memory.FillRAM[0x220b] &= ~0x20;
				SA1.Flags |= IRQ_FLAG;
				SA1.IRQActive |= DMA_IRQ_SOURCE;
			}

			if (((byte ^ Memory.FillRAM[0x220a]) & 0x10) && (Memory.FillRAM[0x2301] & byte & 0x10))
				Memory.FillRAM[0x220b] &= ~0x10;

			break;
		case 0x220b:
			if (byte & 0x80)
			{
				SA1.IRQActive &= ~SNES_IRQ_SOURCE;
				Memory.FillRAM[0x2301] &= ~0x80;
			}

			if (byte & 0x40)
			{
				SA1.IRQActive &= ~TIMER_IRQ_SOURCE;
				Memory.FillRAM[0x2301] &= ~0x40;
			}

			if (byte & 0x20)
			{
				SA1.IRQActive &= ~DMA_IRQ_SOURCE;
				Memory.FillRAM[0x2301] &= ~0x20;
			}

			if (byte & 0x10)
				Memory.FillRAM[0x2301] &= ~0x10;

			if (!SA1.IRQActive)
				SA1.Flags &= ~IRQ_FLAG;

			break;
		case 0x2220:
		case 0x2221:
		case 0x2222:
		case 0x2223:
			SetSA1MemMap(address - 0x2220, byte);
			break;
		case 0x2224:
			Memory.BWRAM = &Memory.SRAM[(byte & 0x1f) * 0x2000];
			break;
		case 0x2225:
			if (byte == Memory.FillRAM[address])
				break;

			SA1SetBWRAMMemMap(byte);
			break;
		case 0x2231:
			if (byte & 0x80)
				SA1.in_char_dma = 0;

			break;
		case 0x2236: /* DMA destination start address (LH) */
			Memory.FillRAM[address] = byte;

			if ((Memory.FillRAM[0x2230] & 0xa4) == 0x80) /* Normal DMA to I-RAM */
			{
				SA1DMA();
				break;
			}

			if ((Memory.FillRAM[0x2230] & 0xb0) != 0xb0)
				break;

			SA1.in_char_dma = 1;
			Memory.FillRAM[0x2300] |= 0x20;

			if (Memory.FillRAM[0x2201] & 0x20)
			{
				Memory.FillRAM[0x2202] &= ~0x20;
				SetIRQSource(SA1_DMA_IRQ_SOURCE);
			}

			break;
		case 0x2237:
			Memory.FillRAM[address] = byte;

			if ((Memory.FillRAM[0x2230] & 0xa4) != 0x84) /* Normal DMA to BW-RAM */
				break;

			SA1DMA();
			break;
		case 0x223f:
			SA1.UseVirtualBitmapFormat2 = (bool) (byte & 0x80);
			break;
		case 0x224f:
			Memory.FillRAM[address] = byte;

			if ((Memory.FillRAM[0x2230] & 0xb0) != 0xa0) /* Char conversion 2 DMA enabled */
				break;

			/* memmove converted: Same malloc but constant non-overlapping addresses [Neb] */
			memcpy(&Memory.ROM[MAX_ROM_SIZE - 0x10000] + (SA1.in_char_dma << 4), &Memory.FillRAM[0x2240], 16);
			SA1.in_char_dma = (SA1.in_char_dma + 1) & 7;

			if ((SA1.in_char_dma & 3) != 0)
				break;

			SA1CharConv2();
			break;
		case 0x2250:
			if (byte & 2)
				SA1.sum = 0;

			SA1.arithmetic_op = byte & 3;
			break;
		case 0x2251:
			SA1.op1 = (SA1.op1 & 0xff00) | byte;
			break;
		case 0x2252:
			SA1.op1 = (SA1.op1 & 0x00ff) | (byte << 8);
			break;
		case 0x2253:
			SA1.op2 = (SA1.op2 & 0xff00) | byte;
			break;
		case 0x2254:
			SA1.op2 = (SA1.op2 & 0x00ff) | (byte << 8);

			switch (SA1.arithmetic_op)
			{
				case 0: /* multiply */
					SA1.sum = (int16_t) SA1.op1 * (int16_t) SA1.op2;
					SA1.op2 = 0;
					break;
				case 1: /* divide */
					if (SA1.op2 != 0)
					{
						int16_t quotient = (int16_t) SA1.op1 / (uint16_t) SA1.op2;
						uint16_t remainder = (int16_t) SA1.op1 % (uint16_t) SA1.op2;
						SA1.sum = (remainder << 16) | quotient;
					}
					else
						SA1.sum = 0;

					SA1.op1 = 0;
					SA1.op2 = 0;
					break;
				default: /* cumulative sum */
					SA1.sum += (int16_t) SA1.op1 * (int16_t) SA1.op2;
					SA1.CumulativeOverflow = (SA1.sum >= (((int64_t) 1) << 40));
					SA1.sum &= (((uint64_t) 1) << 40) - 1;
					SA1.op2 = 0;
					break;
			}

			break;
		case 0x2258: /* Variable bit-field length/auto inc/start. */
			Memory.FillRAM[0x2258] = byte;
			SA1ReadVariableLengthData(true, false);
			return;
		case 0x2259:
		case 0x225a:
		case 0x225b: /* Variable bit-field start address */
			Memory.FillRAM[address] = byte;
			SA1.variable_bit_pos    = 0;
			SA1ReadVariableLengthData(false, true);
			return;
		default:
			break;
	}

	Memory.FillRAM[address] = byte;
}

static void SA1CharConv2()
{
	uint32_t dest    = Memory.FillRAM[0x2235] | (Memory.FillRAM[0x2236] << 8);
	uint32_t offset  = (SA1.in_char_dma & 7) ? 0 : 1;
	int32_t  depthX8 = (Memory.FillRAM[0x2231] & 3) == 0 ? 64 : (Memory.FillRAM[0x2231] & 3) == 1 ? 32 : 16;
	uint8_t* p       = &Memory.FillRAM[0x3000] + (dest & 0x7ff) + offset * depthX8;
	uint8_t* q       = &Memory.ROM[MAX_ROM_SIZE - 0x10000] + offset * 64;
	int      l, b;

	switch (depthX8)
	{
		case 16:
			for (l = 0; l < 8; l++, q += 8, p += 2)
			{
				for (b = 0; b < 8; b++)
				{
					uint8_t r = q[b];
					p[0] = (p[0] << 1) | ((r >> 0) & 1);
					p[1] = (p[1] << 1) | ((r >> 1) & 1);
				}
			}

			break;
		case 32:
			for (l = 0; l < 8; l++, q += 8, p += 2)
			{
				for (b = 0; b < 8; b++)
				{
					uint8_t r = q[b];
					p[0] = (p[0] << 1) | ((r >> 0) & 1);
					p[1] = (p[1] << 1) | ((r >> 1) & 1);
					p[16] = (p[16] << 1) | ((r >> 2) & 1);
					p[17] = (p[17] << 1) | ((r >> 3) & 1);
				}
			}

			break;
		case 64:
			for (l = 0; l < 8; l++, q += 8, p += 2)
			{
				for (b = 0; b < 8; b++)
				{
					uint8_t r = q[b];
					p[0] = (p[0] << 1) | ((r >> 0) & 1);
					p[1] = (p[1] << 1) | ((r >> 1) & 1);
					p[16] = (p[16] << 1) | ((r >> 2) & 1);
					p[17] = (p[17] << 1) | ((r >> 3) & 1);
					p[32] = (p[32] << 1) | ((r >> 4) & 1);
					p[33] = (p[33] << 1) | ((r >> 5) & 1);
					p[48] = (p[48] << 1) | ((r >> 6) & 1);
					p[49] = (p[49] << 1) | ((r >> 7) & 1);
				}
			}

			break;
	}
}

static void SA1DMA()
{
	uint32_t src = Memory.FillRAM[0x2232] | (Memory.FillRAM[0x2233] << 8) | (Memory.FillRAM[0x2234] << 16);
	uint32_t dst = Memory.FillRAM[0x2235] | (Memory.FillRAM[0x2236] << 8) | (Memory.FillRAM[0x2237] << 16);
	uint32_t len = Memory.FillRAM[0x2238] | (Memory.FillRAM[0x2239] << 8);
	uint8_t* s;
	uint8_t* d;

	switch (Memory.FillRAM[0x2230] & 3)
	{
		case 0: /* ROM */
			s = SA1.Map[(src & 0xffffff) >> MEMMAP_SHIFT];

			if (s >= (uint8_t*) MAP_LAST)
				s += (src & 0xffff);
			else
				s = &Memory.ROM[src & 0xffff];

			break;
		case 1: /* BW-RAM */
			src &= Memory.SRAMMask;
			len &= Memory.SRAMMask;
			s = &Memory.SRAM[src];
			break;
		default:
		case 2:
			src &= 0x3ff;
			len &= 0x3ff;
			s = &Memory.FillRAM[0x3000] + src;
			break;
	}

	if (Memory.FillRAM[0x2230] & 4)
	{
		dst &= Memory.SRAMMask;
		len &= Memory.SRAMMask;
		d = &Memory.SRAM[dst];
	}
	else
	{
		dst &= 0x3ff;
		len &= 0x3ff;
		d = &Memory.FillRAM[0x3000] + dst;
	}

	/* memmove required: Can overlap arbitrarily [Neb] */
	memmove(d, s, len);
	Memory.FillRAM[0x2301] |= 0x20;

	if (Memory.FillRAM[0x220a] & 0x20)
	{
		SA1.Flags |= IRQ_FLAG;
		SA1.IRQActive |= DMA_IRQ_SOURCE;
	}
}

static void SA1ReadVariableLengthData(bool inc, bool no_shift)
{
	uint8_t  s;
	uint32_t data;
	uint32_t addr  = Memory.FillRAM[0x2259] | (Memory.FillRAM[0x225a] << 8) | (Memory.FillRAM[0x225b] << 16);
	uint8_t  shift = Memory.FillRAM[0x2258] & 15;

	if (no_shift)
		shift = 0;
	else if (shift == 0)
		shift = 16;

	s = shift + SA1.variable_bit_pos;

	if (s >= 16)
	{
		addr += (s >> 4) << 1;
		s &= 15;
	}

	data = (SA1GetWord(addr, WRAP_NONE) | (SA1GetWord(addr + 2, WRAP_NONE) << 16)) >> s;
	Memory.FillRAM[0x230c] = (uint8_t) data;
	Memory.FillRAM[0x230d] = (uint8_t) (data >> 8);

	if (!inc)
		return;

	SA1.variable_bit_pos = (SA1.variable_bit_pos + shift) & 15;
	Memory.FillRAM[0x2259] = (uint8_t)  addr;
	Memory.FillRAM[0x225a] = (uint8_t) (addr >> 8);
	Memory.FillRAM[0x225b] = (uint8_t) (addr >> 16);
}
