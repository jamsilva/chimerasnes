#include "chisnes.h"
#include "memmap.h"
#include "ppu.h"
#include "cpuexec.h"
#include "dma.h"
#include "apu.h"
#include "sa1.h"
#include "sdd1.h"
#include "spc7110.h"
#include "spc7110dec.h"

static uint8_t sdd1_decode_buffer[0x10000];

extern int32_t  HDMA_ModeByteCounts[8];
extern uint8_t* HDMAMemPointers[8];
extern uint8_t* HDMABasePointers[8];

void DoDMA(uint8_t Channel)
{
	int32_t  count;
	int32_t  inc;
	SDMA*    d;
	bool     in_sa1_dma  = false;
	uint8_t* in_sdd1_dma = NULL;
	uint8_t* spc7110_dma = NULL;
	bool     s7_wrap     = false;

	if (Channel > 7 || CPU.InDMA)
		return;

	CPU.InDMA = true;
	d         = &DMA[Channel];
	count     = d->TransferBytes;

	/* Prepare for custom chip DMA */
	if (count == 0)
		count = 0x10000;

	inc = d->AAddressFixed ? 0 : (!d->AAddressDecrement ? 1 : -1);

	if ((d->ABank == 0x7E || d->ABank == 0x7F) && d->BAddress == 0x80 && !d->ReverseTransfer)
	{
		d->AAddress += d->TransferBytes;
		/* Does an invalid DMA actually take time?
		 * I'd say yes, since 'invalid' is probably just the WRAM chip
		 * not being able to read and write itself at the same time */
		CPU.Cycles += (d->TransferBytes + 1) * SLOW_ONE_CYCLE;
		goto update_address;
	}

	switch (d->BAddress)
	{
		case 0x18:
		case 0x19:
			if (IPPU.RenderThisFrame)
				FLUSH_REDRAW();

			break;
	}

	if (Settings.Chip == S_DD1)
	{
		if (d->AAddressFixed && Memory.FillRAM[0x4801] > 0)
		{
			uint8_t* in_ptr = GetBasePointer(((d->ABank << 16) | d->AAddress));

			/* XXX: Should probably verify that we're DMAing from ROM?
			 * And somewhere we should make sure we're not running across a mapping boundary too. */
			inc = !d->AAddressDecrement ? 1 : -1;

			if (in_ptr)
			{
				in_ptr += d->AAddress;
				SDD1_decompress(sdd1_decode_buffer, in_ptr, d->TransferBytes);
			}

			in_sdd1_dma = sdd1_decode_buffer;
		}

		Memory.FillRAM[0x4801] = 0;
	}

	if (((Settings.Chip & SPC7110) == SPC7110) && (d->AAddress == 0x4800 || d->ABank == 0x50))
	{
		int32_t c, icount;
		spc7110_dma = s7r.bank50;

		for (c = 0; c < count; c++)
			spc7110_dma[c] = spc7110dec_read();

		icount = (s7r.reg4809 | (s7r.reg480A << 8)) - count;
		s7r.reg4809 =  icount & 0x00ff;
		s7r.reg480A = (icount & 0xff00) >> 8;
		inc = 1;
		d->AAddress -= count;
	}

	if (SA1.in_char_dma && d->BAddress == 0x18 && (d->ABank & 0xf0) == 0x40)
	{
		/* Perform packed bitmap to PPU character format conversion on the
		 * data before transmitting it to V-RAM via-DMA. */
		int32_t  i;
		int32_t  num_chars = 1 << ((Memory.FillRAM[0x2231] >> 2) & 7);
		int32_t  depth = (Memory.FillRAM[0x2231] & 3) == 0 ? 8 : (Memory.FillRAM[0x2231] & 3) == 1 ? 4 : 2;
		int32_t  bytes_per_char = 8 * depth;
		int32_t  bytes_per_line = depth * num_chars;
		int32_t  char_line_bytes = bytes_per_char * num_chars;
		uint32_t addr = (d->AAddress / char_line_bytes) * char_line_bytes;
		uint8_t* base = GetBasePointer((d->ABank << 16) + addr) + addr;
		uint8_t* buffer = &Memory.ROM[MAX_ROM_SIZE - 0x10000];
		uint8_t* p = buffer;
		uint32_t inc = char_line_bytes - (d->AAddress % char_line_bytes);
		uint32_t char_count = inc / bytes_per_char;
		in_sa1_dma = true;

		switch (depth)
		{
			case 2:
				for (i = 0; i < count; i += inc, base += char_line_bytes, inc = char_line_bytes, char_count = num_chars)
				{
					uint32_t j;
					uint8_t* line = base + (num_chars - char_count) * 2;

					for (j = 0; j < char_count && p - buffer < count; j++, line += 2)
					{
						int32_t  b, l;
						uint8_t* q = line;

						for (l = 0; l < 8; l++, q += bytes_per_line)
						{
							for (b = 0; b < 2; b++)
							{
								uint8_t r = q[b];
								p[0]      = (p[0] << 1) | ((r >> 0) & 1);
								p[1]      = (p[1] << 1) | ((r >> 1) & 1);
								p[0]      = (p[0] << 1) | ((r >> 2) & 1);
								p[1]      = (p[1] << 1) | ((r >> 3) & 1);
								p[0]      = (p[0] << 1) | ((r >> 4) & 1);
								p[1]      = (p[1] << 1) | ((r >> 5) & 1);
								p[0]      = (p[0] << 1) | ((r >> 6) & 1);
								p[1]      = (p[1] << 1) | ((r >> 7) & 1);
							}

							p += 2;
						}
					}
				}

				break;
			case 4:
				for (i = 0; i < count; i += inc, base += char_line_bytes, inc = char_line_bytes, char_count = num_chars)
				{
					uint32_t j;
					uint8_t* line = base + (num_chars - char_count) * 4;

					for (j = 0; j < char_count && p - buffer < count; j++, line += 4)
					{
						uint8_t* q = line;
						int32_t  b, l;

						for (l = 0; l < 8; l++, q += bytes_per_line)
						{
							for (b = 0; b < 4; b++)
							{
								uint8_t r = q[b];
								p[0]      = (p[0] << 1) | ((r >> 0) & 1);
								p[1]      = (p[1] << 1) | ((r >> 1) & 1);
								p[16]     = (p[16] << 1) | ((r >> 2) & 1);
								p[17]     = (p[17] << 1) | ((r >> 3) & 1);
								p[0]      = (p[0] << 1) | ((r >> 4) & 1);
								p[1]      = (p[1] << 1) | ((r >> 5) & 1);
								p[16]     = (p[16] << 1) | ((r >> 6) & 1);
								p[17]     = (p[17] << 1) | ((r >> 7) & 1);
							}

							p += 2;
						}

						p += 32 - 16;
					}
				}

				break;
			case 8:
				for (i = 0; i < count; i += inc, base += char_line_bytes, inc = char_line_bytes, char_count = num_chars)
				{
					uint8_t* line = base + (num_chars - char_count) * 8;
					uint32_t j;

					for (j = 0; j < char_count && p - buffer < count; j++, line += 8)
					{
						uint8_t* q = line;
						int32_t  b, l;

						for (l = 0; l < 8; l++, q += bytes_per_line)
						{
							for (b = 0; b < 8; b++)
							{
								uint8_t r = q[b];
								p[0]      = (p[0] << 1) | ((r >> 0) & 1);
								p[1]      = (p[1] << 1) | ((r >> 1) & 1);
								p[16]     = (p[16] << 1) | ((r >> 2) & 1);
								p[17]     = (p[17] << 1) | ((r >> 3) & 1);
								p[32]     = (p[32] << 1) | ((r >> 4) & 1);
								p[33]     = (p[33] << 1) | ((r >> 5) & 1);
								p[48]     = (p[48] << 1) | ((r >> 6) & 1);
								p[49]     = (p[49] << 1) | ((r >> 7) & 1);
							}

							p += 2;
						}

						p += 64 - 16;
					}
				}

				break;
		}
	}

	if (!d->ReverseTransfer)
	{
		uint16_t p    = d->AAddress;
		uint8_t* base = GetBasePointer((d->ABank << 16) + d->AAddress);
		CPU.Cycles += SLOW_ONE_CYCLE * (count + 1); /* reflects extra cycle used by DMA */

		if (!base)
			base = Memory.ROM;

		if (in_sa1_dma)
		{
			base = &Memory.ROM[MAX_ROM_SIZE - 0x10000];
			p    = 0;
		}
		else if (in_sdd1_dma)
		{
			base = in_sdd1_dma;
			p    = 0;
		}
		else if (spc7110_dma)
		{
			base = spc7110_dma;
			p    = 0;
		}

		if (inc > 0)
			d->AAddress += count;
		else if (inc < 0)
			d->AAddress -= count;

		if (d->TransferMode == 0 || d->TransferMode == 2 || d->TransferMode == 6)
		{
			switch (d->BAddress)
			{
				case 0x04: /* OAMDATA */
					do
					{
						ICPU.OpenBus = base[p];
						REGISTER_2104(ICPU.OpenBus);
						p += inc;
					} while (--count > 0);

					break;
				case 0x18: /* VMDATAL */
					IPPU.FirstVRAMRead = true;

					if (!PPU.VMA.FullGraphicCount)
					{
						do
						{
							ICPU.OpenBus = base[p];
							REGISTER_2118_linear(ICPU.OpenBus);
							p += inc;
						} while (--count > 0);
					}
					else
					{
						do
						{
							ICPU.OpenBus = base[p];
							REGISTER_2118_tile(ICPU.OpenBus);
							p += inc;
						} while (--count > 0);
					}

					break;
				case 0x19: /* VMDATAH */
					IPPU.FirstVRAMRead = true;

					if (!PPU.VMA.FullGraphicCount)
					{
						do
						{
							ICPU.OpenBus = base[p];
							REGISTER_2119_linear(ICPU.OpenBus);
							p += inc;
						} while (--count > 0);
					}
					else
					{
						do
						{
							ICPU.OpenBus = base[p];
							REGISTER_2119_tile(ICPU.OpenBus);
							p += inc;
						} while (--count > 0);
					}

					break;
				case 0x22: /* CGDATA */
					do
					{
						ICPU.OpenBus = base[p];
						REGISTER_2122(ICPU.OpenBus);
						p += inc;
					} while (--count > 0);

					break;
				case 0x80: /* WMDATA */
					do
					{
						ICPU.OpenBus = base[p];
						REGISTER_2180(ICPU.OpenBus);
						p += inc;
					} while (--count > 0);

					break;
				default:
					do
					{
						ICPU.OpenBus = base[p];
						SetPPU(ICPU.OpenBus, 0x2100 + d->BAddress);
						p += inc;
					} while (--count > 0);

					break;
			}
		}
		else if (d->TransferMode == 1 || d->TransferMode == 5)
		{
			if (d->BAddress == 0x18)
			{
				/* Write to V-RAM */
				IPPU.FirstVRAMRead = true;

				if (!PPU.VMA.FullGraphicCount)
				{
					while (count > 1)
					{
						ICPU.OpenBus = base[p];
						REGISTER_2118_linear(ICPU.OpenBus);
						p += inc;
						ICPU.OpenBus = base[p];
						REGISTER_2119_linear(ICPU.OpenBus);
						p += inc;
						count -= 2;
					}

					if (count == 1)
					{
						ICPU.OpenBus = base[p];
						REGISTER_2118_linear(ICPU.OpenBus);
					}
				}
				else
				{
					while (count > 1)
					{
						ICPU.OpenBus = base[p];
						REGISTER_2118_tile(ICPU.OpenBus);
						p += inc;
						ICPU.OpenBus = base[p];
						REGISTER_2119_tile(ICPU.OpenBus);
						p += inc;
						count -= 2;
					}

					if (count == 1)
					{
						ICPU.OpenBus = base[p];
						REGISTER_2118_tile(ICPU.OpenBus);
					}
				}
			}
			else /* DMA mode 1 general case */
			{
				while (count > 1)
				{
					ICPU.OpenBus = base[p];
					SetPPU(ICPU.OpenBus, 0x2100 + d->BAddress);
					p += inc;
					ICPU.OpenBus = base[p];
					SetPPU(ICPU.OpenBus, 0x2101 + d->BAddress);
					p += inc;
					count -= 2;
				}

				if (count == 1)
				{
					ICPU.OpenBus = base[p];
					SetPPU(ICPU.OpenBus, 0x2100 + d->BAddress);
				}
			}
		}
		else if (d->TransferMode == 3 || d->TransferMode == 7)
		{
			do
			{
				ICPU.OpenBus = base[p];
				SetPPU(ICPU.OpenBus, 0x2100 + d->BAddress);
				p += inc;

				if (count <= 1)
					break;

				ICPU.OpenBus = base[p];
				SetPPU(ICPU.OpenBus, 0x2100 + d->BAddress);
				p += inc;

				if (count <= 2)
					break;

				ICPU.OpenBus = base[p];
				SetPPU(ICPU.OpenBus, 0x2101 + d->BAddress);
				p += inc;

				if (count <= 3)
					break;

				ICPU.OpenBus = base[p];
				SetPPU(ICPU.OpenBus, 0x2101 + d->BAddress);
				p += inc;
				count -= 4;
			} while (count > 0);
		}
		else if (d->TransferMode == 4)
		{
			do
			{
				ICPU.OpenBus = base[p];
				SetPPU(ICPU.OpenBus, 0x2100 + d->BAddress);
				p += inc;

				if (count <= 1)
					break;

				ICPU.OpenBus = base[p];
				SetPPU(ICPU.OpenBus, 0x2101 + d->BAddress);
				p += inc;

				if (count <= 2)
					break;

				ICPU.OpenBus = base[p];
				SetPPU(ICPU.OpenBus, 0x2102 + d->BAddress);
				p += inc;

				if (count <= 3)
					break;

				ICPU.OpenBus = base[p];
				SetPPU(ICPU.OpenBus, 0x2103 + d->BAddress);
				p += inc;
				count -= 4;
			} while (count > 0);
		}
	}
	else
	{
		/* XXX: DMA is potentially broken here for cases where the dest is
		 * XXX: in the Address Bus B range. Note that this bad dest may not
		 * XXX: cover the whole range of the DMA though, if we transfer
		 * XXX: 65536 bytes only 256 of them may be Address Bus B. */
		do
		{
			switch (d->TransferMode)
			{
				case 0:
				case 2:
				case 6:
					ICPU.OpenBus = GetPPU(0x2100 + d->BAddress);
					SetByte(ICPU.OpenBus, (d->ABank << 16) + d->AAddress);
					d->AAddress += inc;
					--count;
					break;
				case 1:
				case 5:
					ICPU.OpenBus = GetPPU(0x2100 + d->BAddress);
					SetByte(ICPU.OpenBus, (d->ABank << 16) + d->AAddress);
					d->AAddress += inc;

					if (!--count)
						break;

					ICPU.OpenBus = GetPPU(0x2101 + d->BAddress);
					SetByte(ICPU.OpenBus, (d->ABank << 16) + d->AAddress);
					d->AAddress += inc;
					--count;
					break;
				case 3:
				case 7:
					ICPU.OpenBus = GetPPU(0x2100 + d->BAddress);
					SetByte(ICPU.OpenBus, (d->ABank << 16) + d->AAddress);
					d->AAddress += inc;

					if (!--count)
						break;

					ICPU.OpenBus = GetPPU(0x2100 + d->BAddress);
					SetByte(ICPU.OpenBus, (d->ABank << 16) + d->AAddress);
					d->AAddress += inc;

					if (!--count)
						break;

					ICPU.OpenBus = GetPPU(0x2101 + d->BAddress);
					SetByte(ICPU.OpenBus, (d->ABank << 16) + d->AAddress);
					d->AAddress += inc;

					if (!--count)
						break;

					ICPU.OpenBus = GetPPU(0x2101 + d->BAddress);
					SetByte(ICPU.OpenBus, (d->ABank << 16) + d->AAddress);
					d->AAddress += inc;
					--count;
					break;
				case 4:
					ICPU.OpenBus = GetPPU(0x2100 + d->BAddress);
					SetByte(ICPU.OpenBus, (d->ABank << 16) + d->AAddress);
					d->AAddress += inc;

					if (!--count)
						break;

					ICPU.OpenBus = GetPPU(0x2101 + d->BAddress);
					SetByte(ICPU.OpenBus, (d->ABank << 16) + d->AAddress);
					d->AAddress += inc;

					if (!--count)
						break;

					ICPU.OpenBus = GetPPU(0x2102 + d->BAddress);
					SetByte(ICPU.OpenBus, (d->ABank << 16) + d->AAddress);
					d->AAddress += inc;

					if (!--count)
						break;

					ICPU.OpenBus = GetPPU(0x2103 + d->BAddress);
					SetByte(ICPU.OpenBus, (d->ABank << 16) + d->AAddress);
					d->AAddress += inc;
					count--;
					break;
				default:
					count = 0;
					break;
			}
		} while (count);
	}

	IAPU.Executing = Settings.APUEnabled;
	APU_EXECUTE();

	if ((Settings.Chip & GSU) == GSU)
		while (CPU.Cycles > CPU.NextEvent)
			DoHBlankProcessing_SFX();
	else
		while (CPU.Cycles > CPU.NextEvent)
			DoHBlankProcessing_NoSFX();

	if (((Settings.Chip & SPC7110) == SPC7110) && spc7110_dma && s7_wrap)
		free(spc7110_dma);

update_address:
	/* Super Punch-Out requires that the A-BUS address be updated after the DMA transfer. */
	Memory.FillRAM[0x4302 + (Channel << 4)] = (uint8_t) d->AAddress;
	Memory.FillRAM[0x4303 + (Channel << 4)] = d->AAddress >> 8;

	/* Secret of Mana requires that the DMA bytes transfer count be set to zero when DMA has completed. */
	Memory.FillRAM[0x4305 + (Channel << 4)] = 0;
	Memory.FillRAM[0x4306 + (Channel << 4)] = 0;

	DMA[Channel].IndirectAddress = 0;
	d->TransferBytes = 0;
	CPU.InDMA = false;
}

void StartHDMA()
{
	uint8_t i;
	IPPU.HDMA = Memory.FillRAM[0x420c];

	if (IPPU.HDMA != 0)
		CPU.Cycles += ONE_CYCLE * 3;

	for (i = 0; i < 8; i++)
	{
		if (IPPU.HDMA & (1 << i))
		{
			CPU.Cycles += SLOW_ONE_CYCLE;
			DMA[i].LineCount = 0;
			DMA[i].FirstLine = true;
			DMA[i].Address   = DMA[i].AAddress;

			if (DMA[i].HDMAIndirectAddressing)
				CPU.Cycles += SLOW_ONE_CYCLE << 2;
		}

		HDMAMemPointers[i] = NULL;
	}
}

uint8_t DoHDMA(uint8_t byte)
{
	uint8_t mask;
	SDMA* p = &DMA[0];
	int32_t d = 0;
	CPU.InDMA = true;
	CPU.Cycles += ONE_CYCLE * 3;

	for (mask = 1; mask; mask <<= 1, p++, d++)
	{
		if (!(byte & mask))
			continue;

		if (!p->LineCount)
		{
			uint8_t line;
			/* remember, InDMA is set. Get/Set incur no charges! */
			CPU.Cycles += SLOW_ONE_CYCLE;
			line = GetByte((p->ABank << 16) + p->Address);

			if (line == 0x80)
			{
				p->Repeat    = true;
				p->LineCount = 128;
			}
			else
			{
				p->Repeat    = !(line & 0x80);
				p->LineCount = line & 0x7f;
			}

			/* Disable H-DMA'ing into V-RAM (register 2118) for Hook
				* XXX: instead of p->BAddress == 0x18, make SetPPU fail
				* XXX: writes to $2118/9 when appropriate
				*/
			if (!p->LineCount || p->BAddress == 0x18)
			{
				byte &= ~mask;
				p->IndirectAddress += HDMAMemPointers[d] - HDMABasePointers[d];
				Memory.FillRAM[0x4305 + (d << 4)] = (uint8_t) p->IndirectAddress;
				Memory.FillRAM[0x4306 + (d << 4)] = p->IndirectAddress >> 8;
				continue;
			}

			p->Address++;
			p->FirstLine = true;

			if (p->HDMAIndirectAddressing)
			{
				p->IndirectBank = Memory.FillRAM[0x4307 + (d << 4)];
				/* again, no cycle charges while InDMA is set! */
				CPU.Cycles += SLOW_ONE_CYCLE << 2;
				p->IndirectAddress = GetWord((p->ABank << 16) + p->Address);
				p->Address += 2;
			}
			else
			{
				p->IndirectBank    = p->ABank;
				p->IndirectAddress = p->Address;
			}

			HDMABasePointers[d] = HDMAMemPointers[d] = GetMemPointer((p->IndirectBank << 16) + p->IndirectAddress);
		}
		else
			CPU.Cycles += SLOW_ONE_CYCLE;

		if (!HDMAMemPointers[d])
		{
			if (!p->HDMAIndirectAddressing)
			{
				p->IndirectBank    = p->ABank;
				p->IndirectAddress = p->Address;
			}

			if (!(HDMABasePointers[d] = HDMAMemPointers[d] = GetMemPointer((p->IndirectBank << 16) + p->IndirectAddress)))
			{
				/* XXX: Instead of this, goto a slow path that first
					* XXX: verifies src!=Address Bus B, then uses
					* XXX: GetByte(). Or make GetByte return OpenBus
					* XXX: (probably?) for Address Bus B while inDMA. */
				byte &= ~mask;
				continue;
			}
		}

		if (p->Repeat && !p->FirstLine)
		{
			p->LineCount--;
			continue;
		}

		switch (p->TransferMode)
		{
			case 0:
				CPU.Cycles += SLOW_ONE_CYCLE;
				SetPPU(*HDMAMemPointers[d]++, 0x2100 + p->BAddress);
				break;
			case 5:
				CPU.Cycles += 2 * SLOW_ONE_CYCLE;
				SetPPU(HDMAMemPointers[d][0], 0x2100 + p->BAddress);
				SetPPU(HDMAMemPointers[d][1], 0x2101 + p->BAddress);
				HDMAMemPointers[d] += 2;
				/* fall through */
			case 1:
				CPU.Cycles += 2 * SLOW_ONE_CYCLE;
				SetPPU(HDMAMemPointers[d][0], 0x2100 + p->BAddress);
				ICPU.OpenBus = HDMAMemPointers[d][1];
				SetPPU(HDMAMemPointers[d][1], 0x2101 + p->BAddress);
				HDMAMemPointers[d] += 2;
				break;
			case 2:
			case 6:
				CPU.Cycles += 2 * SLOW_ONE_CYCLE;
				SetPPU(HDMAMemPointers[d][0], 0x2100 + p->BAddress);
				SetPPU(HDMAMemPointers[d][1], 0x2100 + p->BAddress);
				HDMAMemPointers[d] += 2;
				break;
			case 3:
			case 7:
				CPU.Cycles += 4 * SLOW_ONE_CYCLE;
				SetPPU(HDMAMemPointers[d][0], 0x2100 + p->BAddress);
				SetPPU(HDMAMemPointers[d][1], 0x2100 + p->BAddress);
				SetPPU(HDMAMemPointers[d][2], 0x2101 + p->BAddress);
				SetPPU(HDMAMemPointers[d][3], 0x2101 + p->BAddress);
				HDMAMemPointers[d] += 4;
				break;
			case 4:
				CPU.Cycles += 4 * SLOW_ONE_CYCLE;
				SetPPU(HDMAMemPointers[d][0], 0x2100 + p->BAddress);
				SetPPU(HDMAMemPointers[d][1], 0x2101 + p->BAddress);
				SetPPU(HDMAMemPointers[d][2], 0x2102 + p->BAddress);
				SetPPU(HDMAMemPointers[d][3], 0x2103 + p->BAddress);
				HDMAMemPointers[d] += 4;
				break;
		}

		if (!p->HDMAIndirectAddressing)
			p->Address += HDMA_ModeByteCounts[p->TransferMode];

		p->IndirectAddress += HDMA_ModeByteCounts[p->TransferMode];
		/* XXX: Check for p->IndirectAddress crossing a mapping boundary,
			* XXX: and invalidate HDMAMemPointers[d] */
		p->FirstLine = false;
		--p->LineCount;
	}

	CPU.InDMA = false;
	return byte;
}

void ResetDMA()
{
	int32_t d;

	for (d = 0; d < 8; d++)
	{
		DMA[d].ReverseTransfer = true;
		DMA[d].HDMAIndirectAddressing = true;
		DMA[d].AAddressFixed = true;
		DMA[d].AAddressDecrement = true;
		DMA[d].TransferMode = 7;
		DMA[d].AAddress = 0xffff;
		DMA[d].BAddress = 0xff;
		DMA[d].ABank = 0xff;
		DMA[d].Address = 0xffff;
		DMA[d].TransferBytes = 0xffff;
		DMA[d].IndirectAddress = 0xffff;
	}
}
