#include <time.h>

#include "spc7110.h"
#include "spc7110dec.h"
#include "memmap.h"

SPC7110Regs s7r; /* SPC7110 registers, about 33KB */
S7RTC rtc_f9; /* FEOEZ (and Shounen Jump no Shou) RTC */

void UpdateRTC(); /* S-RTC function hacked to work with the RTC */

void InitSPC7110() /* Emulate power on state */
{
	spc7110dec_init();
	memset(&s7r, 0, sizeof(SPC7110Regs));
	s7r.DataRomOffset = 0x00100000; /* handy constant! */
	s7r.DataRomSize = Memory.CalculatedSize - s7r.DataRomOffset;
	s7r.reg4832 = 1;
	s7r.reg4833 = 2;
	s7r.AlignBy = 1;
}

static void SetSPC7110SRAMMap(uint8_t newstate)
{
	if (newstate & 0x80)
	{
		Memory.Map[0x006] = (uint8_t*) MAP_HIROM_SRAM;
		Memory.Map[0x007] = (uint8_t*) MAP_HIROM_SRAM;
		Memory.Map[0x306] = (uint8_t*) MAP_HIROM_SRAM;
		Memory.Map[0x307] = (uint8_t*) MAP_HIROM_SRAM;
	}
	else
	{
		Memory.Map[0x006] = (uint8_t*) MAP_RONLY_SRAM;
		Memory.Map[0x007] = (uint8_t*) MAP_RONLY_SRAM;
		Memory.Map[0x306] = (uint8_t*) MAP_RONLY_SRAM;
		Memory.Map[0x307] = (uint8_t*) MAP_RONLY_SRAM;
	}
}

uint8_t GetSPC7110(uint16_t Address) /* reads SPC7110 and RTC registers. */
{
	uint32_t i;
	uint8_t tmp;

	switch (Address)
	{
		/* decompressed data read port. decrements 4809-A (with wrap)
		   4805-6 is the offset into the bank
		   AlignBy is set (afaik) at decompression time, and is the offset multiplier */
		case 0x4800:
		{
			uint16_t count = s7r.reg4809 | (s7r.reg480A << 8);

			if (count > 0)
				count--;
			else
				count = 0xFFFF;

			s7r.reg4809 = 0x00ff & count;
			s7r.reg480A = (0xff00 & count) >> 8;
			s7r.reg4800 = spc7110dec_read();
			return s7r.reg4800;
		}
		case 0x4801: /* table register low */
			return s7r.reg4801;
		case 0x4802: /* table register middle */
			return s7r.reg4802;
		case 0x4803: /* table register high */
			return s7r.reg4803;
		case 0x4804: /* index of pointer in table (each entry is 4 bytes) */
			return s7r.reg4804;
		case 0x4805: /* offset register low */
			return s7r.reg4805;
		case 0x4806: /* offset register high */
			return s7r.reg4806;
		/* DMA channel (not that I see this usually set,
		   regardless of what channel DMA is on) */
		case 0x4807:
			return s7r.reg4807;
		/* C r/w option, unknown, defval:00 is what Dark Force says
		   afaict, Snes9x doesn't use this at all. */
		case 0x4808:
			return s7r.reg4808;
		/* C-Length low
		   counts down the number of bytes left to read from the decompression buffer.
		   this is set by the ROM, and wraps on bounds. */
		case 0x4809:
			return s7r.reg4809;
		case 0x480A: /* C Length high */
			return s7r.reg480A;
		case 0x480B: /* Offset enable. If this is zero, 4805-6 are useless. Emulated by setting AlignBy to 0 */
			return s7r.reg480B;
		case 0x480C: /* decompression finished: just emulated by switching each read. */
			s7r.reg480C ^= 0x80;
			return s7r.reg480C ^ 0x80;
		/* Data access port
		 * reads from the data ROM (anywhere over the first 8 mbits
		 * behavior is complex, will document later,
		 * possibly missing cases, because of the number of switches in play */
		case 0x4810:
			if (s7r.written == 0)
				return 0;

			if ((s7r.written & 0x07) != 0x07)
				return 0;

			i = ((s7r.reg4813 << 16) | (s7r.reg4812 << 8) | s7r.reg4811) % s7r.DataRomSize;

			if (s7r.reg4818 & 0x02)
			{
				if (s7r.reg4818 & 0x08)
				{
					int16_t r4814 = (s7r.reg4815 << 8) | s7r.reg4814;
					i += r4814;
					r4814++;
					s7r.reg4815 = (uint8_t) (r4814 >> 8);
					s7r.reg4814 = (uint8_t) (r4814 & 0x00FF);
				}
				else
				{
					uint16_t r4814 = (s7r.reg4815 << 8) | s7r.reg4814;
					i += r4814;

					if (r4814 != 0xFFFF)
						r4814++;
					else
						r4814 = 0;

					s7r.reg4815 = (uint8_t) (r4814 >> 8);
					s7r.reg4814 = (uint8_t) (r4814 & 0x00FF);
				}
			}

			i += s7r.DataRomOffset;
			tmp = Memory.ROM[i];
			i = (s7r.reg4813 << 16) | (s7r.reg4812 << 8) | s7r.reg4811;

			if (s7r.reg4818 & 0x02)
			{
				i %= s7r.DataRomSize;
				s7r.reg4811 = i & 0x00FF;
				s7r.reg4812 = (i & 0x00FF00) >> 8;
				s7r.reg4813 = (i & 0xFF0000) >> 16;
				return tmp;
			}

			if (s7r.reg4818 & 0x01)
			{
				if (s7r.reg4818 & 0x04)
				{
					int16_t inc = (s7r.reg4817 << 8) | s7r.reg4816;

					if (s7r.reg4818 & 0x10)
					{
						if (s7r.reg4818 & 0x08)
						{
							int16_t r4814;
							r4814 = (s7r.reg4815 << 8) | s7r.reg4814;
							r4814 += inc;
							s7r.reg4815 = (r4814 & 0xFF00) >> 8;
							s7r.reg4814 = r4814 & 0xFF;
						}
						else
						{
							uint16_t r4814;
							r4814 = (s7r.reg4815 << 8) | s7r.reg4814;
							r4814 += inc;
							s7r.reg4815 = (r4814 & 0xFF00) >> 8;
							s7r.reg4814 = r4814 & 0xFF;
						}
					}
					else
						i += inc;
				}
				else
				{
					uint16_t inc = (s7r.reg4817 << 8) | s7r.reg4816;

					if (s7r.reg4818 & 0x10)
					{
						if (s7r.reg4818 & 0x08)
						{
							int16_t r4814;
							r4814 = (s7r.reg4815 << 8) | s7r.reg4814;
							r4814 += inc;
							s7r.reg4815 = (r4814 & 0xFF00) >> 8;
							s7r.reg4814 = r4814 & 0xFF;
						}
						else
						{
							uint16_t r4814;
							r4814 = (s7r.reg4815 << 8) | s7r.reg4814;
							r4814 += inc;
							s7r.reg4815 = (r4814 & 0xFF00) >> 8;
							s7r.reg4814 = r4814 & 0xFF;
						}
					}
					else
						i += inc;
				}
			}
			else if (!(s7r.reg4818 & 0x10))
				++i;
			else if (s7r.reg4818 & 0x08)
			{
				int16_t r4814;
				r4814 = (s7r.reg4815 << 8) | s7r.reg4814;
				r4814 += 1;
				s7r.reg4815 = (r4814 & 0xFF00) >> 8;
				s7r.reg4814 = r4814 & 0xFF;
			}
			else
			{
				uint16_t r4814;
				r4814 = (s7r.reg4815 << 8) | s7r.reg4814;
				r4814 += 1;
				s7r.reg4815 = (r4814 & 0xFF00) >> 8;
				s7r.reg4814 = r4814 & 0xFF;
			}

			i %= s7r.DataRomSize;
			s7r.reg4811 = i & 0x00FF;
			s7r.reg4812 = (i & 0x00FF00) >> 8;
			s7r.reg4813 = (i & 0xFF0000) >> 16;
			return tmp;
		case 0x4811: /* direct read address low */
			return s7r.reg4811;
		case 0x4812: /* direct read address middle */
			return s7r.reg4812;
		case 0x4813: /* direct read access high */
			return s7r.reg4813;
		case 0x4814: /* read adjust low */
			return s7r.reg4814;
		case 0x4815: /* read adjust high */
			return s7r.reg4815;
		case 0x4816: /* read increment low */
			return s7r.reg4816;
		case 0x4817: /* read increment high */
			return s7r.reg4817;
		case 0x4818: /* Data ROM command mode; essentially, this controls the insane code of $4810 and $481A */
			return s7r.reg4818;
		/* read after adjust port
		   what this does, besides more nasty stuff like 4810,
		   I don't know. Just assume it is a different implementation of $4810,
		   if it helps your sanity */
		case 0x481A:
			if (s7r.written != 0x1F)
				return 0;

			i = ((s7r.reg4813 << 16) | (s7r.reg4812 << 8) | s7r.reg4811);

			if (s7r.reg4818 & 0x08)
				i += ((int16_t) (s7r.reg4815 << 8)) | s7r.reg4814;
			else
				i += (s7r.reg4815 << 8) | s7r.reg4814;

			i %= s7r.DataRomSize;
			i += s7r.DataRomOffset;
			tmp = Memory.ROM[i];

			if ((s7r.reg4818 & 0x60) != 0x60)
				return tmp;

			i = ((s7r.reg4813 << 16) | (s7r.reg4812 << 8) | s7r.reg4811);

			if (!(s7r.reg4818 & 0x10))
			{
				if (s7r.reg4818 & 0x08)
				{
					int16_t adj = ((int16_t) (s7r.reg4815 << 8)) | s7r.reg4814;
					i += adj;
				}
				else
				{
					uint16_t adj;
					adj = (s7r.reg4815 << 8) | s7r.reg4814;
					i += adj;
				}

				i %= s7r.DataRomSize;
				s7r.reg4811 = i & 0x00FF;
				s7r.reg4812 = (i & 0x00FF00) >> 8;
				s7r.reg4813 = ((i & 0xFF0000) >> 16);
			}
			else if (s7r.reg4818 & 0x08)
			{
				int16_t adj = ((int16_t) (s7r.reg4815 << 8)) | s7r.reg4814;
				adj += adj;
				s7r.reg4815 = (adj & 0xFF00) >> 8;
				s7r.reg4814 = adj & 0xFF;
			}
			else
			{
				uint16_t adj;
				adj = (s7r.reg4815 << 8) | s7r.reg4814;
				adj += adj;
				s7r.reg4815 = (adj & 0xFF00) >> 8;
				s7r.reg4814 = adj & 0xFF;
			}

			return tmp;
		case 0x4820: /* multiplicand low or dividend lowest */
			return s7r.reg4820;
		case 0x4821: /* multiplicand high or divdend lower */
			return s7r.reg4821;
		case 0x4822: /* dividend higher */
			return s7r.reg4822;
		case 0x4823: /* dividend highest */
			return s7r.reg4823;
		case 0x4824: /* multiplier low */
			return s7r.reg4824;
		case 0x4825: /* multiplier high */
			return s7r.reg4825;
		case 0x4826: /* divisor low */
			return s7r.reg4826;
		case 0x4827: /* divisor high */
			return s7r.reg4827;
		case 0x4828: /* result lowest */
			return s7r.reg4828;
		case 0x4829: /* result lower */
			return s7r.reg4829;
		case 0x482A: /* result higher */
			return s7r.reg482A;
		case 0x482B: /* result highest */
			return s7r.reg482B;
		case 0x482C: /* remainder (division) low */
			return s7r.reg482C;
		case 0x482D: /* remainder (division) high */
			return s7r.reg482D;
		case 0x482E: /* signed/unsigned */
			return s7r.reg482E;
		case 0x482F: /* finished flag, emulated as an on-read toggle. */
			if (!s7r.reg482F)
				return 0;

			s7r.reg482F = 0;
			return 0x80;
		case 0x4830: /* SRAM toggle */
			return s7r.reg4830;
		case 0x4831: /* DX bank mapping */
			return s7r.reg4831;
		case 0x4832: /* EX bank mapping */
			return s7r.reg4832;
		case 0x4833: /* FX bank mapping */
			return s7r.reg4833;
		case 0x4834: /* SRAM mapping? We have no clue! */
			return s7r.reg4834;
		case 0x4840: /* RTC enable */
			if (Settings.Chip != SPC7110RTC)
				return Address >> 8;

			return s7r.reg4840;
		case 0x4841: /* command/index/value of RTC (essentially, zero unless we're in read mode */
			if (Settings.Chip != SPC7110RTC)
				return Address >> 8;

			if (!rtc_f9.init)
				return 0;

			UpdateRTC();
			tmp = rtc_f9.reg[rtc_f9.index];
			rtc_f9.index++;
			rtc_f9.index %= 0x10;
			return tmp;
		case 0x4842: /* RTC done flag */
			if (Settings.Chip != SPC7110RTC)
				return Address >> 8;

			s7r.reg4842 ^= 0x80;
			return s7r.reg4842 ^ 0x80;
		default:
			return 0x00;
	}
}

static uint32_t datarom_addr(uint32_t addr)
{
	uint32_t size;

	if (Memory.CalculatedSize > 0x500000)
		size = Memory.CalculatedSize - 0x200000;
	else
		size = Memory.CalculatedSize - 0x100000;

	while (addr >= size)
		addr -= size;

	return addr + 0x100000;
}

void SetSPC7110(uint8_t data, uint16_t Address)
{
	switch (Address)
	{
		case 0x4801: /* table low, middle, and high. */
			s7r.reg4801 = data;
			break;
		case 0x4802:
			s7r.reg4802 = data;
			break;
		case 0x4803:
			s7r.reg4803 = data;
			break;
		case 0x4804: /* table index (4 byte entries, bigendian with a multiplier byte) */
			s7r.reg4804 = data;
			break;
		case 0x4805: /* offset low */
			s7r.reg4805 = data;
			break;
		case 0x4806: /* offset high, starts decompression */
		{
			uint32_t table = (s7r.reg4801 + (s7r.reg4802 << 8) + (s7r.reg4803 << 16));
			uint32_t index = (s7r.reg4804 << 2);
			uint32_t addr = datarom_addr(table + index);
			uint32_t mode = (Memory.ROM[addr + 0]);
			uint32_t offset = (Memory.ROM[addr + 1] << 16) + (Memory.ROM[addr + 2] << 8) + (Memory.ROM[addr + 3]);
			s7r.reg4806 = data;
			spc7110dec_clear(mode, offset, (s7r.reg4805 + (s7r.reg4806 << 8)) << mode);
			s7r.reg480C &= 0x7F;
			break;
		}
		case 0x4807: /* DMA channel register (Is it used??) */
			s7r.reg4807 = data;
			break;
		case 0x4808:
			s7r.reg4808 = data;
			break;
		case 0x4809: /* C-Length low */
			s7r.reg4809 = data;
			break;
		case 0x480A: /* C-Length high */
			s7r.reg480A = data;
			break;
		case 0x480B: /* Offset enable */
		{
			int32_t table, j;
			s7r.reg480B = data;
			table = (s7r.reg4803 << 16) | (s7r.reg4802 << 8) | s7r.reg4801;
			j = 4 * s7r.reg4804 + s7r.DataRomOffset + table;

			if (s7r.reg480B == 0)
			{
				s7r.AlignBy = 0;
				break;
			}

			switch (Memory.ROM[j])
			{
				default:
					s7r.AlignBy = 1;
					break;
				case 0x01:
				case 0x02:
				case 0x03:
					s7r.AlignBy = 1 << Memory.ROM[j];
					break;
			}

			break;
		}
		case 0x4811: /* Data port address low */
			s7r.reg4811 = data;
			s7r.written |= 0x01;
			break;
		case 0x4812: /* data port address middle */
			s7r.reg4812 = data;
			s7r.written |= 0x02;
			break;
		case 0x4813: /* data port address high */
			s7r.reg4813 = data;
			s7r.written |= 0x04;
			break;
		case 0x4814: /* data port adjust low (has a funky immediate increment mode) */
			s7r.reg4814 = data;

			if (!(s7r.reg4818 & 0x02))
			{
				s7r.written |= 0x08;
				break;
			}

			if ((s7r.reg4818 & 0x20) && !(s7r.reg4818 & 0x40))
			{
				s7r.offset_add |= 0x01;

				if (s7r.offset_add == 3 && !(s7r.reg4818 & 0x10))
				{
					uint32_t i = (s7r.reg4813 << 16) | (s7r.reg4812 << 8) | s7r.reg4811;

					if (s7r.reg4818 & 0x08)
						i += (int8_t) s7r.reg4814;
					else
						i += s7r.reg4814;

					i %= s7r.DataRomSize;
					s7r.reg4811 =  i & 0x00FF;
					s7r.reg4812 = (i & 0x00FF00) >> 8;
					s7r.reg4813 = (i & 0xFF0000) >> 16;
				}
			}
			else if ((s7r.reg4818 & 0x40) && !(s7r.reg4818 & 0x20))
			{
				s7r.offset_add |= 0x01;

				if (s7r.offset_add == 3 && !(s7r.reg4818 & 0x10))
				{
					uint32_t i = (s7r.reg4813 << 16) | (s7r.reg4812 << 8) | s7r.reg4811;

					if (s7r.reg4818 & 0x08)
						i += ((int16_t) (s7r.reg4815 << 8)) | s7r.reg4814;
					else
						i += (s7r.reg4815 << 8) | s7r.reg4814;

					i %= s7r.DataRomSize;
					s7r.reg4811 =  i & 0x00FF;
					s7r.reg4812 = (i & 0x00FF00) >> 8;
					s7r.reg4813 = (i & 0xFF0000) >> 16;
				}
			}

			s7r.written |= 0x08;
			break;
		case 0x4815: /* data port adjust high (has a funky immediate increment mode) */
			s7r.reg4815 = data;

			if (!(s7r.reg4818 & 0x02))
			{
				s7r.written |= 0x10;
				break;
			}

			if (s7r.reg4818 & 0x20 && !(s7r.reg4818 & 0x40))
			{
				s7r.offset_add |= 0x02;

				if (s7r.offset_add == 3 && !(s7r.reg4818 & 0x10))
				{
					uint32_t i = (s7r.reg4813 << 16) | (s7r.reg4812 << 8) | s7r.reg4811;

					if (s7r.reg4818 & 0x08)
						i += (int8_t) s7r.reg4814;
					else
						i += s7r.reg4814;

					i %= s7r.DataRomSize;
					s7r.reg4811 =  i & 0x00FF;
					s7r.reg4812 = (i & 0x00FF00) >> 8;
					s7r.reg4813 = (i & 0xFF0000) >> 16;
				}
			}
			else if (s7r.reg4818 & 0x40 && !(s7r.reg4818 & 0x20))
			{
				s7r.offset_add |= 0x02;

				if (s7r.offset_add == 3 && !(s7r.reg4818 & 0x10))
				{
					uint32_t i = (s7r.reg4813 << 16) | (s7r.reg4812 << 8) | s7r.reg4811;

					if (s7r.reg4818 & 0x08)
						i += ((int16_t) (s7r.reg4815 << 8)) | s7r.reg4814;
					else
						i += (s7r.reg4815 << 8) | s7r.reg4814;

					i %= s7r.DataRomSize;
					s7r.reg4811 =  i & 0x00FF;
					s7r.reg4812 = (i & 0x00FF00) >> 8;
					s7r.reg4813 = (i & 0xFF0000) >> 16;
				}
			}

			s7r.written |= 0x10;
			break;
		case 0x4816: /* data port increment low */
			s7r.reg4816 = data;
			break;
		case 0x4817: /* data port increment high */
			s7r.reg4817 = data;
			break;
		case 0x4818: /* data port mode switches; note that it starts inactive. */
			if ((s7r.written & 0x18) != 0x18)
				break;

			s7r.offset_add = 0;
			s7r.reg4818 = data;
			break;
		case 0x4820: /* multiplicand low or dividend lowest */
			s7r.reg4820 = data;
			break;
		case 0x4821: /* multiplicand high or dividend lower */
			s7r.reg4821 = data;
			break;
		case 0x4822: /* dividend higher */
			s7r.reg4822 = data;
			break;
		case 0x4823: /* dividend highest */
			s7r.reg4823 = data;
			break;
		case 0x4824: /* multiplier low */
			s7r.reg4824 = data;
			break;
		case 0x4825: /* multiplier high (triggers operation) */
			s7r.reg4825 = data;

			if (s7r.reg482E & 0x01)
			{
				int16_t m1 = (int16_t) ((s7r.reg4824) | (s7r.reg4825 << 8));
				int16_t m2 = (int16_t) ((s7r.reg4820) | (s7r.reg4821 << 8));
				int32_t mul = m1 * m2;
				s7r.reg4828 = (uint8_t)  (mul & 0x000000FF);
				s7r.reg4829 = (uint8_t) ((mul & 0x0000FF00) >> 8);
				s7r.reg482A = (uint8_t) ((mul & 0x00FF0000) >> 16);
				s7r.reg482B = (uint8_t) ((mul & 0xFF000000) >> 24);
			}
			else
			{
				uint16_t m1 = (uint16_t) ((s7r.reg4824) | (s7r.reg4825 << 8));
				uint16_t m2 = (uint16_t) ((s7r.reg4820) | (s7r.reg4821 << 8));
				uint32_t mul = m1 * m2;
				s7r.reg4828 = (uint8_t)  (mul & 0x000000FF);
				s7r.reg4829 = (uint8_t) ((mul & 0x0000FF00) >> 8);
				s7r.reg482A = (uint8_t) ((mul & 0x00FF0000) >> 16);
				s7r.reg482B = (uint8_t) ((mul & 0xFF000000) >> 24);
			}

			s7r.reg482F = 0x80;
			break;
		case 0x4826: /* divisor low */
			s7r.reg4826 = data;
			break;
		case 0x4827: /* divisor high (triggers operation) */
			s7r.reg4827 = data;

			if (s7r.reg482E & 0x01)
			{
				int32_t quotient;
				int16_t remainder;
				int32_t dividend = (int32_t) (s7r.reg4820 | (s7r.reg4821 << 8) | (s7r.reg4822 << 16) | (s7r.reg4823 << 24));
				int16_t divisor = (int16_t) (s7r.reg4826 | (s7r.reg4827 << 8));

				if (divisor != 0)
				{
					quotient  = (int32_t) (dividend / divisor);
					remainder = (int16_t) (dividend % divisor);
				}
				else
				{
					quotient = 0;
					remainder = dividend & 0x0000FFFF;
				}

				s7r.reg4828 = (uint8_t)  (quotient  & 0x000000FF);
				s7r.reg4829 = (uint8_t) ((quotient  & 0x0000FF00) >> 8);
				s7r.reg482A = (uint8_t) ((quotient  & 0x00FF0000) >> 16);
				s7r.reg482B = (uint8_t) ((quotient  & 0xFF000000) >> 24);
				s7r.reg482C = (uint8_t)   remainder & 0x00FF;
				s7r.reg482D = (uint8_t) ((remainder & 0xFF00) >> 8);
			}
			else
			{
				uint32_t quotient;
				uint16_t remainder;
				uint32_t dividend = (uint32_t) (s7r.reg4820 | (s7r.reg4821 << 8) | (s7r.reg4822 << 16) | (s7r.reg4823 << 24));
				uint16_t divisor = (uint16_t) (s7r.reg4826 | (s7r.reg4827 << 8));

				if (divisor != 0)
				{
					quotient = (uint32_t) (dividend / divisor);
					remainder = (uint16_t) (dividend % divisor);
				}
				else
				{
					quotient = 0;
					remainder = dividend & 0x0000FFFF;
				}

				s7r.reg4828 = (uint8_t)  (quotient  & 0x000000FF);
				s7r.reg4829 = (uint8_t) ((quotient  & 0x0000FF00) >> 8);
				s7r.reg482A = (uint8_t) ((quotient  & 0x00FF0000) >> 16);
				s7r.reg482B = (uint8_t) ((quotient  & 0xFF000000) >> 24);
				s7r.reg482C = (uint8_t)   remainder & 0x00FF;
				s7r.reg482D = (uint8_t) ((remainder & 0xFF00) >> 8);
			}

			s7r.reg482F = 0x80;
			break;
		/* result registers are possibly read-only */

		/* reset: writes here nuke the whole math unit
		   Zero indicates unsigned math, resets with non-zero values turn on signed math */
		case 0x482E:
			s7r.reg4820 = s7r.reg4821 = s7r.reg4822 = s7r.reg4823 = s7r.reg4824 = s7r.reg4825 = s7r.reg4826 = s7r.reg4827 = s7r.reg4828 = s7r.reg4829 = s7r.reg482A = s7r.reg482B = s7r.reg482C = s7r.reg482D = 0;
			s7r.reg482E = data;
			break;
		/* math status register possibly read only */
		case 0x4830: /* SRAM toggle */
			SetSPC7110SRAMMap(data);
			s7r.reg4830 = data;
			break;
		case 0x4831: /* Bank DX mapping */
			s7r.reg4831 = data;
			break;
		case 0x4832: /* Bank EX mapping */
			s7r.reg4832 = data;
			break;
		case 0x4833: /* Bank FX mapping */
			s7r.reg4833 = data;
			break;
		case 0x4834: /* S-RAM mapping? who knows? */
			s7r.reg4834 = data;
			break;
		case 0x4840: /* RTC Toggle */
			if (!data)
			{
				UpdateRTC();
				s7r.reg4840 = data;
				break;
			}

			if (data & 0x01)
			{
				s7r.reg4842 = 0x80;
				rtc_f9.init = false;
				rtc_f9.index = -1;
			}

			s7r.reg4840 = data;
			break;
		case 0x4841: /* RTC init/command/index register */
			if (!rtc_f9.init)
			{
				if (data == 0x03 || data == 0x0C)
				{
					rtc_f9.init = true;
					rtc_f9.control = data;
					rtc_f9.index = -1;
				}

				break;
			}

			if (rtc_f9.index == -1)
			{
				rtc_f9.index = data & 0x0F;
				break;
			}

			if (rtc_f9.control == 0x0C)
			{
				rtc_f9.index = data & 0x0F;
				s7r.reg4842 = 0x80;
				rtc_f9.last_used = time(NULL);
				break;
			}

			if (rtc_f9.index == 0x0D)
			{
				if (data & 0x08)
				{
					if (rtc_f9.reg[1] < 3)
					{
						UpdateRTC();
						rtc_f9.reg[0] = 0;
						rtc_f9.reg[1] = 0;
						rtc_f9.last_used = time(NULL);
					}
					else
					{
						UpdateRTC();
						rtc_f9.reg[0] = 0;
						rtc_f9.reg[1] = 0;
						rtc_f9.last_used = time(NULL) - 60;
						UpdateRTC();
						rtc_f9.last_used = time(NULL);
					}

					data &= 0x07;
				}

				if (rtc_f9.reg[0x0D] & 0x01)
				{
					if (!(data & 0x01))
					{
						rtc_f9.reg[rtc_f9.index & 0x0F] = data;
						rtc_f9.last_used = time(NULL) - 1;
						UpdateRTC();
						rtc_f9.last_used = time(NULL);
					}
				}
			}

			if (rtc_f9.index == 0x0F)
			{
				if (data & 0x01 && !(rtc_f9.reg[0x0F] & 0x01))
				{
					UpdateRTC();
					rtc_f9.reg[0] = 0;
					rtc_f9.reg[1] = 0;
					rtc_f9.last_used = time(NULL);
				}

				if (data & 0x02 && !(rtc_f9.reg[0x0F] & 0x02))
				{
					UpdateRTC();
					rtc_f9.last_used = time(NULL);
				}
			}

			rtc_f9.reg[rtc_f9.index & 0x0F] = data;
			s7r.reg4842 = 0x80;
			rtc_f9.index = (rtc_f9.index + 1) % 0x10;
			break;
	}
}

uint8_t GetSPC7110Byte(uint32_t Address) /* emulate the SPC7110's ability to remap banks Dx, Ex, and Fx. */
{
	uint32_t i = 0;

	switch ((Address & 0xF00000) >> 16)
	{
		case 0xD0:
			i = s7r.reg4831;
			break;
		case 0xE0:
			i = s7r.reg4832;
			break;
		case 0xF0:
			i = s7r.reg4833;
			break;
	}

	i <<= 20;
	i += Address & 0x0FFFFF;
	i += s7r.DataRomOffset;
	return Memory.ROM[i];
}

int32_t RTCDaysInMonth(int32_t month, int32_t year)
{
	switch (month)
	{
		case 2:
			if (!(year & 3)) /* DKJM2 only uses 199x - 22xx */
				return 29;

			return 28;
		case 4:
		case 6:
		case 9:
		case 11:
			return 30;
		default: /* months 1,3,5,7,8,10,12 */
			return 31;
	}
}

#define MINUTETICKS 60
#define HOURTICKS  (60 * MINUTETICKS)
#define DAYTICKS   (24 * HOURTICKS)

void UpdateRTC() /* Advance the RTC time */
{
	time_t cur_systime;
	int32_t seconds, minutes, hours, days, month, year, temp_days, year_tens, year_ones, time_diff;

	/* Keep track of game time by computing the number of seconds that pass on the system
	   clock and adding the same number of seconds to the RTC clock structure. */
	if (!rtc_f9.init || (rtc_f9.reg[0x0D] & 0x01) != 0 || (rtc_f9.reg[0x0F] & 0x03) != 0)
		return;

	/* This method assumes one time_t clock tick is one second */
	cur_systime = time(NULL);
	time_diff = (int32_t) (cur_systime - rtc_f9.last_used);
	rtc_f9.last_used = cur_systime;

	if (time_diff <= 0)
		return;

	if (time_diff > DAYTICKS)
	{
		days = time_diff / DAYTICKS;
		time_diff = time_diff - days * DAYTICKS;
	}
	else
		days = 0;

	if (time_diff > HOURTICKS)
	{
		hours = time_diff / HOURTICKS;
		time_diff = time_diff - hours * HOURTICKS;
	}
	else
		hours = 0;

	if (time_diff > MINUTETICKS)
	{
		minutes = time_diff / MINUTETICKS;
		time_diff = time_diff - minutes * MINUTETICKS;
	}
	else
		minutes = 0;

	if (time_diff > 0)
		seconds = time_diff;
	else
		seconds = 0;

	seconds += (rtc_f9.reg[1] * 10 + rtc_f9.reg[0]);

	if (seconds >= 60)
	{
		seconds -= 60;
		minutes += 1;
	}

	minutes += (rtc_f9.reg[3] * 10 + rtc_f9.reg[2]);

	if (minutes >= 60)
	{
		minutes -= 60;
		hours += 1;
	}

	hours += (rtc_f9.reg[5] * 10 + rtc_f9.reg[4]);

	if (hours >= 24)
	{
		hours -= 24;
		days += 1;
	}

	year = rtc_f9.reg[11] * 10 + rtc_f9.reg[10];
	year += (1900);
	month = rtc_f9.reg[8] + 10 * rtc_f9.reg[9];
	rtc_f9.reg[12] += days;
	days += (rtc_f9.reg[7] * 10 + rtc_f9.reg[6]);

	if (days > 0)
	{
		while (days > (temp_days = RTCDaysInMonth(month, year)))
		{
			days -= temp_days;
			month += 1;

			if (month > 12)
			{
				year += 1;
				month = 1;
			}
		}
	}

	year_tens       = year % 100;
	year_ones       = year_tens % 10;
	year_tens      /= 10;
	rtc_f9.reg[0]   = seconds % 10;
	rtc_f9.reg[1]   = seconds / 10;
	rtc_f9.reg[2]   = minutes % 10;
	rtc_f9.reg[3]   = minutes / 10;
	rtc_f9.reg[4]   = hours % 10;
	rtc_f9.reg[5]   = hours / 10;
	rtc_f9.reg[6]   = days % 10;
	rtc_f9.reg[7]   = days / 10;
	rtc_f9.reg[8]   = month % 10;
	rtc_f9.reg[9]   = month / 10;
	rtc_f9.reg[10]  = year_ones;
	rtc_f9.reg[11]  = year_tens;
	rtc_f9.reg[12] %= 7;
	return;
}

uint8_t* GetBasePointerSPC7110(uint32_t Address) /* allows DMA from the ROM (is this even possible on the SPC7110?) */
{
	uint32_t i = 0;

	switch ((Address & 0xF00000) >> 16)
	{
		case 0xD0:
			i = s7r.reg4831;
			break;
		case 0xE0:
			i = s7r.reg4832;
			break;
		case 0xF0:
			i = s7r.reg4833;
			break;
	}

	i <<= 20;
	i += Address & 0x0F0000;
	return &Memory.ROM[i];
}

void DeinitSPC7110()
{
	spc7110dec_deinit();
	Settings.Chip = NOCHIP;
}

void ResetSPC7110() /* emulate a reset. */
{
	memset(&s7r, 0, sizeof(SPC7110Regs));
	s7r.DataRomOffset = 0x00100000; /* handy constant! */
	s7r.DataRomSize = Memory.CalculatedSize - s7r.DataRomOffset;
	s7r.reg4832 = 1;
	s7r.reg4833 = 2;
	s7r.AlignBy = 1;
	spc7110dec_reset();
}
