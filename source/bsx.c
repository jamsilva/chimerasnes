#include "chisnes.h"
#include "bsx.h"
#include "cpuexec.h"
#include "memmap.h"

#include <stdio.h>
#include <time.h>

#define BIOS_SIZE  0x100000
#define FLASH_SIZE 0x100000
#define PSRAM_SIZE 0x80000

#define BSXPPUBASE 0x2180

static uint32_t FlashSize;
static uint8_t* MapROM;
static uint8_t* FlashROM;

static void BSX_Map_SNES();
static void BSX_Map_LoROM();
static void BSX_Map_HiROM();
static void BSX_Map_MMC();
static void BSX_Map_FlashIO();
static void BSX_Map_SRAM();
static void BSX_Map_PSRAM();
static void BSX_Map_BIOS();
static void BSX_Map_RAM();
static void BSX_Map();
static bool BSX_LoadBIOS();
static void map_psram_mirror_sub(uint32_t bank);
static bool is_bsx(uint8_t* p);

static void stream_close(stream_t* s)
{
	if (s->file != NULL)
		filestream_close(s->file);

	s->file = NULL;
}


static void stream_open(stream_t* s, const char* path)
{
	int64_t strsize;
	s->file = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);

	if (s->file == NULL)
		return;

	strsize = filestream_get_size(s->file);

	if (strsize <= 0)
		goto err;

	s->queue = (strsize + 21) / 22;
	s->first = true;
	s->pf_latch_enable = s->dt_latch_enable = false;
	return;

err:
	filestream_close(s->file);
	s->file = NULL;
}

static uint8_t stream_get(stream_t* s)
{
	int c = filestream_getc(s->file);
	return c == EOF ? 0xFF : (uint8_t) c;
}

static void BSX_Map_SNES()
{
	/* These maps will be partially overwritten */
	int32_t c;

	for (c = 0; c < 0x400; c += 16) /* Banks 00->3F and 80->BF */
	{
		Memory.Map[c + 0] = Memory.Map[c + 0x800] = Memory.RAM;
		Memory.Map[c + 1] = Memory.Map[c + 0x801] = Memory.RAM;
		Memory.BlockIsRAM[c + 0] = Memory.BlockIsRAM[c + 0x800] = true;
		Memory.BlockIsRAM[c + 1] = Memory.BlockIsRAM[c + 0x801] = true;
		Memory.Map[c + 2] = Memory.Map[c + 0x802] = (uint8_t*) MAP_PPU;
		Memory.Map[c + 3] = Memory.Map[c + 0x803] = (uint8_t*) MAP_PPU;
		Memory.Map[c + 4] = Memory.Map[c + 0x804] = (uint8_t*) MAP_CPU;
		Memory.Map[c + 5] = Memory.Map[c + 0x805] = (uint8_t*) MAP_CPU;
		Memory.Map[c + 6] = Memory.Map[c + 0x806] = (uint8_t*) MAP_NONE;
		Memory.Map[c + 7] = Memory.Map[c + 0x807] = (uint8_t*) MAP_NONE;
	}
}

static void BSX_Map_LoROM()
{
	/* These maps will be partially overwritten */
	int32_t i, c;

	for (c = 0; c < 0x400; c += 16) /* Banks 00->3F, 40->7F, 80->BF and C0->FF */
	{
		for (i = c; i < c + 8; i++)
			Memory.Map[i + 0x400] = Memory.Map[i + 0xC00] = &MapROM[(c << 11) % FlashSize];

		for (i = c + 8; i < c + 16; i++)
		{
			Memory.Map[i] = Memory.Map[i + 0x800] = &MapROM[(c << 11) % FlashSize] - 0x8000;
			Memory.BlockIsRAM[i] = Memory.BlockIsRAM[i + 0x800] = BSX.write_enable;
			Memory.BlockIsROM[i] = Memory.BlockIsROM[i + 0x800] = !BSX.write_enable;
			Memory.Map[i + 0x400] = Memory.Map[i + 0xC00] = &MapROM[(c << 11) % FlashSize] - 0x8000;
		}

		for (i = c; i < c + 16; i++)
		{
			Memory.BlockIsRAM[i + 0x400] = Memory.BlockIsRAM[i + 0xC00] = BSX.write_enable;
			Memory.BlockIsROM[i + 0x400] = Memory.BlockIsROM[i + 0xC00] = !BSX.write_enable;
		}
	}
}

static void BSX_Map_HiROM()
{
	/* These maps will be partially overwritten */
	int32_t i, c;

	for (c = 0; c < 0x400; c += 16)
	{
		for (i = c + 8; i < c + 16; i++) /* Banks 00->3F and 80->BF */
		{
			Memory.Map[i] = Memory.Map[i + 0x800] = &MapROM[(c << 12) % FlashSize];
			Memory.BlockIsRAM[i] = Memory.BlockIsRAM[i + 0x800] = BSX.write_enable;
			Memory.BlockIsROM[i] = Memory.BlockIsROM[i + 0x800] = !BSX.write_enable;
		}

		for (i = c; i < c + 16; i++) /* Banks 40->7F and C0->FF */
		{
			Memory.Map[i + 0x400] = Memory.Map[i + 0xC00] = &MapROM[(c << 12) % FlashSize];
			Memory.BlockIsRAM[i + 0x400] = Memory.BlockIsRAM[i + 0xC00] = BSX.write_enable;
			Memory.BlockIsROM[i + 0x400] = Memory.BlockIsROM[i + 0xC00] = !BSX.write_enable;
		}
	}
}

static void BSX_Map_MMC()
{
	int32_t c;

	for (c = 0x010; c < 0x0F0; c += 16) /* Banks 01->0E:5000-5FFF */
	{
		Memory.Map[c + 5] = (uint8_t*) MAP_BSX;
		Memory.BlockIsRAM[c + 5] = Memory.BlockIsROM[c + 5] = false;
	}
}

static void BSX_Map_FlashIO()
{
	int32_t i, c;

	if (!BSX.prevMMC[0x0C])
		return;

	for (c = 0; c < 0x400; c += 16)
	{
		for (i = c + 8; i < c + 16; i++) /* Banks 00->3F and 80->BF */
		{
			Memory.Map[i] = Memory.Map[i + 0x800] = (uint8_t*) MAP_BSX;
			Memory.BlockIsRAM[i] = Memory.BlockIsRAM[i + 0x800] = true;
			Memory.BlockIsROM[i] = Memory.BlockIsROM[i + 0x800] = false;
		}

		for (i = c; i < c + 16; i++) /* Banks 40->7F and C0->FF */
		{
			Memory.Map[i + 0x400] = Memory.Map[i + 0xC00] = (uint8_t*) MAP_BSX;
			Memory.BlockIsRAM[i + 0x400] = Memory.BlockIsRAM[i + 0xC00] = true;
			Memory.BlockIsROM[i + 0x400] = Memory.BlockIsROM[i + 0xC00] = false;
		}
	}
}

static void BSX_Map_SRAM()
{
	int32_t c;

	for (c = 0x100; c < 0x180; c += 16) /* Banks 10->17:5000-5FFF */
	{
		Memory.Map[c + 5] = (uint8_t*) Memory.SRAM + ((c & 0x70) << 8) - 0x5000;
		Memory.BlockIsRAM[c + 5] = true;
		Memory.BlockIsROM[c + 5] = false;
	}
}

static void map_psram_mirror_sub(uint32_t bank)
{
	int32_t i, c;
	bank <<= 4;

	if (BSX.prevMMC[0x02])
	{
		for (c = 0; c < 0x80; c += 16) /* HiROM */
		{
			if ((bank & 0x7F0) >= 0x400)
			{
				for (i = c; i < c + 16; i++)
				{
					Memory.Map[i + bank] = &Memory.PSRAM[(c << 12) % PSRAM_SIZE];
					Memory.BlockIsRAM[i + bank] = true;
					Memory.BlockIsROM[i + bank] = false;
				}
			}
			else
			{
				for (i = c + 8; i < c + 16; i++)
				{
					Memory.Map[i + bank] = &Memory.PSRAM[(c << 12) % PSRAM_SIZE];
					Memory.BlockIsRAM[i + bank] = true;
					Memory.BlockIsROM[i + bank] = false;
				}
			}
		}
	}
	else
	{
		for (c = 0; c < 0x100; c += 16) /* LoROM */
		{
			if ((bank & 0x7F0) >= 0x400)
			{
				for (i = c; i < c + 8; i++)
				{
					Memory.Map[i + bank] = &Memory.PSRAM[(c << 11) % PSRAM_SIZE];
					Memory.BlockIsRAM[i + bank] = true;
					Memory.BlockIsROM[i + bank] = false;
				}
			}

			for (i = c + 8; i < c + 16; i++)
			{
				Memory.Map[i + bank] = &Memory.PSRAM[(c << 11) % PSRAM_SIZE] - 0x8000;
				Memory.BlockIsRAM[i + bank] = true;
				Memory.BlockIsROM[i + bank] = false;
			}
		}
	}
}

static void BSX_Map_PSRAM(void)
{
	int32_t c;

	if (BSX.prevMMC[0x02])  /* HiROM Mode */
	{
		if (!BSX.prevMMC[0x05] && !BSX.prevMMC[0x06]) /* Map Memory.PSRAM to 00-07/40-47 / 80-87/C0-C7 */
		{
			if (BSX.prevMMC[0x03])
			{
				map_psram_mirror_sub(0x00);
				map_psram_mirror_sub(0x40);
			}

			if (BSX.prevMMC[0x04])
			{
				map_psram_mirror_sub(0x80);
				map_psram_mirror_sub(0xC0);
			}
		}
		else if (BSX.prevMMC[0x05] && !BSX.prevMMC[0x06]) /* Map Memory.PSRAM to 10-17/50-57 / 90-97-D0-D7 */
		{
			if (BSX.prevMMC[0x03])
			{
				map_psram_mirror_sub(0x10);
				map_psram_mirror_sub(0x50);
			}

			if (BSX.prevMMC[0x04])
			{
				map_psram_mirror_sub(0x90);
				map_psram_mirror_sub(0xD0);
			}
		}
		else if (!BSX.prevMMC[0x05] && BSX.prevMMC[0x06]) /* Map Memory.PSRAM to 20-27/60-67 / A0-A7/E0-E7 */
		{
			if (BSX.prevMMC[0x03])
			{
				map_psram_mirror_sub(0x20);
				map_psram_mirror_sub(0x60);
			}

			if (BSX.prevMMC[0x04])
			{
				map_psram_mirror_sub(0xA0);
				map_psram_mirror_sub(0xE0);
			}
		}
		else /* Map Memory.PSRAM to 30-37/70-77 / B0-B7/F0-F7 */
		{
			if (BSX.prevMMC[0x03])
			{
				map_psram_mirror_sub(0x30);
				map_psram_mirror_sub(0x70);
			}

			if (BSX.prevMMC[0x04])
			{
				map_psram_mirror_sub(0xB0);
				map_psram_mirror_sub(0xF0);
			}
		}

		if (BSX.prevMMC[0x03]) /* Map Memory.PSRAM to 20->3F:6000-7FFF */
		{
			for (c = 0x200; c < 0x400; c += 16)
			{
				Memory.Map[c + 6] = &Memory.PSRAM[((c & 0x70) << 12) % PSRAM_SIZE];
				Memory.Map[c + 7] = &Memory.PSRAM[((c & 0x70) << 12) % PSRAM_SIZE];
				Memory.BlockIsRAM[c + 6] = true;
				Memory.BlockIsRAM[c + 7] = true;
				Memory.BlockIsROM[c + 6] = false;
				Memory.BlockIsROM[c + 7] = false;
			}
		}

		if (BSX.prevMMC[0x04]) /* Map Memory.PSRAM to A0->BF:6000-7FFF */
		{
			for (c = 0xA00; c < 0xC00; c += 16)
			{
				Memory.Map[c + 6] = &Memory.PSRAM[((c & 0x70) << 12) % PSRAM_SIZE];
				Memory.Map[c + 7] = &Memory.PSRAM[((c & 0x70) << 12) % PSRAM_SIZE];
				Memory.BlockIsRAM[c + 6] = true;
				Memory.BlockIsRAM[c + 7] = true;
				Memory.BlockIsROM[c + 6] = false;
				Memory.BlockIsROM[c + 7] = false;
			}
		}
	}
	else /* LoROM mode */
	{
		if (!BSX.prevMMC[0x05] && !BSX.prevMMC[0x06]) /* Map Memory.PSRAM to 00-0F/80-8F */
		{
			if (BSX.prevMMC[0x03])
				map_psram_mirror_sub(0x00);

			if (BSX.prevMMC[0x04])
				map_psram_mirror_sub(0x80);
		}
		else if (BSX.prevMMC[0x05] && !BSX.prevMMC[0x06]) /* Map Memory.PSRAM to 20-2F/A0-AF */
		{
			if (BSX.prevMMC[0x03])
				map_psram_mirror_sub(0x20);

			if (BSX.prevMMC[0x04])
				map_psram_mirror_sub(0xA0);
		}
		else if (!BSX.prevMMC[0x05] && BSX.prevMMC[0x06]) /* Map Memory.PSRAM to 40-4F/C0-CF */
		{
			if (BSX.prevMMC[0x03])
				map_psram_mirror_sub(0x40);

			if (BSX.prevMMC[0x04])
				map_psram_mirror_sub(0xC0);
		}
		else /* Map Memory.PSRAM to 60-6F/E0-EF */
		{
			if (BSX.prevMMC[0x03])
				map_psram_mirror_sub(0x60);

			if (BSX.prevMMC[0x04])
				map_psram_mirror_sub(0xE0);
		}

		/* Map Memory.PSRAM to 70-7D/F0-FF */
		if (BSX.prevMMC[0x03])
			map_psram_mirror_sub(0x70);

		if (BSX.prevMMC[0x04])
			map_psram_mirror_sub(0xF0);
	}
}

static void BSX_Map_BIOS()
{
	int32_t i, c;

	if (BSX.prevMMC[0x07]) /* Banks 00->1F:8000-FFFF */
	{
		for (c = 0; c < 0x200; c += 16)
		{
			for (i = c + 8; i < c + 16; i++)
			{
				Memory.Map[i] = &Memory.BIOSROM[(c << 11) % BIOS_SIZE] - 0x8000;
				Memory.BlockIsRAM[i] = false;
				Memory.BlockIsROM[i] = true;
			}
		}
	}

	if (BSX.prevMMC[0x08]) /* Banks 80->9F:8000-FFFF */
	{
		for (c = 0; c < 0x200; c += 16)
		{
			for (i = c + 8; i < c + 16; i++)
			{
				Memory.Map[i + 0x800] = &Memory.BIOSROM[(c << 11) % BIOS_SIZE] - 0x8000;
				Memory.BlockIsRAM[i + 0x800] = false;
				Memory.BlockIsROM[i + 0x800] = true;
			}
		}
	}
}

static void BSX_Map_RAM()
{
	int32_t c;

	for (c = 0; c < 16; c++) /* Banks 7E->7F */
	{
		Memory.Map[c + 0x7E0] = Memory.RAM;
		Memory.Map[c + 0x7F0] = Memory.RAM + 0x10000;
		Memory.BlockIsRAM[c + 0x7E0] = true;
		Memory.BlockIsRAM[c + 0x7F0] = true;
		Memory.BlockIsROM[c + 0x7E0] = false;
		Memory.BlockIsROM[c + 0x7F0] = false;
	}
}

static void BSX_Map()
{
	memcpy(BSX.prevMMC, BSX.MMC, sizeof(BSX.MMC));
	MapROM = FlashROM;
	FlashSize = FLASH_SIZE;

	if (BSX.prevMMC[0x02])
		BSX_Map_HiROM();
	else
		BSX_Map_LoROM();

	BSX_Map_FlashIO();
	BSX_Map_PSRAM();
	BSX_Map_SNES();
	BSX_Map_SRAM();
	BSX_Map_RAM();
	BSX_Map_BIOS();
	BSX_Map_MMC();

	/* Monitor new register changes */
	BSX.dirty  = false;
	BSX.dirty2 = false;

	map_WriteProtectROM();
}

static uint8_t BSX_Get_Bypass_FlashIO(uint32_t offset)
{
	/* For games other than BS-X */
	FlashROM = Memory.ROM;

	if (BSX.prevMMC[0x02])
		return (FlashROM[offset & 0x0FFFFF]);
	else
		return (FlashROM[(offset & 0x1F0000) >> 1 | (offset & 0x7FFF)]);
}

static void BSX_Set_Bypass_FlashIO(uint32_t offset, uint8_t byte)
{
	/* For games other than BS-X */
	FlashROM = Memory.ROM;

	if (BSX.prevMMC[0x02])
		FlashROM[offset & 0x0FFFFF] = FlashROM[offset & 0x0FFFFF] & byte;
	else
		FlashROM[(offset & 0x1F0000) >> 1 | (offset & 0x7FFF)] = FlashROM[(offset & 0x1F0000) >> 1 | (offset & 0x7FFF)] & byte;
}

uint8_t GetBSX(uint32_t address)
{
	uint8_t  bank = (address >> 16) & 0xFF;
	uint16_t offset = address & 0xFFFF;

	if ((bank >= 0x01 && bank <= 0x0E) && ((address & 0xF000) == 0x5000)) /* MMC */
		return BSX.MMC[bank];

	/* Flash Mapping */
	switch (offset) /* note: may be more registers, purposes unknown */
	{
		case 0x0002:
		case 0x8002:
			if (BSX.flash_bsr) /* Page Status Register */
				return 0xC0;

			break;
		case 0x0004:
		case 0x8004:
			if (BSX.flash_gsr) /* Global Status Register */
				return 0x82;

			break;
		case 0xFF00:
		case 0xFF02:
		case 0xFF04:
		case 0xFF06:
		case 0xFF08:
		case 0xFF0A:
		case 0xFF0C:
		case 0xFF0E:
		case 0xFF10:
		case 0xFF12: /* return flash vendor information */
			if (!BSX.read_enable)
				break;

			switch (offset & 0xFF)
			{
				case 0: /* first vendor byte */
					return 0x4D;
				case 2: /* third vendor byte */
					return 0x50;
				case 6: /* first flash size byte - 2MB */
					return 0x1A;
				default:
					return 0x00;
			}

			break;
		default:
			break;
	}

	if (BSX.flash_csr) /* Compatible Status Register */
	{
		BSX.flash_csr = false;
		return 0x80;
	}

	return BSX_Get_Bypass_FlashIO(address); /* default: read-through mode */
}

void SetBSX(uint8_t byte, uint32_t address)
{
	uint32_t x;
	uint8_t  bank = (address >> 16) & 0xFF;

	if ((bank >= 0x01 && bank <= 0x0E) && ((address & 0xF000) == 0x5000)) /* MMC */
	{
		if (bank == 0x0E && BSX.dirty) /* Avoid updating the memory map when it is not needed */
		{
			BSX_Map();
			BSX.dirty = false;
		}
		else if (bank != 0x0E && BSX.MMC[bank] != byte)
		{
			BSX.dirty = true;
		}

		BSX.MMC[bank] = byte;
	}

	/* Flash IO */
	if (BSX.write_enable) /* Write to Flash */
	{
		BSX_Set_Bypass_FlashIO(address, byte);
		BSX.write_enable = false;
		return;
	}

	/* Flash Command Handling */
	/* Memory Pack Type 1 & 3 & 4 */
	BSX.flash_command <<= 8;
	BSX.flash_command |= byte;

	switch (BSX.flash_command & 0xFF)
	{
		case 0x00:
		case 0xFF: /* Reset to normal */
			BSX.flash_bsr = false;
			BSX.flash_csr = false;
			BSX.flash_gsr = false;
			BSX.read_enable = false;
			BSX.write_enable = false;
			break;
		case 0x10:
		case 0x40: /* Write Byte */
			BSX.flash_bsr = false;
			BSX.flash_csr = true;
			BSX.flash_gsr = false;
			BSX.read_enable = false;
			BSX.write_enable = true;
			break;
		case 0x50: /* Clear Status Register */
			BSX.flash_bsr = false;
			BSX.flash_csr = false;
			BSX.flash_gsr = false;
			break;
		case 0x70: /* Read CSR */
			BSX.flash_bsr = false;
			BSX.flash_csr = true;
			BSX.flash_gsr = false;
			BSX.read_enable = false;
			BSX.write_enable = false;
			break;
		case 0x71: /* Read Extended Status Registers (Page and Global) */
			BSX.flash_bsr = true;
			BSX.flash_csr = false;
			BSX.flash_gsr = true;
			BSX.read_enable = false;
			BSX.write_enable = false;
			break;
		case 0x75: /* Show Page Buffer / Vendor Info */
			BSX.flash_csr = false;
			BSX.read_enable = true;
			break;
		case 0xD0: /* DO COMMAND */
			switch (BSX.flash_command & 0xFFFF)
			{
				case 0x20D0: /* Block Erase */
					for (x = 0; x < 0x10000; x++)
					{
						if (BSX.MMC[0x02])
							FlashROM[(address & 0x0F0000) + x] = 0xFF;
						else
							FlashROM[((address & 0x1E0000) >> 1) + x] = 0xFF;
					}

					break;
				case 0xA7D0: /* Chip Erase */
					for (x = 0; x < FLASH_SIZE; x++)
						FlashROM[x] = 0xFF;

					break;
				case 0x38D0: /* Flashcart Reset */
				default:
					break;
			}

			break;
		default:
			break;
	}
}

static void BSXSetStream(uint8_t count, stream_t* stream, uint32_t firstreg)
{
	char     path[PATH_MAX + 1];
	char*    pathp = path;
	char*    directory = ""; //GetDirectory(SAT_DIR);
	uint32_t dirlen = strlen(directory);
	uint32_t slashlen = strlen(SLASH_STR);
	stream_close(stream);
	memcpy(pathp, directory, dirlen);
	pathp += dirlen;
	memcpy(pathp, SLASH_STR, slashlen);
	pathp += slashlen;
	pathp += snprintf(pathp, sizeof(path) - dirlen - slashlen, "BSX%04X-%d.bin", (BSX.PPU[firstreg - BSXPPUBASE] | (BSX.PPU[firstreg + 1 - BSXPPUBASE] * 256)), count); /* BSXHHHH-DDD.bin */
	stream_open(stream, path);

	if (stream->file)
		BSX.PPU[firstreg + 5 - BSXPPUBASE] = 0;
}

uint8_t BSXGetRTC() /* Get Time */
{
	static time_t t;
	struct tm* tmr;
	uint8_t index = BSX.out_index;
	BSX.out_index++;

	if (BSX.out_index > 22)
		BSX.out_index = 0;

	switch (index)
	{
		case 0:
		case 1:
		case 2:
		case 3:
		case 7:
		case 8:
		case 9:
			return 0x00;
		case 4:
		case 18:
		case 19:
		case 20:
		case 21:
		case 22:
			return 0x10;
		case 5:
		case 6:
			return 0x01;
		default:
			break;
	}

	if (index == 10)
		time(&t);

	tmr = localtime(&t);

	switch (index)
	{
		case 10:
			return tmr->tm_sec;
		case 11:
			return tmr->tm_min;
		case 12:
			return tmr->tm_hour;
		case 13:
			return tmr->tm_wday + 1;
		case 14:
			return tmr->tm_mday;
		case 15:
			return tmr->tm_mon + 1;
		case 16:
			return (uint8_t) (tmr->tm_year + 1900);
		case 17:
			return (tmr->tm_year + 1900) >> 8;
		default:
			return 0x00; /* unreachable */
	}
}

uint8_t GetBSXPPU(uint16_t address)
{
	uint8_t   t;
	stream_t* s = (address > 0x218D) ? &BSX.sat_stream2 : &BSX.sat_stream1;

	switch(address) /* known read registers */
	{
		case 0x218A: /* Stream 1 - Prefix Count (R) */
		case 0x2190: /* Stream 2 */
			if (!s->pf_latch_enable || !s->dt_latch_enable)
				return 0;

			if (BSX.PPU[address - 2 - BSXPPUBASE] == 0 && BSX.PPU[address - 1 - BSXPPUBASE] == 0)
				return 1;

			if (s->queue <= 0)
			{
				s->count++;
				BSXSetStream(s->count - 1, s, address - 2);
			}

			if (!s->file && (s->count - 1) > 0)
			{
				s->count = 1;
				BSXSetStream(s->count - 1, s, address - 2);
			}

			if (s->file)
			{
				if (s->queue >= 128) /* Lock at 0x7F for bigger packets */
					BSX.PPU[address - BSXPPUBASE] = 0x7F;
				else
					BSX.PPU[address - BSXPPUBASE] = s->queue;

				return BSX.PPU[address - BSXPPUBASE];
			}

			return 0;
		case 0x218B: /* Stream 1 - Prefix Latch (R/W) */
		case 0x2191: /* Stream 2 */
			if (!s->pf_latch_enable)
				return 0;

			if (BSX.PPU[address - 3 - BSXPPUBASE] == 0 && BSX.PPU[address - 2 - BSXPPUBASE] == 0)
				BSX.PPU[address - BSXPPUBASE] = 0x90;

			if (s->file)
			{
				t = 0;

				if (s->first) /* First packet */
				{
					t |= 0x10;
					s->first = false;
				}

				if (--s->queue == 0) /* Last packet */
					t |= 0x80;

				BSX.PPU[address - BSXPPUBASE] = t;
			}

			BSX.PPU[address + 2 - BSXPPUBASE] |= BSX.PPU[address - BSXPPUBASE];
			return BSX.PPU[address - BSXPPUBASE];
		case 0x218C: /* Stream 1 - Data Latch (R/W) */
		case 0x2192: /* Stream 2 */
			if (!s->dt_latch_enable)
				return 0;

			if (BSX.PPU[address - 4 - BSXPPUBASE] == 0 && BSX.PPU[address - 3 - BSXPPUBASE] == 0)
				BSX.PPU[address - BSXPPUBASE] = BSXGetRTC();
			else if (s->file)
				BSX.PPU[address - BSXPPUBASE] = stream_get(s);

			return BSX.PPU[address - BSXPPUBASE];
		case 0x218D: /* Stream 1 - OR gate (R) */
		case 0x2193: /* Stream 2 */
			t = BSX.PPU[address - BSXPPUBASE];
			BSX.PPU[address - BSXPPUBASE] = 0;
			return t;
		case 0x2188: /* Stream 1 - Logical Channel 1 + Data Structure (R/W) */
		case 0x218E: /* Stream 2 */
		case 0x2189: /* Stream 1 - Logical Channel 2 (R/W) [6bit] */
		case 0x218F: /* Stream 2 */
		case 0x2194: /* Satellaview LED / Stream Enable (R/W) [4bit] */
		case 0x2195: /* Unknown */
		case 0x2196: /* Satellaview Status (R) */
		case 0x2197: /* Soundlink Settings (R/W) */
		case 0x2198: /* Serial I/O - Serial Number (R/W) */
		case 0x2199: /* Serial I/O - Unknown (R/W) */
			return BSX.PPU[address - BSXPPUBASE];
		default:
			return ICPU.OpenBus;
	}
}

void SetBSXPPU(uint8_t byte, uint16_t address)
{
	stream_t* s = (address > 0x218D) ? &BSX.sat_stream2 : &BSX.sat_stream1;

	switch (address) /* known write registers */
	{
		case 0x2189: /* Stream 1 - Logical Channel 2 (R/W) [6bit] */
		case 0x218F: /* Stream 2 - Logical Channel 2 (R/W) [6bit]  */
			byte &= 0x3F;
			/* fall through */
		case 0x2188: /* Stream 1 - Logical Channel 1 + Data Structure (R/W) */
		case 0x218E: /* Stream 2 */
			if (BSX.PPU[address - BSXPPUBASE] == byte)
				s->count = 0;

			BSX.PPU[address - BSXPPUBASE] = byte;
			break;
		case 0x218B: /* Stream 1 - Prefix Latch (R/W) */
		case 0x2191: /* Stream 2 */
			s->pf_latch_enable = (bool) byte;
			break;
		case 0x218C: /* Stream 1 - Data Latch (R/W) */
		case 0x2192: /* Stream 2 */
			if (BSX.PPU[address - 4 - BSXPPUBASE] == 0 && BSX.PPU[address - 3 - BSXPPUBASE] == 0)
				BSX.out_index = 0;

			s->dt_latch_enable = (bool) byte;
			break;
		case 0x2194: /* Satellaview LED / Stream Enable (R/W) [4bit] */
			byte &= 0x0F;
			/* fall through */
		case 0x2197: /* Soundlink Settings (R/W) */
			BSX.PPU[address - BSXPPUBASE] = byte;
			break;
	}
}

uint8_t* GetBasePointerBSX(uint32_t address)
{
	return MapROM;
}

static bool BSX_LoadBIOS()
{
	RFILE* fp;
	size_t size;
	char path[PATH_MAX + 1];
	char* pathp = path;
	const char* dir = GetBIOSDir();
	printf("%s\n", dir);
	uint32_t dirlen = strlen(dir);
	uint32_t slashlen = strlen(SLASH_STR);
	memcpy(pathp, dir, dirlen);
	pathp += dirlen;
	memcpy(pathp, SLASH_STR, slashlen);
	pathp += slashlen;
	strcpy(pathp, "BS-X.bin");
	fp = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);

	if (!fp)
	{
		strcpy(pathp, "BS-X.bios");
		fp = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
	}

	if (!fp)
		return false;

	size = filestream_read(fp, (void *) Memory.BIOSROM, BIOS_SIZE);
	filestream_close(fp);
	return size == BIOS_SIZE;
}

static bool is_BSX_BIOS(const uint8_t* data, uint32_t size)
{
	return size == BIOS_SIZE && !strncmp((char*) (data + 0x7FC0), "Satellaview BS-X     ", 21);
}

void InitBSX()
{
	if (is_BSX_BIOS(Memory.ROM, Memory.CalculatedSize)) /* BS-X itself */
	{
		Settings.Chip = BSFW;
		Memory.LoROM = true;
		memmove(Memory.BIOSROM, Memory.ROM, BIOS_SIZE);
		BSX.flash_mode = false;
		FlashSize = FLASH_SIZE;
	}
	else
	{
		uint8_t* header;
		int32_t r1 = is_bsx(Memory.ROM + 0x7FC0);
		int32_t r2 = is_bsx(Memory.ROM + 0xFFC0);

		if (!(r1 | r2)) /* BS games */
			return;

		header = Memory.ROM + (r1 ? 0x7FC0 : 0xFFC0);
		Settings.Chip = BS;
		Memory.LoROM = r1;
		BSX.flash_mode = !((header[0x18] & 0xEF) == 0x20);
		FlashSize = (header[0x19] & 0x20) ? PSRAM_SIZE : FLASH_SIZE;

		/* Fix Block Allocation Flags
		 * (for games that don't have it setup properly,
		 * for example when taken seperately from the upper memory of the Memory Pack,
		 * else the game will error out on BS-X) */
		for (; ((header[0x10] & 1) == 0) && header[0x10] != 0; header[0x10] >>= 1);

		if (!BSX_LoadBIOS() && !is_BSX_BIOS(Memory.BIOSROM, BIOS_SIZE))
			memset(Memory.BIOSROM, 0, BIOS_SIZE);
	}

	MapROM = NULL;
	FlashROM = Memory.ROM;
	memset(&BSX.sat_stream1, 0, sizeof(BSX.sat_stream1));
	memset(&BSX.sat_stream2, 0, sizeof(BSX.sat_stream2));
}

void ResetBSX()
{
	if (Settings.Chip == BSFW)
		memset(Memory.ROM, 0, FLASH_SIZE);

	memset(BSX.PPU,     0, sizeof(BSX.PPU));
	memset(BSX.MMC,     0, sizeof(BSX.MMC));
	memset(BSX.prevMMC, 0, sizeof(BSX.prevMMC));
	BSX.dirty         = false;
	BSX.dirty2        = false;
	BSX.write_enable  = false;
	BSX.read_enable   = false;
	BSX.flash_command = 0;
	BSX.old_write     = 0;
	BSX.new_write     = 0;
	BSX.out_index     = 0;

	if(Memory.BIOSROM[0]) /* starting from the bios */
	{
		BSX.MMC[0x02] = BSX.MMC[0x07] = BSX.MMC[0x08] = 0x80;
		BSX.MMC[0x03] = BSX.MMC[0x05] = BSX.MMC[0x06] = BSX.MMC[0x09] = BSX.MMC[0x0B] = BSX.MMC[0x0E] = 0x80;
	}
	else
	{
		BSX.MMC[0x02] = BSX.flash_mode ? 0x80: 0;

		if (FlashSize == PSRAM_SIZE) /* per bios: run from psram or flash card */
		{
			memcpy(Memory.PSRAM, FlashROM, PSRAM_SIZE);
			BSX.MMC[0x01] = BSX.MMC[0x03] = BSX.MMC[0x04] = BSX.MMC[0x0C] = BSX.MMC[0x0D] = BSX.MMC[0x0E] = 0x80;
		}
		else
		{
			BSX.MMC[0x03] = BSX.MMC[0x05] = BSX.MMC[0x06] = BSX.MMC[0x09] = BSX.MMC[0x0B] = BSX.MMC[0x0E] = 0x80;
		}
	}

	/* default register values */
	BSX.PPU[0x2196 - BSXPPUBASE] = 0x10;
	BSX.PPU[0x2197 - BSXPPUBASE] = 0x80;

	/* stream reset */
	stream_close(&BSX.sat_stream1);
	stream_close(&BSX.sat_stream2);

	BSX_Map();
}

void BSXPostLoadState()
{
	uint8_t temp[16];
	bool pd1 = BSX.dirty;
	bool pd2 = BSX.dirty2;
	memcpy(temp,    BSX.MMC,     sizeof(BSX.MMC));
	memcpy(BSX.MMC, BSX.prevMMC, sizeof(BSX.MMC));
	BSX_Map();
	memcpy(BSX.MMC, temp,        sizeof(BSX.MMC));
	BSX.dirty  = pd1;
	BSX.dirty2 = pd2;
}

static bool valid_normal_bank(uint8_t bankbyte)
{
	switch (bankbyte)
	{
		case 32:
		case 33:
		case 48:
		case 49:
			return true;
		default:
			return false;
	}
}

static bool is_bsx(uint8_t* p)
{
	if (p[26] != 0x33 && p[26] != 0xFF)
		return false;

	if (p[21] && (p[21] & 131) != 128)
		return false;

	if (!valid_normal_bank(p[24]))
		return false;

	if (!p[22] && !p[23])
		return false;

	if (p[22] == 0xFF && p[23] == 0xFF)
		return true;

	if (!(p[22] & 0xF) && ((p[22] >> 4) - 1 < 12))
		return true;

	return false;
}
