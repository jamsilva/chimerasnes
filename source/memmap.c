#include <stdio.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <ctype.h>

#ifdef __W32_HEAP
	#include <malloc.h>
#endif

#include <streams/file_stream.h>

#include "chisnes.h"
#include "memmap.h"
#include "cpuexec.h"
#include "ppu.h"
#include "display.h"
#include "cheats.h"
#include "apu.h"
#include "sa1.h"
#include "dsp.h"
#include "fxemu.h"
#include "srtc.h"
#include "sdd1.h"
#include "spc7110.h"
#include "seta.h"
#include "math.h"

#ifdef _MSC_VER
	/* Necessary to build on MSVC */
	#define strnicmp _strnicmp
#endif

static void DeinterleaveType1(int32_t size, uint8_t* base)
{
	int32_t i, j;
	uint8_t blocks[256];
	uint8_t* tmp = NULL;
	int32_t nblocks = size >> 15;

	for (i = 0, j = 0; i < nblocks; i += 2, j++)
	{
		blocks[i] = j + nblocks;
		blocks[i + 1] = j;
	}

	tmp = (uint8_t*) malloc(0x8000);

	if (tmp == NULL)
		return;

	for (i = 0; i < nblocks; i++)
	{
		for (j = i; j < nblocks; j++)
		{
			uint8_t b;

			if (blocks[j] != i)
				continue;

			/* memmove converted: Different mallocs [Neb] */
			memcpy(tmp, &base[blocks[j] * 0x8000], 0x8000);
			/* memmove converted: Different addresses, or identical for blocks[i] == blocks[j] [Neb] */
			memcpy(&base[blocks[j] * 0x8000], &base[blocks[i] * 0x8000], 0x8000);
			/* memmove converted: Different mallocs [Neb] */
			memcpy(&base[blocks[i] * 0x8000], tmp, 0x8000);
			b = blocks[j];
			blocks[j] = blocks[i];
			blocks[i] = b;
			break;
		}
	}

	free(tmp);
}

static void DeinterleaveGD24(int32_t size, uint8_t* base)
{
	uint8_t* tmp = NULL;

	if (size != 0x300000)
		return;

	tmp = (uint8_t*) malloc(0x80000);

	if (!tmp)
		return;

	/* memmove converted: Different mallocs [Neb] */
	memcpy(tmp, &base[0x180000], 0x80000);
	/* memmove converted: Different addresses [Neb] */
	memcpy(&base[0x180000], &base[0x200000], 0x80000);
	/* memmove converted: Different addresses [Neb] */
	memcpy(&base[0x200000], &base[0x280000], 0x80000);
	/* memmove converted: Different mallocs [Neb] */
	memcpy(&base[0x280000], tmp, 0x80000);
	free(tmp);
	DeinterleaveType1(size, base);
}

static bool AllASCII(uint8_t* b, int32_t size)
{
	int32_t i;

	for (i = 0; i < size; i++)
		if (b[i] < 32 || b[i] > 126)
			return false;

	return true;
}

static int32_t ScoreHiROM(bool skip_header, int32_t romoff)
{
	int32_t score = 0;
	int32_t o     = romoff + 0xff00 + (skip_header ?  0x200 : 0);

	/* Check for extended HiROM expansion used in Mother 2 Deluxe et al.*/
	/* Looks for size byte 13 (8MB) and an actual ROM size greater than 4MB */
	if (Memory.ROM[o + 0xd7] == 13 && Memory.CalculatedSize > 1024 * 1024 * 4)
		score += 5;

	if (Memory.ROM[o + 0xd5] & 0x1)
		score += 2;

	/* Mode23 is SA-1 */
	if (Memory.ROM[o + 0xd5] == 0x23)
		score -= 2;

	if (Memory.ROM[o + 0xd4] == 0x20)
		score += 2;

	if ((Memory.ROM[o + 0xdc] + (Memory.ROM[o + 0xdd] << 8) + Memory.ROM[o + 0xde] + (Memory.ROM[o + 0xdf] << 8)) == 0xffff)
	{
		score += 2;

		if (Memory.ROM[o + 0xde] + (Memory.ROM[o + 0xdf] << 8))
			score++;
	}

	if (Memory.ROM[o + 0xda] == 0x33)
		score += 2;

	if ((Memory.ROM[o + 0xd5] & 0xf) < 4)
		score += 2;

	if (!(Memory.ROM[o + 0xfd] & 0x80))
		score -= 6;

	if ((Memory.ROM[o + 0xfc] | (Memory.ROM[o + 0xfd] << 8)) > 0xffb0)
		score -= 2; /* reduced after looking at a scan by Cowering */

	if (Memory.CalculatedSize > 1024 * 1024 * 3)
		score += 4;

	if ((1 << (Memory.ROM[o + 0xd7] - 7)) > 48)
		score -= 1;

	if (!AllASCII(&Memory.ROM[o + 0xb0], 6))
		score -= 1;

	if (!AllASCII(&Memory.ROM[o + 0xc0], ROM_NAME_LEN - 1))
		score -= 1;

	return score;
}

static int32_t ScoreLoROM(bool skip_header, int32_t romoff)
{
	int32_t score = 0;
	int32_t o = romoff + 0x7f00 + (skip_header ? 0x200 : 0);

	if (!(Memory.ROM[o + 0xd5] & 0x1))
		score += 3;

	if (Memory.ROM[o + 0xd5] == 0x23) /* Mode23 is SA-1 */
		score += 2;

	if ((Memory.ROM[o + 0xdc] + (Memory.ROM[o + 0xdd] << 8) + Memory.ROM[o + 0xde] + (Memory.ROM[o + 0xdf] << 8)) == 0xffff)
	{
		score += 2;

		if (0 != (Memory.ROM[o + 0xde] + (Memory.ROM[o + 0xdf] << 8)))
			score++;
	}

	if (Memory.ROM[o + 0xda] == 0x33)
		score += 2;

	if ((Memory.ROM[o + 0xd5] & 0xf) < 4)
		score += 2;

	if (!(Memory.ROM[o + 0xfd] & 0x80))
		score -= 6;

	if ((Memory.ROM[o + 0xfc] | (Memory.ROM[o + 0xfd] << 8)) > 0xffb0)
		score -= 2; /* reduced per Cowering suggestion */

	if (Memory.CalculatedSize <= 1024 * 1024 * 16)
		score += 2;

	if ((1 << (Memory.ROM[o + 0xd7] - 7)) > 48)
		score -= 1;

	if (!AllASCII(&Memory.ROM[o + 0xb0], 6))
		score -= 1;

	if (!AllASCII(&Memory.ROM[o + 0xc0], ROM_NAME_LEN - 1))
		score -= 1;

	return score;
}

static char* Safe(const char* s)
{
	static char* safe;
	static int32_t safe_len = 0;
	int32_t i, len;

	if (s == NULL)
	{
		free(safe);
		safe = NULL;
		return NULL;
	}

	len = strlen(s);

	if (!safe || len + 1 > safe_len)
	{
		if (safe)
			free(safe);

		safe = (char*) malloc(safe_len = len + 1);
	}

	for (i = 0; i < len; i++)
	{
		if (s[i] >= 32 && s[i] < 127)
			safe[i] = s[i];
		else
			safe[i] = '?';
	}

	safe[len] = 0;
	return safe;
}

bool InitMemory()
{
	Memory.RAM   = (uint8_t*) calloc(0x20000, sizeof(uint8_t));
	Memory.SRAM  = (uint8_t*) calloc(0x20000, sizeof(uint8_t));
	Memory.VRAM  = (uint8_t*) calloc(0x10000, sizeof(uint8_t));
	Memory.ROM   = (uint8_t*) malloc((MAX_ROM_SIZE + 0x200 + 0x8000) * sizeof(uint8_t)); /* Don't bother initializing ROM, we will load a game anyway. */
	Memory.FillRAM = Memory.ROM;                                                         /* FillRAM uses first 32K of ROM image area, otherwise space just wasted. Might be read by the SuperFX code. */
	Memory.CX4RAM  = Memory.FillRAM + 0x400000 + 8192 * 8;                               /* CX4 */
	Memory.OBC1RAM = Memory.FillRAM + 0x6000;                                            /* OBC1 */
	Memory.PSRAM   = Memory.FillRAM + 0x400000;                                          /* BS-X */
	IPPU.TileCache[TILE_2BIT]  = (uint8_t*) calloc(MAX_2BIT_TILES, 128);
	IPPU.TileCache[TILE_4BIT]  = (uint8_t*) calloc(MAX_4BIT_TILES, 128);
	IPPU.TileCache[TILE_8BIT]  = (uint8_t*) calloc(MAX_8BIT_TILES, 128);
	IPPU.TileCached[TILE_2BIT] = (uint8_t*) calloc(MAX_2BIT_TILES, 1);
	IPPU.TileCached[TILE_4BIT] = (uint8_t*) calloc(MAX_4BIT_TILES, 1);
	IPPU.TileCached[TILE_8BIT] = (uint8_t*) calloc(MAX_8BIT_TILES, 1);

	if (Memory.ROM) /* Add 0x8000 to ROM image pointer to stop SuperFX code accessing unallocated memory (can cause crash on some ports). */
		Memory.ROM += 0x8000;

	if (!Memory.RAM || !Memory.SRAM || !Memory.VRAM || !Memory.ROM || !Memory.PSRAM || !IPPU.TileCache[TILE_2BIT] || !IPPU.TileCache[TILE_4BIT] || !IPPU.TileCache[TILE_8BIT] || !IPPU.TileCached[TILE_2BIT] || !IPPU.TileCached[TILE_4BIT] || !IPPU.TileCached[TILE_8BIT])
	{
		DeinitMemory();
		return false;
	}

	SuperFX.pvRegisters = &Memory.FillRAM[0x3000];
	SuperFX.nRamBanks = 2; /* Most only use 1.  1 = 64KB, 2 = 128KB = 1024Mb */
	SuperFX.pvRam = Memory.SRAM;
	SuperFX.nRomBanks = (2 * 1024 * 1024) / (32 * 1024);
	SuperFX.pvRom = (uint8_t*) Memory.ROM;
	return true;
}

void DeinitMemory()
{
	if (Memory.ROM)
		Memory.ROM -= 0x8000;

	free(Memory.RAM);
	free(Memory.SRAM);
	free(Memory.VRAM);
	free(Memory.ROM);
	Memory.RAM = Memory.SRAM = Memory.VRAM = Memory.ROM = Memory.PSRAM = NULL;

	for (int32_t t = 0; t < 2; t++)
	{
		free(IPPU.TileCache[t]);
		free(IPPU.TileCached[t]);
		IPPU.TileCache[t] = IPPU.TileCached[t] = NULL;
	}

	Safe(NULL);
}

static void LoadSFTBIOS()
{
	RFILE*      fp;
	char        path[PATH_MAX + 1];
	const char* dir = GetBIOSDir();
	strcpy(path, dir);
	strcat(path, SLASH_STR);
	strcat(path, "STBIOS.bin");
	fp = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);

	if (!fp)
		return;

	filestream_read(fp, Memory.ROM, 0x40000);
	filestream_close(fp);
}

static char* ROMName(char* buf)
{
	return buf + sprintf(buf, "\"%s\"", Memory.ROMName);
}

static char* ROMId(char* buf)
{
	return buf + sprintf(buf, "(%s)", Memory.ROMId);
}

static char* CompanyName(char* buf)
{
	static const char* nintendo_licensees[] =
	{
		[0x000] = "Unlicensed",
		[0x001] = "Nintendo",
		[0x002] = "Rocket Games/Ajinomoto",
		[0x003] = "Imagineer-Zoom",
		[0x004] = "Gray Matter",
		[0x005] = "Zamuse",
		[0x006] = "Falcom",
		[0x008] = "Capcom",
		[0x009] = "Hot B Co.",
		[0x00A] = "Jaleco",
		[0x00B] = "Coconuts Japan",
		[0x00C] = "Coconuts Japan/G.X.Media",
		[0x00D] = "Micronet",
		[0x00E] = "Technos",
		[0x00F] = "Mebio Software",
		[0x010] = "Shouei System",
		[0x011] = "Starfish",
		[0x013] = "Mitsui Fudosan/Dentsu",
		[0x015] = "Warashi Inc.",
		[0x017] = "Nowpro",
		[0x019] = "Game Village",
		[0x01A] = "IE Institute",
		[0x024] = "Banarex",
		[0x025] = "Starfish",
		[0x026] = "Infocom",
		[0x027] = "Electronic Arts Japan",
		[0x029] = "Cobra Team",
		[0x02A] = "Human/Field",
		[0x02B] = "KOEI",
		[0x02C] = "Hudson Soft",
		[0x02D] = "S.C.P./Game Village",
		[0x02E] = "Yanoman",
		[0x030] = "Tecmo Products",
		[0x031] = "Japan Glary Business",
		[0x032] = "Forum/OpenSystem",
		[0x033] = "Virgin Games (Japan)",
		[0x034] = "SMDE",
		[0x035] = "Yojigen",
		[0x037] = "Daikokudenki",
		[0x03D] = "Creatures Inc.",
		[0x03E] = "TDK Deep Impresion",
		[0x048] = "Destination Software/KSS",
		[0x049] = "Sunsoft/Tokai Engineering",
		[0x04A] = "POW (Planning Office Wada)/VR 1 Japan",
		[0x04B] = "Micro World",
		[0x04D] = "San-X",
		[0x04E] = "Enix",
		[0x04F] = "Loriciel/Electro Brain",
		[0x050] = "Kemco Japan",
		[0x051] = "Seta Co.,Ltd.",
		[0x052] = "Culture Brain",
		[0x053] = "Irem Corp.",
		[0x054] = "Palsoft",
		[0x055] = "Visit Co., Ltd.",
		[0x056] = "Intec",
		[0x057] = "System Sacom",
		[0x058] = "Poppo",
		[0x059] = "Ubisoft Japan",
		[0x05B] = "Media Works",
		[0x05C] = "NEC InterChannel",
		[0x05D] = "Tam",
		[0x05E] = "Gajin/Jordan",
		[0x05F] = "Smilesoft",
		[0x062] = "Mediakite",
		[0x06C] = "Viacom",
		[0x06D] = "Carrozzeria",
		[0x06E] = "Dynamic",
		[0x070] = "Magifact",
		[0x071] = "Hect",
		[0x072] = "Codemasters",
		[0x073] = "Taito/GAGA Communications",
		[0x074] = "Laguna",
		[0x075] = "Telstar Fun & Games/Event/Taito",
		[0x077] = "Arcade Zone Ltd.",
		[0x078] = "Entertainment International/Empire Software",
		[0x079] = "Loriciel",
		[0x07A] = "Gremlin Graphics",
		[0x090] = "Seika Corp.",
		[0x091] = "UBI SOFT Entertainment Software",
		[0x092] = "Sunsoft US",
		[0x094] = "Life Fitness",
		[0x096] = "System 3",
		[0x097] = "Spectrum Holobyte",
		[0x099] = "Irem",
		[0x09B] = "Raya Systems",
		[0x09C] = "Renovation Products",
		[0x09D] = "Malibu Games",
		[0x09F] = "Eidos/U.S. Gold",
		[0x0A0] = "Playmates Interactive",
		[0x0A3] = "Fox Interactive",
		[0x0A4] = "Time Warner Interactive",
		[0x0AA] = "Disney Interactive",
		[0x0AC] = "Black Pearl",
		[0x0AE] = "Advanced Productions",
		[0x0B1] = "GT Interactive",
		[0x0B2] = "RARE",
		[0x0B3] = "Crave Entertainment",
		[0x0B4] = "Absolute Entertainment",
		[0x0B5] = "Acclaim",
		[0x0B6] = "Activision",
		[0x0B7] = "American Sammy",
		[0x0B8] = "Take 2/GameTek",
		[0x0B9] = "Hi Tech",
		[0x0BA] = "LJN Ltd.",
		[0x0BC] = "Mattel",
		[0x0BE] = "Mindscape/Red Orb Entertainment",
		[0x0BF] = "Romstar",
		[0x0C0] = "Taxan",
		[0x0C1] = "Midway/Tradewest",
		[0x0C3] = "American Softworks Corp.",
		[0x0C4] = "Majesco Sales Inc.",
		[0x0C5] = "3DO",
		[0x0C8] = "Hasbro",
		[0x0C9] = "NewKidCo",
		[0x0CA] = "Telegames",
		[0x0CB] = "Metro3D",
		[0x0CD] = "Vatical Entertainment",
		[0x0CE] = "LEGO Media",
		[0x0D0] = "Xicat Interactive",
		[0x0D1] = "Cryo Interactive",
		[0x0D4] = "Red Storm Entertainment",
		[0x0D5] = "Microids",
		[0x0D7] = "Conspiracy/Swing",
		[0x0D8] = "Titus",
		[0x0D9] = "Virgin Interactive",
		[0x0DA] = "Maxis",
		[0x0DC] = "LucasArts Entertainment",
		[0x0DF] = "Ocean",
		[0x0E1] = "Electronic Arts",
		[0x0E3] = "Laser Beam",
		[0x0E6] = "Elite Systems",
		[0x0E7] = "Electro Brain",
		[0x0E8] = "The Learning Company",
		[0x0E9] = "BBC",
		[0x0EB] = "Software 2000",
		[0x0ED] = "BAM! Entertainment",
		[0x0EE] = "Studio 3",
		[0x0F2] = "Classified Games",
		[0x0F4] = "TDK Mediactive",
		[0x0F6] = "DreamCatcher",
		[0x0F7] = "JoWood Produtions",
		[0x0F8] = "SEGA",
		[0x0F9] = "Wannado Edition",
		[0x0FA] = "LSP (Light & Shadow Prod.)",
		[0x0FB] = "ITE Media",
		[0x0FC] = "Infogrames",
		[0x0FD] = "Interplay",
		[0x0FE] = "JVC (US)",
		[0x0FF] = "Parker Brothers",
		[0x101] = "SCI (Sales Curve Interactive)/Storm",
		[0x104] = "THQ Software",
		[0x105] = "Accolade Inc.",
		[0x106] = "Triffix Entertainment",
		[0x108] = "Microprose Software",
		[0x109] = "Universal Interactive/Sierra/Simon & Schuster",
		[0x10B] = "Kemco",
		[0x10C] = "Rage Software",
		[0x10D] = "Encore",
		[0x10F] = "Zoo",
		[0x110] = "Kiddinx",
		[0x111] = "Simon & Schuster Interactive",
		[0x112] = "Asmik Ace Entertainment Inc./AIA",
		[0x113] = "Empire Interactive",
		[0x116] = "Jester Interactive",
		[0x118] = "Rockstar Games",
		[0x119] = "Scholastic",
		[0x11A] = "Ignition Entertainment",
		[0x11B] = "Summitsoft",
		[0x11C] = "Stadlbauer",
		[0x120] = "Misawa",
		[0x121] = "Teichiku",
		[0x122] = "Namco Ltd.",
		[0x123] = "LOZC",
		[0x124] = "KOEI",
		[0x126] = "Tokuma Shoten Intermedia",
		[0x127] = "Tsukuda Original",
		[0x128] = "DATAM-Polystar",
		[0x12B] = "Bullet-Proof Software",
		[0x12C] = "Vic Tokai Inc.",
		[0x12E] = "Character Soft",
		[0x12F] = "I'Max",
		[0x130] = "Saurus",
		[0x133] = "General Entertainment",
		[0x136] = "I'Max",
		[0x137] = "Success",
		[0x139] = "SEGA Japan",
		[0x144] = "Takara",
		[0x145] = "Chun Soft",
		[0x146] = "Video System Co., Ltd./McO'River",
		[0x147] = "BEC",
		[0x149] = "Varie",
		[0x14A] = "Yonezawa/S'pal",
		[0x14B] = "Kaneko",
		[0x14D] = "Victor Interactive Software/Pack-in-Video",
		[0x14E] = "Nichibutsu/Nihon Bussan",
		[0x14F] = "Tecmo",
		[0x150] = "Imagineer",
		[0x153] = "Nova",
		[0x154] = "Den'Z",
		[0x155] = "Bottom Up",
		[0x157] = "TGL (Technical Group Laboratory)",
		[0x159] = "Hasbro Japan",
		[0x15B] = "Marvelous Entertainment",
		[0x15D] = "Keynet Inc.",
		[0x15E] = "Hands-On Entertainment",
		[0x168] = "Telenet",
		[0x169] = "Hori",
		[0x16C] = "Konami",
		[0x16D] = "K.Amusement Leasing Co.",
		[0x16E] = "Kawada",
		[0x16F] = "Takara",
		[0x171] = "Technos Japan Corp.",
		[0x172] = "JVC (Europe/Japan)/Victor Musical Industries",
		[0x174] = "Toei Animation",
		[0x175] = "Toho",
		[0x177] = "Namco",
		[0x178] = "Media Rings Corp.",
		[0x179] = "J-Wing",
		[0x17B] = "Pioneer LDC",
		[0x17C] = "KID",
		[0x17D] = "Mediafactory",
		[0x181] = "Infogrames Hudson",
		[0x18C] = "Acclaim Japan",
		[0x18D] = "ASCII Co./Nexoft",
		[0x18E] = "Bandai",
		[0x190] = "Enix",
		[0x192] = "HAL Laboratory/Halken",
		[0x193] = "SNK",
		[0x195] = "Pony Canyon Hanbai",
		[0x196] = "Culture Brain",
		[0x197] = "Sunsoft",
		[0x198] = "Toshiba EMI",
		[0x199] = "Sony Imagesoft",
		[0x19B] = "Sammy",
		[0x19C] = "Magical",
		[0x19D] = "Visco",
		[0x19F] = "Compile",
		[0x1A1] = "MTO Inc.",
		[0x1A3] = "Sunrise Interactive",
		[0x1A5] = "Global A Entertainment",
		[0x1A6] = "Fuuki",
		[0x1B0] = "Taito",
		[0x1B2] = "Kemco",
		[0x1B3] = "Square",
		[0x1B4] = "Tokuma Shoten",
		[0x1B5] = "Data East",
		[0x1B6] = "Tonkin House",
		[0x1B8] = "KOEI",
		[0x1BA] = "Konami/Ultra/Palcom",
		[0x1BB] = "NTVIC/VAP",
		[0x1BC] = "Use Co., Ltd.",
		[0x1BD] = "Meldac",
		[0x1BE] = "Pony Canyon (Japan)/FCI (US)",
		[0x1BF] = "Angel/Sotsu Agency/Sunrise",
		[0x1C0] = "Yumedia/Aroma Co., Ltd.",
		[0x1C3] = "Boss",
		[0x1C4] = "Axela/Crea-Tech",
		[0x1C5] = "Sekaibunka-Sha/Sumire kobo/Marigul Management Inc.",
		[0x1C6] = "Konami Computer Entertainment Osaka",
		[0x1C9] = "Enterbrain",
		[0x1D4] = "Taito/Disco",
		[0x1D5] = "Sofel",
		[0x1D6] = "Quest Corp.",
		[0x1D7] = "Sigma",
		[0x1D8] = "Ask Kodansha",
		[0x1DA] = "Naxat",
		[0x1DB] = "Copya System",
		[0x1DC] = "Capcom Co., Ltd.",
		[0x1DD] = "Banpresto",
		[0x1DE] = "TOMY",
		[0x1DF] = "Acclaim/LJN Japan",
		[0x1E1] = "NCS",
		[0x1E2] = "Human Entertainment",
		[0x1E3] = "Altron",
		[0x1E4] = "Jaleco",
		[0x1E5] = "Gaps Inc.",
		[0x1EB] = "Elf",
		[0x1F8] = "Jaleco",
		[0x1FA] = "Yutaka",
		[0x1FB] = "Varie",
		[0x1FC] = "T&ESoft",
		[0x1FD] = "Epoch Co., Ltd.",
		[0x1FF] = "Athena",
		[0x200] = "Asmik",
		[0x201] = "Natsume",
		[0x202] = "King Records",
		[0x203] = "Atlus",
		[0x204] = "Epic/Sony Records (Japan)",
		[0x206] = "IGS (Information Global Service)",
		[0x208] = "Chatnoir",
		[0x209] = "Right Stuff",
		[0x20B] = "NTT COMWARE",
		[0x20D] = "Spike",
		[0x20E] = "Konami Computer Entertainment Tokyo",
		[0x20F] = "Alphadream Corp.",
		[0x211] = "Sting",
		[0x21C] = "A Wave",
		[0x21D] = "Motown Software",
		[0x21E] = "Left Field Entertainment",
		[0x21F] = "Extreme Entertainment Group",
		[0x220] = "TecMagik",
		[0x225] = "Cybersoft",
		[0x227] = "Psygnosis",
		[0x22A] = "Davidson/Western Tech.",
		[0x22B] = "Unlicensed",
		[0x230] = "The Game Factory Europe",
		[0x231] = "Hip Games",
		[0x232] = "Aspyr",
		[0x235] = "Mastiff",
		[0x236] = "iQue",
		[0x237] = "Digital Tainment Pool",
		[0x238] = "XS Games",
		[0x239] = "Daiwon",
		[0x241] = "PCCW Japan",
		[0x244] = "KiKi Co. Ltd.",
		[0x245] = "Open Sesame Inc.",
		[0x246] = "Sims",
		[0x247] = "Broccoli",
		[0x248] = "Avex",
		[0x249] = "D3 Publisher",
		[0x24B] = "Konami Computer Entertainment Japan",
		[0x24D] = "Square-Enix",
		[0x24E] = "KSG",
		[0x24F] = "Micott & Basara Inc.",
		[0x251] = "Orbital Media",
		[0x262] = "The Game Factory USA",
		[0x265] = "Treasure",
		[0x266] = "Aruze",
		[0x267] = "Ertain",
		[0x268] = "SNK Playmore",
		[0x299] = "Yojigen"
	};

	if (nintendo_licensees[Memory.CompanyId] != NULL)
		return buf + sprintf(buf, "%s", nintendo_licensees[Memory.CompanyId]);
	else
		return buf + sprintf(buf, "Unknown company 0x%x", Memory.CompanyId);
}

static char* MapType(char* buf)
{
	return buf + sprintf(buf, Memory.LoROM ? "LoROM" : "HiROM");
}

static char* CartType(char* buf)
{
	static const char* EnhancementChips[NUMCHIPS] = {
		[NOCHIP]     = "",
		[DSP_1]      = "+DSP-1",
		[DSP_2]      = "+DSP-2",
		[DSP_3]      = "+DSP-3",
		[DSP_4]      = "+DSP-4",
		[GSU]        = "+SuperFX",
		[ST_010]     = "+ST-010",
		[ST_011]     = "+ST-011",
		[ST_018]     = "+ST-018",
		[SFT]        = "+SuFami Turbo",
		[XBAND]      = "+XBⱯND Firmware",
		[BS]         = "+Satellaview",
		[BSFW]       = "+Satellaview Firmware",
		[S_RTC]      = "+S-RTC",
		[SPC7110]    = "+SPC7110",
		[SPC7110RTC] = "+SPC7110+RTC",
		[SA_1]       = "+SA-1",
		[CX_4]       = "+CX4",
		[S_DD1]      = "+S-DD1",
		[OBC_1]      = "+OBC-1"
	};

	const char* chip = EnhancementChips[Settings.Chip] ? EnhancementChips[Settings.Chip] : "+bad chip";
	return buf + sprintf(buf, "%s", chip);
}

static char* CartSize(char* buf)
{
	if(Memory.ROMSize < 7 || Memory.ROMSize - 7 > 23)
		return buf + sprintf(buf, "with corrupted size");
	else
		return buf + sprintf(buf, "%dMbit cartridge", 1 << (Memory.ROMSize - 7));
}

static char* SRAMSize(char* buf)
{
	if (Memory.SRAMSize == 0)
		return buf;

	if(Memory.SRAMSize > 16)
		return buf + sprintf(buf, ", SRAM Error");
	else
		return buf + sprintf(buf, " with %dKB SRAM", (Memory.SRAMMask + 1) / 1024);
}

static char* ROMSpeed(char* buf)
{
	return buf + sprintf(buf, "%s (0x%02x)", (Memory.ROMSpeed & 0x10) ? "FastROM" : "SlowROM", Memory.ROMSpeed);
}

static char* TVStandard(char* buf)
{
	return buf + sprintf(buf, Settings.PAL ? "PAL" : "NTSC");
}

static char* ROMInfo(char* buf)
{
	char* at = ROMName(buf);
	*at++ = ' ';
	at =  ROMId(at);
	at += sprintf(at, " by ");
	at =  CompanyName(at);
	at += sprintf(at, ", ");
	at =  MapType(at);
	at =  CartType(at);
	*at++ = ' ';
	at =  CartSize(at);
	at =  SRAMSize(at);
	at += sprintf(at, ", ");
	at =  ROMSpeed	(at);
	at += sprintf(at, ", ");
	at =  TVStandard(at);
	sprintf(at, "\n");
	return buf;
}

bool LoadROM(const struct retro_game_info* game, char* info_buf)
{
	int32_t TotalFileSize = 0;
	bool Interleaved = false;
	bool Tales = false;
	const uint8_t* src;
	uint8_t* RomHeader = Memory.ROM;
	int32_t hi_score, lo_score;
	Memory.ExtendedFormat = NOPE;
	DeinitSPC7110();
	Memory.CalculatedSize = 0;
	Memory.HeaderCount = 0;
	TotalFileSize = game->size;
	src = game->data;
	Memory.HeaderCount = 0;

	if ((game->size & 0x1FFF) == 0x200)
	{
		TotalFileSize -= 0x200;
		src += 0x200;
		Memory.HeaderCount = 1;
	}

	if (TotalFileSize > MAX_ROM_SIZE)
		return false;

	memcpy(Memory.ROM, src, TotalFileSize);
	hi_score = ScoreHiROM(false, 0);
	lo_score = ScoreLoROM(false, 0);
	Memory.CalculatedSize = TotalFileSize & ~0x1FFF; /* round down to lower 0x2000 */
	memset(Memory.ROM + Memory.CalculatedSize, 0, MAX_ROM_SIZE - Memory.CalculatedSize);

	if (Memory.CalculatedSize > 0x400000 && !(Memory.ROM[0x7fd5] == 0x32 && ((Memory.ROM[0x7fd6] & 0xf0) == 0x40)) && /* exclude S-DD1 */
	   !(Memory.ROM[0xFFD5] == 0x3A && ((Memory.ROM[0xFFD6] & 0xF0) == 0xF0))) /* exclude SPC7110 */
		Memory.ExtendedFormat = YEAH; /* you might be a Jumbo! */

	if (Memory.ExtendedFormat != NOPE)
	{
		int32_t loromscore, hiromscore, swappedlorom, swappedhirom;
		loromscore   = ScoreLoROM(false, 0);
		hiromscore   = ScoreHiROM(false, 0);
		swappedlorom = ScoreLoROM(false, 0x400000);
		swappedhirom = ScoreHiROM(false, 0x400000);

		/* set swapped here. */
		if (MATH_MAX(swappedlorom, swappedhirom) >= MATH_MAX(loromscore, hiromscore))
		{
			Memory.ExtendedFormat = BIGFIRST;
			hi_score              = swappedhirom;
			lo_score              = swappedlorom;
			RomHeader             = Memory.ROM + 0x400000;
		}
		else
		{
			Memory.ExtendedFormat = SMALLFIRST;
			lo_score              = loromscore;
			hi_score              = hiromscore;
			RomHeader             = Memory.ROM;
		}
	}

	if (lo_score >= hi_score)
	{
		Memory.LoROM = true;

		if ((RomHeader[0x7fd5] & 0xf0) == 0x20 || (RomHeader[0x7fd5] & 0xf0) == 0x30) /* Ignore map type byte if not 0x2x or 0x3x */
		{
			switch (RomHeader[0x7fd5] & 0xf)
			{
				case 5:
					Tales = true;
					/* fall through */
				case 1:
					Interleaved = true;
					break;
			}
		}
	}
	else
	{
		if ((RomHeader[0xffd5] & 0xf0) == 0x20 || (RomHeader[0xffd5] & 0xf0) == 0x30)
		{
			switch (RomHeader[0xffd5] & 0xf)
			{
				case 0:
				case 3:
					Interleaved = true;
					break;
			}
		}

		Memory.LoROM = false;
	}

	/* More */
	if(!strncmp((char*) Memory.ROM, "BANDAI SFC-ADX", 14) && !(strncmp((char*) &Memory.ROM[0x10], "SFC-ADX BACKUP", 14) == 0))
	{
		Settings.Chip = SFT;
		Memory.LoROM = true;
		Interleaved = false;
		Tales = false;
		memmove(&Memory.ROM[0x100000], Memory.ROM, Memory.CalculatedSize);
		LoadSFTBIOS();
	}
	else if (match_lo_na("YUYU NO QUIZ DE GO!GO!") || match_lo_na("SP MOMOTAROU DENTETSU2") || match_lo_na("SUPER FORMATION SOCCE"))
	{
		Memory.LoROM = true;
		Interleaved = false;
	}
	else if (match_hi_na("\x82\x79\x82\x8f\x82\x8f\x82\xc1\x82\xc6\x96\x83\x90\x9d\x81\x49") || match_hi_na("\x5a\x6f\x6f\x82\xc1\x82\xc6\x96\x83\x90\x9d\x21\x49\x56\x54")) /* BS Zooっと麻雀 */
	{
		Memory.LoROM = false;
	}
	else if (match_hi_na("\x8D\xC4\x42\x53\x92\x54\x92\xE3\x8B\xE4\x8A\x79\x95\x94")) /* 再BS探偵倶楽部 */
	{
		Memory.LoROM = false;
	}
	else if (match_hi_na("BATMAN--REVENGE JOKER"))
	{
		Memory.LoROM = true;
		Interleaved = true;
	}
	else if (match_lo_na("THE DUEL: TEST DRIVE"))
	{
		Memory.LoROM = true;
		Interleaved = false;
	}
	else if (match_lo_na("POPEYE IJIWARU MAJO"))
	{
		Memory.LoROM = true;
		Interleaved = false;
	}
	else if (match_lo_na("POPN TWINBEE"))
	{
		Memory.LoROM = true;
		Interleaved = false;
	}
	else if (match_lo_na("MEY Fun with Numbers"))
	{
		int32_t i;

		for (i = 0x87fc0; i < 0x87fe0; i++)
			Memory.ROM[i] = 0;
	}
	else if (Memory.CalculatedSize == 0x100000 && match_hi_na("WWF SUPER WRESTLEMANIA"))
	{
		int32_t cvcount;
		memcpy(&Memory.ROM[0x100000], Memory.ROM, 0x100000);

		for (cvcount = 0; cvcount < 16; cvcount++)
		{
			memcpy(&Memory.ROM[0x8000 * cvcount], &Memory.ROM[0x10000 * cvcount + 0x100000 + 0x8000], 0x8000);
			memcpy(&Memory.ROM[0x8000 * cvcount + 0x80000], &Memory.ROM[0x10000 * cvcount + 0x100000], 0x8000);
		}

		Memory.LoROM = true;
		memset(&Memory.ROM[Memory.CalculatedSize], 0, MAX_ROM_SIZE - Memory.CalculatedSize);
	}

	if (Interleaved)
	{
		if (Tales)
		{
			if (Memory.ExtendedFormat == BIGFIRST)
			{
				DeinterleaveType1(0x400000, Memory.ROM);
				DeinterleaveType1(Memory.CalculatedSize - 0x400000, Memory.ROM + 0x400000);
			}
			else
			{
				DeinterleaveType1(Memory.CalculatedSize - 0x400000, Memory.ROM);
				DeinterleaveType1(0x400000, Memory.ROM + Memory.CalculatedSize - 0x400000);
			}

			Memory.LoROM = false;
		}
		else if (Memory.CalculatedSize == 0x300000)
		{
			Memory.LoROM = !Memory.LoROM;
			DeinterleaveGD24(Memory.CalculatedSize, Memory.ROM);
		}
		else
		{
			Memory.LoROM = !Memory.LoROM;
			DeinterleaveType1(Memory.CalculatedSize, Memory.ROM);
		}
	}

	InitROM();
	ApplyCheats();
	Reset();
	ROMInfo(info_buf);
	return true;
}

void ParseSNESHeader(uint8_t* RomHeader)
{
	if ((Settings.Chip & BS) == BS)
	{
		uint32_t size_count;
		Memory.SRAMSize = 0x05;
		strncpy(Memory.ROMName, (char*) &RomHeader[0x10], 17);
		memset(&Memory.ROMName[0x11], 0, ROM_NAME_LEN - 1 - 17);
		Memory.ROMSpeed = RomHeader[0x28];
		Memory.ROMType = 0xe5;
		Memory.ROMSize = 1;

		for (size_count = 0x800; size_count < Memory.CalculatedSize; size_count <<= 1, ++Memory.ROMSize);
	}
	else
	{
		Memory.SRAMSize = RomHeader[0x28];
		strncpy(Memory.ROMName, (char*) &RomHeader[0x10], ROM_NAME_LEN - 1);
		Memory.ROMSpeed = RomHeader[0x25];
		Memory.ROMType = RomHeader[0x26];
		Memory.ROMSize = RomHeader[0x27];
	}

	Memory.ROMRegion = RomHeader[0x29];
	/* memmove converted: Different mallocs [Neb] */
	memcpy(Memory.ROMId, &RomHeader[0x02], 4);

	if (RomHeader[0x2A] != 0x33)
	{
		Memory.CompanyId = ((RomHeader[0x2A] >> 4) & 0x0F) * 36 + (RomHeader[0x2A] & 0x0F);
	}
	else if (isalnum(RomHeader[0x00]) && isalnum(RomHeader[0x01]))
	{
		int	l, r, l2, r2;
		l = toupper(RomHeader[0x00]);
		r = toupper(RomHeader[0x01]);
		l2 = (l > '9') ? l - '7' : l - '0';
		r2 = (r > '9') ? r - '7' : r - '0';
		Memory.CompanyId = l2 * 36 + r2;
	}
}

static bool bs_name(uint8_t* p)
{
	int32_t lcount;

	for(lcount = 16; lcount > 0; lcount--)
	{
		if(*p == 0) /* null strings */
		{
			if(lcount == 16)
				return false;

			p++;
		}
		else if((*p >= 0x20 && *p <= 0x7f) || (*p >= 0xa0 && *p <= 0xdf)) /* SJIS single byte char */
			p++;
		else if(lcount >= 2) /* SJIS multi byte char */
		{
			if(((*p >= 0x81 && *p <= 0x9f) || (*p >= 0xe0 && *p <= 0xfc)) && ((*(p + 1) >= 0x40 && *(p + 1) <= 0x7e) || (*(p + 1) >= 0x80 && *(p + 1) <= 0xfc)))
			{
				p += 2;
				lcount--;
			}
			else
				return false;
		}
		else
			return false;
	}

	return true;
}

/* 7fc0h or ffc0h
 * ffc0h - FFCFh: CartName
 * ffd0h        : Memory pack location
 * ffd1h - ffd5h: 00:00:00:00:00 (??)
 * ffd6h        : Month 10h, 20h, 30h...
 * ffd7h        : Day   This byte / 8  low 3bits is unknown.
 * ffd8h        : ROMSpeed
 * ffd9h        : Satellaview ROM Type
 * ffdah        : Maker ID
 * ffdbh        : ROM Version */
static bool is_bsx(uint8_t *p) /* p == "0xFFC0" or "0x7FC0" ROM offset pointer */
{
	uint32_t c;
	int32_t i;
	bool b = false;
	bool bb = false;

	if (p[0x19] & 0x4f) /* Satellaview ROM Type */
		return false;

	c = p[0x1a]; /* Maker ID */

	if ((c != 0x33) && (c != 0xff)) /* 0x33 = Manufacturer: Nintendo */
		return false;

	c = (p[0x17] << 8) | p[0x16]; /* Month, Day */

	if ((c != 0x0000) && (c != 0xffff))
	{
		if ((c & 0x040f) != 0)
			return false;
		if ((c & 0xff) > 0xc0)
			return false;
	}

	c = p[0x18]; /* ROMSpeed */

	if ((c & 0xce) || ((c & 0x30) == 0))
		return false;

	if(p[0x10] == 0) /* Memory pack location */
		return false;

	for(i = 0; i < 8; i++)
	{
		if(p[0x10] & (1 << i))
		{
			if(bb)
				return false;

			b = true;
		}
		else if(b)
			bb = true;
	}

	if ((p[0x15] & 0x03) != 0)
		return false;

	c = p[0x13];

	if ((c != 0x00) && (c != 0xff))
		return false;

	if (p[0x14] != 0x00)
		return false;

	return bs_name(p);
}

void InitROM()
{
	uint8_t* RomHeader = Memory.ROM + 0x7FB0;
	Settings.Chip      = NOCHIP;
	SuperFX.nRomBanks  = Memory.CalculatedSize >> 15;

	if (Memory.ExtendedFormat == BIGFIRST)
		RomHeader += 0x400000;

	if (!Memory.LoROM)
		RomHeader += 0x8000;

	if ((Settings.Chip & BS) != BS)
	{
		if (is_bsx(Memory.ROM + 0x7FC0))
		{
			Settings.Chip = BS;
			Memory.LoROM = true;
		}
		else if (is_bsx(Memory.ROM + 0xFFC0))
		{
			Settings.Chip = BS;
			Memory.LoROM = false;
		}
	}

	memset(Memory.BlockIsRAM, 0, MEMMAP_NUM_BLOCKS);
	memset(Memory.BlockIsROM, 0, MEMMAP_NUM_BLOCKS);
	memset(Memory.ROMId, 0, 5);
	Memory.CompanyId = 0;
	ParseSNESHeader(RomHeader);

	/* Detect and initialize chips - detection codes are compatible with NSRT */

	if (Memory.ROMType == 0x03) /* DSP1/2/3/4 */
	{
		if (Memory.ROMSpeed == 0x30)
			Settings.Chip = DSP_4;
		else
			Settings.Chip = DSP_1;
	}
	else if (Memory.ROMType == 0x05)
	{
		if (Memory.ROMSpeed == 0x20)
			Settings.Chip = DSP_2;
		else if (Memory.ROMSpeed == 0x30 && RomHeader[0x2a] == 0xb2)
			Settings.Chip = DSP_3;
		else
			Settings.Chip = DSP_1;
	}

	switch (Settings.Chip)
	{
		case DSP_1:
			if (!Memory.LoROM)
			{
				DSP0.boundary = 0x7000;
				DSP0.maptype = M_DSP1_HIROM;
			}
			else if (Memory.CalculatedSize > 0x100000)
			{
				DSP0.boundary = 0x4000;
				DSP0.maptype = M_DSP1_LOROM_L;
			}
			else
			{
				DSP0.boundary = 0xc000;
				DSP0.maptype = M_DSP1_LOROM_S;
			}

			SetDSP = &DSP1SetByte;
			GetDSP = &DSP1GetByte;
			break;
		case DSP_2:
			DSP0.boundary = 0x10000;
			DSP0.maptype = M_DSP2_LOROM;
			SetDSP = &DSP2SetByte;
			GetDSP = &DSP2GetByte;
			break;
		case DSP_3:
			DSP0.boundary = 0xc000;
			DSP0.maptype = M_DSP3_LOROM;
			SetDSP = &DSP3SetByte;
			GetDSP = &DSP3GetByte;
			break;
		case DSP_4:
			DSP0.boundary = 0xc000;
			DSP0.maptype = M_DSP4_LOROM;
			SetDSP = &DSP4SetByte;
			GetDSP = &DSP4GetByte;
			break;
		default:
			SetDSP = NULL;
			GetDSP = NULL;
			break;
	}

	switch (((Memory.ROMType & 0xff) << 8) + (Memory.ROMSpeed & 0xff))
	{
		case 0x5535:
			Settings.Chip = S_RTC;
			InitSRTC();
			break;
		case 0xF93A:
			Settings.Chip = SPC7110RTC;
			InitSPC7110();
			break;
		case 0xF53A:
			Settings.Chip = SPC7110;
			InitSPC7110();
			break;
		case 0x2530:
			Settings.Chip = OBC_1;
			break;
		case 0x3423:
		case 0x3523:
			Settings.Chip = SA_1;
			break;
		case 0x1320:
		case 0x1420:
		case 0x1520:
		case 0x1A20:
		case 0x1330:
		case 0x1430:
		case 0x1530:
		case 0x1A30:
			Settings.Chip = GSU;

			if (Memory.ROM[0x7FDA] == 0x33)
				Memory.SRAMSize = Memory.ROM[0x7FBD];
			else
				Memory.SRAMSize = 5;

			break;
		case 0x4332:
		case 0x4532:
			Settings.Chip = S_DD1;
			break;
		case 0xE530:
			if (Settings.Chip == NOCHIP)
				Settings.Chip = BSFW;

			break;
		case 0xF530:
			Settings.Chip = ST_018;
			SetSETA = &NullSet;
			GetSETA = &NullGet;
			Memory.SRAMSize = 2;
			break;
		case 0xF630:
			if (Memory.ROM[0x7FD7] == 0x09)
			{
				Settings.Chip = ST_011;
				SetSETA = &NullSet;
				GetSETA = &NullGet;
			}
			else
			{
				Settings.Chip = ST_010;
				SetSETA = &SetST010;
				GetSETA = &GetST010;
			}

			Memory.SRAMSize = 2;
			break;
		case 0xF320:
			Settings.Chip = CX_4;
			break;
	}

	Map_Initialize();

	if (Memory.LoROM)
	{
		if ((Settings.Chip & BS) == BS)
			Map_BSLoROMMap();
		else if (Settings.Chip == ST_010 || Settings.Chip == ST_011)
			Map_SetaDSPLoROMMap();
		else if (Settings.Chip == GSU)
			Map_SuperFXLoROMMap();
		else if (Settings.Chip == SA_1)
			Map_SA1LoROMMap();
		else if (Settings.Chip == S_DD1)
			Map_SDD1LoROMMap();
		else if (Memory.ExtendedFormat != NOPE)
			Map_JumboLoROMMap();
		else if (strncmp(Memory.ROMName, "WANDERERS FROM YS", 17) == 0)
			Map_NoMAD1LoROMMap();
		else if (strncmp(Memory.ROMName, "SOUND NOVEL-TCOOL", 17) == 0 ||
		         strncmp(Memory.ROMName, "DERBY STALLION 96", 17) == 0)
			Map_ROM24MBSLoROMMap();
		else if (strncmp(Memory.ROMName, "THOROUGHBRED BREEDER3", 21) == 0 ||
		         strncmp(Memory.ROMName, "RPG-TCOOL 2", 11) == 0)
			Map_SRAM512KLoROMMap();
		else if (strncmp(Memory.ROMName, "ADD-ON BASE CASSETE", 19) == 0)
		{
			Memory.SRAMSize = 5;
			Map_SufamiTurboPseudoLoROMMap();
		}
		else
			Map_LoROMMap();
	}
	else
	{
		if (match_na("XBAND JAPANESE MODEM") || match_na("XBAND VIDEOGAME MODEM"))
		{
			Settings.Chip = XBAND;
			Map_XBANDHiROMMap();
		}
		else if ((Settings.Chip & SPC7110) == SPC7110)
			Map_SPC7110HiROMMap();
		else if (Memory.ExtendedFormat != NOPE)
			Map_ExtendedHiROMMap();
		else
			Map_HiROMMap();
	}

	Settings.PAL = ((Settings.Chip & BS) != BS && (((Memory.ROMRegion >= 2) && (Memory.ROMRegion <= 12)) || Memory.ROMRegion == 18));
	Memory.ROMName[ROM_NAME_LEN - 1] = 0;

	if (strlen(Memory.ROMName) != 0)
	{
		char *p = Memory.ROMName + strlen(Memory.ROMName);

		if (p > Memory.ROMName + 21 && Memory.ROMName[20] == ' ')
			p = Memory.ROMName + 21;

		while (p > Memory.ROMName && *(p - 1) == ' ')
			p--;

		*p = 0;
	}

	Memory.SRAMMask = Memory.SRAMSize ? ((1 << (Memory.SRAMSize + 3)) * 128) - 1 : 0;
	ResetSpeedMap();
	ApplyROMFixes();
	sprintf(Memory.ROMName,   "%s", Safe(Memory.ROMName));
	sprintf(Memory.ROMId,     "%s", Safe(Memory.ROMId));
}

void FixROMSpeed(int32_t FastROMSpeed)
{
	int32_t c;

	for (c = 0x800; c < 0x1000; c++)
		if (c & 0x8 || c & 0x400)
			Memory.MemorySpeed[c] = (uint8_t) FastROMSpeed;
}

void ResetSpeedMap()
{
	int32_t i;
	memset(Memory.MemorySpeed, Settings.SlowOneCycle, 0x1000);

	for (i = 0; i < 0x400; i += 0x10)
	{
		Memory.MemorySpeed[i + 2] = Memory.MemorySpeed[0x800 + i + 2] = Settings.OneCycle;
		Memory.MemorySpeed[i + 3] = Memory.MemorySpeed[0x800 + i + 3] = Settings.OneCycle;
		Memory.MemorySpeed[i + 4] = Memory.MemorySpeed[0x800 + i + 4] = Settings.OneCycle;
		Memory.MemorySpeed[i + 5] = Memory.MemorySpeed[0x800 + i + 5] = Settings.OneCycle;
	}
}

uint32_t map_mirror(uint32_t size, uint32_t pos)
{
	/* from bsnes */
	uint32_t mask = 1 << 31;

	if (size == 0)
		return 0;

	if (pos < size)
		return pos;

	while (!(pos & mask))
		mask >>= 1;

	if (size <= (pos & mask))
		return map_mirror(size, pos - mask);

	return mask + map_mirror(size - mask, pos - mask);
}

void map_lorom(uint32_t bank_s, uint32_t bank_e, uint32_t addr_s, uint32_t addr_e, uint32_t size)
{
	uint32_t c, i, p, addr;

	for (c = bank_s; c <= bank_e; c++)
	{
		for (i = addr_s; i <= addr_e; i += 0x1000)
		{
			p = (c << 4) | (i >> 12);
			addr = (c & 0x7f) * 0x8000;
			Memory.Map[p] = Memory.ROM + map_mirror(size, addr) - (i & 0x8000);
			Memory.BlockIsROM[p] = true;
			Memory.BlockIsRAM[p] = false;
		}
	}
}

void map_hirom(uint32_t bank_s, uint32_t bank_e, uint32_t addr_s, uint32_t addr_e, uint32_t size)
{
	uint32_t c, i, p, addr;

	for (c = bank_s; c <= bank_e; c++)
	{
		for (i = addr_s; i <= addr_e; i += 0x1000)
		{
			p = (c << 4) | (i >> 12);
			addr = c << 16;
			Memory.Map[p] = Memory.ROM + map_mirror(size, addr);
			Memory.BlockIsROM[p] = true;
			Memory.BlockIsRAM[p] = false;
		}
	}
}

void map_lorom_offset(uint32_t bank_s, uint32_t bank_e, uint32_t addr_s, uint32_t addr_e, uint32_t size, uint32_t offset)
{
	uint32_t c, i, p, addr;

	for (c = bank_s; c <= bank_e; c++)
	{
		for (i = addr_s; i <= addr_e; i += 0x1000)
		{
			p = (c << 4) | (i >> 12);
			addr = ((c - bank_s) & 0x7f) * 0x8000;
			Memory.Map[p] = Memory.ROM + offset + map_mirror(size, addr) - (i & 0x8000);
			Memory.BlockIsROM[p] = true;
			Memory.BlockIsRAM[p] = false;
		}
	}
}

void map_hirom_offset(uint32_t bank_s, uint32_t bank_e, uint32_t addr_s, uint32_t addr_e, uint32_t size, uint32_t offset)
{
	uint32_t c, i, p, addr;

	for (c = bank_s; c <= bank_e; c++)
	{
		for (i = addr_s; i <= addr_e; i += 0x1000)
		{
			p = (c << 4) | (i >> 12);
			addr = (c - bank_s) << 16;
			Memory.Map[p] = Memory.ROM + offset + map_mirror(size, addr);
			Memory.BlockIsROM[p] = true;
			Memory.BlockIsRAM[p] = false;
		}
	}
}

void map_space(uint32_t bank_s, uint32_t bank_e, uint32_t addr_s, uint32_t addr_e, uint8_t* data)
{
	uint32_t c, i, p;

	for (c = bank_s; c <= bank_e; c++)
	{
		for (i = addr_s; i <= addr_e; i += 0x1000)
		{
			p = (c << 4) | (i >> 12);
			Memory.Map[p] = data;
			Memory.BlockIsROM[p] = false;
			Memory.BlockIsRAM[p] = true;
		}
	}
}

void map_index(uint32_t bank_s, uint32_t bank_e, uint32_t addr_s, uint32_t addr_e, intptr_t index, int32_t type)
{
	uint32_t c, i, p;
	bool isROM = !(type == MAP_TYPE_I_O || type == MAP_TYPE_RAM);
	bool isRAM = !(type == MAP_TYPE_I_O || type == MAP_TYPE_ROM);

	for (c = bank_s; c <= bank_e; c++)
	{
		for (i = addr_s; i <= addr_e; i += 0x1000)
		{
			p = (c << 4) | (i >> 12);
			Memory.Map[p] = (uint8_t*) index;
			Memory.BlockIsROM[p] = isROM;
			Memory.BlockIsRAM[p] = isRAM;
		}
	}
}

void map_System()
{
	/* will be overwritten */
	map_space(0x00, 0x3f, 0x0000, 0x1fff, Memory.RAM);
	map_index(0x00, 0x3f, 0x2000, 0x3fff, MAP_PPU, MAP_TYPE_I_O);
	map_index(0x00, 0x3f, 0x4000, 0x5fff, MAP_CPU, MAP_TYPE_I_O);
	map_space(0x80, 0xbf, 0x0000, 0x1fff, Memory.RAM);
	map_index(0x80, 0xbf, 0x2000, 0x3fff, MAP_PPU, MAP_TYPE_I_O);
	map_index(0x80, 0xbf, 0x4000, 0x5fff, MAP_CPU, MAP_TYPE_I_O);
}

void map_WRAM()
{
	/* will overwrite others */
	map_space(0x7e, 0x7e, 0x0000, 0xffff, Memory.RAM);
	map_space(0x7f, 0x7f, 0x0000, 0xffff, Memory.RAM + 0x10000);
}

void map_LoROMSRAM()
{
	uint32_t hi;

	if (Memory.ROMSize > 11 || Memory.SRAMSize > 5)
		hi = 0x7fff;
	else
		hi = 0xffff;

	map_index(0x70, 0x7d, 0x0000, hi, MAP_LOROM_SRAM, MAP_TYPE_RAM);

	if (Memory.SRAMSize > 0)
		map_index(0xf0, 0xff, 0x0000, hi, MAP_LOROM_SRAM, MAP_TYPE_RAM);
}

void map_HiROMSRAM()
{
	map_index(0x20, 0x3f, 0x6000, 0x7fff, MAP_HIROM_SRAM, MAP_TYPE_RAM);
	map_index(0xa0, 0xbf, 0x6000, 0x7fff, MAP_HIROM_SRAM, MAP_TYPE_RAM);
}

void map_DSP()
{
	switch (DSP0.maptype)
	{
		case M_DSP1_LOROM_S:
			map_index(0x20, 0x3f, 0x8000, 0xffff, MAP_DSP, MAP_TYPE_I_O);
			map_index(0xa0, 0xbf, 0x8000, 0xffff, MAP_DSP, MAP_TYPE_I_O);
			break;
		case M_DSP1_LOROM_L:
			map_index(0x60, 0x6f, 0x0000, 0x7fff, MAP_DSP, MAP_TYPE_I_O);
			map_index(0xe0, 0xef, 0x0000, 0x7fff, MAP_DSP, MAP_TYPE_I_O);
			break;
		case M_DSP1_HIROM:
			map_index(0x00, 0x1f, 0x6000, 0x7fff, MAP_DSP, MAP_TYPE_I_O);
			map_index(0x80, 0x9f, 0x6000, 0x7fff, MAP_DSP, MAP_TYPE_I_O);
			break;
		case M_DSP2_LOROM:
			map_index(0x20, 0x3f, 0x6000, 0x6fff, MAP_DSP, MAP_TYPE_I_O);
			map_index(0x20, 0x3f, 0x8000, 0xbfff, MAP_DSP, MAP_TYPE_I_O);
			map_index(0xa0, 0xbf, 0x6000, 0x6fff, MAP_DSP, MAP_TYPE_I_O);
			map_index(0xa0, 0xbf, 0x8000, 0xbfff, MAP_DSP, MAP_TYPE_I_O);
			break;
		case M_DSP3_LOROM:
			map_index(0x20, 0x3f, 0x8000, 0xffff, MAP_DSP, MAP_TYPE_I_O);
			map_index(0xa0, 0xbf, 0x8000, 0xffff, MAP_DSP, MAP_TYPE_I_O);
			break;
		case M_DSP4_LOROM:
			map_index(0x30, 0x3f, 0x8000, 0xffff, MAP_DSP, MAP_TYPE_I_O);
			map_index(0xb0, 0xbf, 0x8000, 0xffff, MAP_DSP, MAP_TYPE_I_O);
			break;
	}
}

void map_CX4()
{
	map_index(0x00, 0x3f, 0x6000, 0x7fff, MAP_CX4, MAP_TYPE_I_O);
	map_index(0x80, 0xbf, 0x6000, 0x7fff, MAP_CX4, MAP_TYPE_I_O);
}

void map_OBC1()
{
	map_index(0x00, 0x3f, 0x6000, 0x7fff, MAP_OBC_RAM, MAP_TYPE_I_O);
	map_index(0x80, 0xbf, 0x6000, 0x7fff, MAP_OBC_RAM, MAP_TYPE_I_O);
}

void map_SetaDSP()
{
	/* where does the SETA chip access, anyway? please confirm these? */
	map_index(0x68, 0x6f, 0x0000, 0x7fff, MAP_SETA_DSP, MAP_TYPE_RAM);
	map_index(0x60, 0x67, 0x0000, 0x3fff, MAP_SETA_DSP, MAP_TYPE_I_O);
}

void map_WriteProtectROM()
{
	int32_t c;

	/* memmove converted: Different mallocs [Neb] */
	memcpy(Memory.WriteMap, Memory.Map, sizeof(Memory.Map));

	for (c = 0; c < 0x1000; c++)
		if (Memory.BlockIsROM[c])
			Memory.WriteMap[c] = (uint8_t*) MAP_NONE;
}

void Map_Initialize()
{
	for (int c = 0; c < 0x1000; c++)
	{
		Memory.Map[c]      = (uint8_t*) MAP_NONE;
		Memory.WriteMap[c] = (uint8_t*) MAP_NONE;
		Memory.BlockIsROM[c] = false;
		Memory.BlockIsRAM[c] = false;
	}
}

void Map_LoROMMap()
{
	map_System();
	map_lorom(0x00, 0x3f, 0x8000, 0xffff, Memory.CalculatedSize);
	map_lorom(0x40, 0x7f, 0x0000, 0xffff, Memory.CalculatedSize);
	map_lorom(0x80, 0xbf, 0x8000, 0xffff, Memory.CalculatedSize);
	map_lorom(0xc0, 0xff, 0x0000, 0xffff, Memory.CalculatedSize);

	if ((Settings.Chip & DSP) == DSP)
		map_DSP();
	else if (Settings.Chip == CX_4)
		map_CX4();
	else if (Settings.Chip == OBC_1)
		map_OBC1();

	map_LoROMSRAM();
	map_WRAM();
	map_WriteProtectROM();
}

void Map_NoMAD1LoROMMap()
{
	map_System();
	map_lorom(0x00, 0x3f, 0x8000, 0xffff, Memory.CalculatedSize);
	map_lorom(0x40, 0x7f, 0x0000, 0xffff, Memory.CalculatedSize);
	map_lorom(0x80, 0xbf, 0x8000, 0xffff, Memory.CalculatedSize);
	map_lorom(0xc0, 0xff, 0x0000, 0xffff, Memory.CalculatedSize);
	map_index(0x70, 0x7f, 0x0000, 0xffff, MAP_LOROM_SRAM, MAP_TYPE_RAM);
	map_index(0xf0, 0xff, 0x0000, 0xffff, MAP_LOROM_SRAM, MAP_TYPE_RAM);
	map_WRAM();
	map_WriteProtectROM();
}

void Map_JumboLoROMMap()
{
	/* XXX: Which game uses this? */
	map_System();
	map_lorom_offset(0x00, 0x3f, 0x8000, 0xffff, Memory.CalculatedSize - 0x400000, 0x400000);
	map_lorom_offset(0x40, 0x7f, 0x0000, 0xffff, Memory.CalculatedSize - 0x600000, 0x600000);
	map_lorom_offset(0x80, 0xbf, 0x8000, 0xffff, 0x400000,                         0);
	map_lorom_offset(0xc0, 0xff, 0x0000, 0xffff, 0x400000,                         0x200000);
	map_LoROMSRAM();
	map_WRAM();
	map_WriteProtectROM();
}

void Map_ROM24MBSLoROMMap()
{
	/* PCB: BSC-1A5M-01, BSC-1A7M-10 */
	map_System();
	map_lorom_offset(0x00, 0x1f, 0x8000, 0xffff, 0x100000, 0);
	map_lorom_offset(0x20, 0x3f, 0x8000, 0xffff, 0x100000, 0x100000);
	map_lorom_offset(0x80, 0x9f, 0x8000, 0xffff, 0x100000, 0x200000);
	map_lorom_offset(0xa0, 0xbf, 0x8000, 0xffff, 0x100000, 0x100000);
	map_LoROMSRAM();
	map_WRAM();
	map_WriteProtectROM();
}

void Map_SRAM512KLoROMMap()
{
	map_System();
	map_lorom(0x00, 0x3f, 0x8000, 0xffff,  Memory.CalculatedSize);
	map_lorom(0x40, 0x7f, 0x0000, 0xffff,  Memory.CalculatedSize);
	map_lorom(0x80, 0xbf, 0x8000, 0xffff,  Memory.CalculatedSize);
	map_lorom(0xc0, 0xff, 0x0000, 0xffff,  Memory.CalculatedSize);
	map_space(0x70, 0x70, 0x0000, 0xffff,  Memory.SRAM);
	map_space(0x71, 0x71, 0x0000, 0xffff, &Memory.SRAM[0x8000]);
	map_space(0x72, 0x72, 0x0000, 0xffff, &Memory.SRAM[0x10000]);
	map_space(0x73, 0x73, 0x0000, 0xffff, &Memory.SRAM[0x18000]);
	map_WRAM();
	map_WriteProtectROM();
}

void Map_SufamiTurboPseudoLoROMMap()
{
	/* for combined images */
	map_System();
	map_lorom_offset(0x00, 0x1f, 0x8000, 0xffff, 0x40000, 0);
	map_lorom_offset(0x20, 0x3f, 0x8000, 0xffff, 0x100000, 0x100000);
	map_lorom_offset(0x40, 0x5f, 0x8000, 0xffff, 0x100000, 0x200000);
	map_lorom_offset(0x80, 0x9f, 0x8000, 0xffff, 0x40000, 0);
	map_lorom_offset(0xa0, 0xbf, 0x8000, 0xffff, 0x100000, 0x100000);
	map_lorom_offset(0xc0, 0xdf, 0x8000, 0xffff, 0x100000, 0x200000);
	map_space(0x60, 0x63, 0x8000, 0xffff, Memory.SRAM - 0x8000);
	map_space(0xe0, 0xe3, 0x8000, 0xffff, Memory.SRAM - 0x8000);
	map_space(0x70, 0x73, 0x8000, 0xffff, Memory.SRAM + 0x4000 - 0x8000);
	map_space(0xf0, 0xf3, 0x8000, 0xffff, Memory.SRAM + 0x4000 - 0x8000);
	map_WRAM();
	map_WriteProtectROM();
}

void Map_SuperFXLoROMMap()
{
	map_System();

	/* Replicate the first 2Mb of the ROM at ROM + 2MB such that each 32K
	 * block is repeated twice in each 64K block. */
	for (int c = 0; c < 64; c++)
	{
		memmove(&Memory.ROM[0x200000 + c * 0x10000], &Memory.ROM[c * 0x8000], 0x8000);
		memmove(&Memory.ROM[0x208000 + c * 0x10000], &Memory.ROM[c * 0x8000], 0x8000);
	}

	map_lorom(0x00, 0x3f, 0x8000, 0xffff, Memory.CalculatedSize);
	map_lorom(0x80, 0xbf, 0x8000, 0xffff, Memory.CalculatedSize);
	map_hirom_offset(0x40, 0x7f, 0x0000, 0xffff, Memory.CalculatedSize, 0);
	map_hirom_offset(0xc0, 0xff, 0x0000, 0xffff, Memory.CalculatedSize, 0);
	map_space(0x00, 0x3f, 0x6000, 0x7fff, Memory.SRAM - 0x6000);
	map_space(0x80, 0xbf, 0x6000, 0x7fff, Memory.SRAM - 0x6000);
	map_space(0x70, 0x70, 0x0000, 0xffff, Memory.SRAM);
	map_space(0x71, 0x71, 0x0000, 0xffff, Memory.SRAM + 0x10000);
	map_WRAM();
	map_WriteProtectROM();
}

void Map_SetaDSPLoROMMap()
{
	map_System();
	map_lorom(0x00, 0x3f, 0x8000, 0xffff, Memory.CalculatedSize);
	map_lorom(0x40, 0x7f, 0x8000, 0xffff, Memory.CalculatedSize);
	map_lorom(0x80, 0xbf, 0x8000, 0xffff, Memory.CalculatedSize);
	map_lorom(0xc0, 0xff, 0x8000, 0xffff, Memory.CalculatedSize);
	map_SetaDSP();
	map_LoROMSRAM();
	map_WRAM();
	map_WriteProtectROM();
}

void Map_SDD1LoROMMap()
{
	map_System();
	map_lorom(0x00, 0x3f, 0x8000, 0xffff, Memory.CalculatedSize);
	map_lorom(0x80, 0xbf, 0x8000, 0xffff, Memory.CalculatedSize);
	map_hirom_offset(0x60, 0x7f, 0x0000, 0xffff, Memory.CalculatedSize, 0);
	map_hirom_offset(0xc0, 0xff, 0x0000, 0xffff, Memory.CalculatedSize, 0); /* will be overwritten dynamically */
	map_index(0x70, 0x7f, 0x0000, 0x7fff, MAP_LOROM_SRAM, MAP_TYPE_RAM);
	map_index(0xa0, 0xbf, 0x6000, 0x7fff, MAP_LOROM_SRAM, MAP_TYPE_RAM);
	map_WRAM();
	map_WriteProtectROM();
}

void Map_SA1LoROMMap()
{
	map_System();
	map_lorom(0x00, 0x3f, 0x8000, 0xffff, Memory.CalculatedSize);
	map_lorom(0x80, 0xbf, 0x8000, 0xffff, Memory.CalculatedSize);
	map_hirom_offset(0xc0, 0xff, 0x0000, 0xffff, Memory.CalculatedSize, 0);
	map_space(0x00, 0x3f, 0x3000, 0x37ff, Memory.FillRAM);
	map_space(0x80, 0xbf, 0x3000, 0x37ff, Memory.FillRAM);
	map_index(0x00, 0x3f, 0x6000, 0x7fff, MAP_BWRAM, MAP_TYPE_I_O);
	map_index(0x80, 0xbf, 0x6000, 0x7fff, MAP_BWRAM, MAP_TYPE_I_O);

	for (int c = 0x40; c < 0x4f; c++)
		map_space(c, c, 0x0000, 0xffff, &Memory.SRAM[(c & 3) * 0x10000]);

	map_WRAM();
	map_WriteProtectROM();

	/* Now copy the map and correct it for the SA1 CPU. */
	/* memmove converted: Different mallocs[Neb] */
	memcpy(SA1.Map,      Memory.Map,      sizeof(Memory.Map));
	/* memmove converted: Different mallocs[Neb] */
	memcpy(SA1.WriteMap, Memory.WriteMap, sizeof(Memory.WriteMap));

	for (int c = 0x000; c < 0x400; c += 0x10) /* SA-1 Banks 00->3f and 80->bf */
	{
		SA1.Map[c + 0] = SA1.Map[c + 0x800] = &Memory.FillRAM[0x3000];
		SA1.Map[c + 1] = SA1.Map[c + 0x801] = (uint8_t*) MAP_NONE;
		SA1.WriteMap[c + 0] = SA1.WriteMap[c + 0x800] = &Memory.FillRAM[0x3000];
		SA1.WriteMap[c + 1] = SA1.WriteMap[c + 0x801] = (uint8_t*) MAP_NONE;
	}

	for (int c = 0x600; c < 0x700; c++) /* SA-1 Banks 60->6f */
		SA1.Map[c] = SA1.WriteMap[c] = (uint8_t*) MAP_BWRAM_BITMAP;

	for (int c = 0x7e0; c < 0x800; c++) /* WRAM is inaccessable */
		SA1.Map[c] = SA1.WriteMap[c] = (uint8_t*) MAP_NONE;

	Memory.BWRAM = Memory.SRAM;
}

void Map_HiROMMap()
{
	map_System();
	map_hirom(0x00, 0x3f, 0x8000, 0xffff, Memory.CalculatedSize);
	map_hirom(0x40, 0x7f, 0x0000, 0xffff, Memory.CalculatedSize);
	map_hirom(0x80, 0xbf, 0x8000, 0xffff, Memory.CalculatedSize);
	map_hirom(0xc0, 0xff, 0x0000, 0xffff, Memory.CalculatedSize);

	if ((Settings.Chip & DSP) == DSP)
		map_DSP();

	map_HiROMSRAM();
	map_WRAM();
	map_WriteProtectROM();
}

void Map_ExtendedHiROMMap()
{
	map_System();
	map_hirom_offset(0x00, 0x3f, 0x8000, 0xffff, Memory.CalculatedSize - 0x400000, 0x400000);
	map_hirom_offset(0x40, 0x7f, 0x0000, 0xffff, Memory.CalculatedSize - 0x400000, 0x400000);
	map_hirom_offset(0x80, 0xbf, 0x8000, 0xffff, 0x400000,                         0);
	map_hirom_offset(0xc0, 0xff, 0x0000, 0xffff, 0x400000,                         0);
	map_HiROMSRAM();
	map_WRAM();
	map_WriteProtectROM();
}

void Map_SPC7110HiROMMap()
{
	map_System();
	map_index(0x00, 0x00, 0x6000, 0x7fff, MAP_HIROM_SRAM, MAP_TYPE_RAM);
	map_hirom(0x00, 0x0f, 0x8000, 0xffff, Memory.CalculatedSize);
	map_index(0x30, 0x30, 0x6000, 0x7fff, MAP_HIROM_SRAM, MAP_TYPE_RAM);

	if(Memory.ROMSize >= 13)
		map_hirom_offset(0x40, 0x4f, 0x0000, 0xffff, Memory.CalculatedSize, 0x600000);

	map_index(0x50, 0x50, 0x0000, 0xffff, MAP_SPC7110_DRAM, MAP_TYPE_ROM);
	map_hirom(0x80, 0x8f, 0x8000, 0xffff, Memory.CalculatedSize);
	map_hirom_offset(0xc0, 0xcf, 0x0000, 0xffff, Memory.CalculatedSize, 0);
	map_index(0xd0, 0xff, 0x0000, 0xffff, MAP_SPC7110_ROM,  MAP_TYPE_ROM);
	map_WRAM();
	map_WriteProtectROM();
}

void Map_BSLoROMMap()
{
	int32_t c, i;
	Memory.SRAMSize = 5;

	for (c = 0; c < 0x400; c += 16) /* Banks 00->3f and 80->bf */
	{
		Memory.Map [c + 0] = Memory.Map [c + 0x800] = Memory.RAM;
		Memory.Map [c + 1] = Memory.Map [c + 0x801] = Memory.RAM;
		Memory.BlockIsRAM [c + 0] = Memory.BlockIsRAM [c + 0x800] = true;
		Memory.BlockIsRAM [c + 1] = Memory.BlockIsRAM [c + 0x801] = true;
		Memory.Map [c + 2] = Memory.Map [c + 0x802] = (uint8_t*) MAP_PPU;
		Memory.Map [c + 3] = Memory.Map [c + 0x803] = (uint8_t*) MAP_PPU;
		Memory.Map [c + 4] = Memory.Map [c + 0x804] = (uint8_t*) MAP_CPU;
		Memory.Map [c + 5] = Memory.Map [c + 0x805] = Memory.RAM;
		Memory.BlockIsRAM [c + 5] = Memory.BlockIsRAM [c + 0x805] = true;
		Memory.Map [c + 6] = Memory.Map [c + 0x806] = Memory.RAM;
		Memory.BlockIsRAM [c + 6] = Memory.BlockIsRAM [c + 0x806] = true;
		Memory.Map [c + 7] = Memory.Map [c + 0x807] = Memory.RAM;
		Memory.BlockIsRAM [c + 7] = Memory.BlockIsRAM [c + 0x807] = true;

		for (i = c + 8; i < c + 16; i++)
		{
			Memory.Map [i] = Memory.Map [i + 0x800] = &Memory.ROM [(c << 11) % Memory.CalculatedSize] - 0x8000;
			Memory.BlockIsROM [i] = Memory.BlockIsROM [i + 0x800] = true;
		}
	}

	for (c = 0; c < 8; c++)
	{
		Memory.Map[(c << 4) + 0x105] = (uint8_t*) MAP_LOROM_SRAM;
		Memory.BlockIsROM [(c << 4) + 0x105] = false;
		Memory.BlockIsRAM [(c << 4) + 0x105] = true;
	}

	for (c = 1; c <= 4; c++)
	{
		for (i = 0; i < 16; i++)
		{
			Memory.Map[0x400 + i + (c << 4)] = (uint8_t*) MAP_LOROM_SRAM;
			Memory.BlockIsRAM[0x400 + i + (c << 4)] = true;
			Memory.BlockIsROM[0x400 + i + (c << 4)] = false;
		}
	}

	for (i = 0; i < 0x80; i++)
	{
		Memory.Map[0x700 + i] = &Memory.PSRAM[0x10000 * (i / 16)];
		Memory.BlockIsRAM[0x700 + i] = true;
		Memory.BlockIsROM[0x700 + i] = false;
	}

	for (i = 0; i < 8; i++)
	{
		Memory.Map[0x205 + (i << 4)] = Memory.Map[0x285 + (i << 4)] = Memory.Map[0x305 + (i << 4)] = Memory.Map[0x385 + (i << 4)] = Memory.Map[0x705 + (i << 4)];
		Memory.BlockIsRAM[0x205 + (i << 4)] = Memory.BlockIsRAM[0x285 + (i << 4)] = Memory.BlockIsRAM[0x305 + (i << 4)] = Memory.BlockIsRAM[0x385 + (i << 4)] = true;
		Memory.BlockIsROM[0x205 + (i << 4)] = Memory.BlockIsROM[0x285 + (i << 4)] = Memory.BlockIsROM[0x305 + (i << 4)] = Memory.BlockIsROM[0x385 + (i << 4)] = false;
	}

	for (c = 0; c < 8; c++)
	{
		Memory.Map[(c << 4) + 0x005] = Memory.PSRAM - 0x5000;
		Memory.BlockIsROM [(c << 4) + 0x005] = false;
		Memory.BlockIsRAM [(c << 4) + 0x005] = true;
	}

	map_WRAM();
	map_WriteProtectROM();
}

void Map_XBANDHiROMMap()
{
	int32_t c, i;

	for (c = 0; c < 0x400; c += 16) /* Banks 00->ff */
	{
		Memory.Map[c + 0] = Memory.Map[c + 0x800] = Memory.RAM;
		Memory.BlockIsRAM[c + 0] = Memory.BlockIsRAM[c + 0x800] = true;
		Memory.Map[c + 1] = Memory.Map[c + 0x801] = Memory.RAM;
		Memory.BlockIsRAM[c + 1] = Memory.BlockIsRAM[c + 0x801] = true;
		Memory.Map[c + 2] = Memory.Map[c + 0x802] = (uint8_t*) MAP_PPU;
		Memory.Map[c + 3] = Memory.Map[c + 0x803] = (uint8_t*) MAP_PPU;
		Memory.Map[c + 4] = Memory.Map[c + 0x804] = (uint8_t*) MAP_CPU;
		Memory.Map[c + 5] = Memory.Map[c + 0x805] = (uint8_t*) MAP_CPU;

		if ((Settings.Chip & DSP) == DSP)
		{
			Memory.Map[c + 6] = Memory.Map[c + 0x806] = (uint8_t*) MAP_DSP;
			Memory.Map[c + 7] = Memory.Map[c + 0x807] = (uint8_t*) MAP_DSP;
		}
		else
		{
			Memory.Map[c + 6] = Memory.Map[c + 0x806] = (uint8_t*) MAP_NONE;
			Memory.Map[c + 7] = Memory.Map[c + 0x807] = (uint8_t*) MAP_NONE;
		}

		for (i = c; i < c + 16; i++)
		{
			Memory.Map[i + 0x400] = Memory.Map[i + 0xc00] = &Memory.ROM[(c << 12) % Memory.CalculatedSize];
			Memory.BlockIsROM[i + 0x400] = Memory.BlockIsROM[i + 0xc00] = true;
		}

		for (i = c + 8; i < c + 16; i++)
		{
			Memory.Map[i] = Memory.Map[i + 0x800] = &Memory.ROM[(c << 12) % Memory.CalculatedSize];
			Memory.BlockIsROM[i] = Memory.BlockIsROM[i + 0x800] = true;
		}
	}

	for (c = 0; c < 16; c++) /* Banks 30->3f and b0->bf, address ranges 6000->7fff is S-RAM. */
	{
		Memory.Map[0x306 + (c << 4)] = (uint8_t*) MAP_HIROM_SRAM;
		Memory.Map[0x307 + (c << 4)] = (uint8_t*) MAP_HIROM_SRAM;
		Memory.Map[0xb06 + (c << 4)] = (uint8_t*) MAP_HIROM_SRAM;
		Memory.Map[0xb07 + (c << 4)] = (uint8_t*) MAP_HIROM_SRAM;
		Memory.BlockIsRAM[0x306 + (c << 4)] = true;
		Memory.BlockIsRAM[0x307 + (c << 4)] = true;
		Memory.BlockIsRAM[0xb06 + (c << 4)] = true;
		Memory.BlockIsRAM[0xb07 + (c << 4)] = true;
	}

	for (c = 0; c < 0x200; c++) /* XBAND */
	{
		Memory.Map[c + 0xe00] = (uint8_t*) MAP_XBAND;
		Memory.BlockIsRAM[c + 0xe00] = true;
		Memory.BlockIsROM[c + 0xe00] = false;
	}

	map_WRAM();
	map_WriteProtectROM();
}

bool match_na(const char* str)
{
	size_t len = strlen(str);

	if (len > ROM_NAME_LEN)
		return false;

	return !strncmp(Memory.ROMName, str, len);
}

bool match_lo_na(const char* str)
{
	size_t len = strlen(str);

	if (len > ROM_NAME_LEN)
		return false;

	return !strncmp((char*) &Memory.ROM[0x7fc0], str, len);
}

bool match_hi_na(const char* str)
{
	size_t len = strlen(str);

	if (len > ROM_NAME_LEN)
		return false;

	return !strncmp((char*) &Memory.ROM[0xffc0], str, len);
}

bool match_id(const char* str)
{
	return !strncmp(Memory.ROMId, str, strlen(str));
}

void ApplyROMFixes()
{
	/* NMI hacks */
	CPU.NMITriggerPoint = 4;

	if (match_na("CACOMA KNIGHT"))
		CPU.NMITriggerPoint = 25;

	Settings.Shutdown = true;

	/* Disabling a speed-up:
	 * Games which spool sound samples between the SNES and sound CPU using
	 * H-DMA as the sample is playing. */
	if (match_na("EARTHWORM JIM 2") ||
			match_na("PRIMAL RAGE") ||
			match_na("CLAY FIGHTER") ||
			match_na("ClayFighter 2") ||
			strncasecmp(Memory.ROMName, "MADDEN", 6) == 0 ||
			strncmp(Memory.ROMName, "NHL", 3) == 0 ||
			match_na("WeaponLord") ||
			strncmp(Memory.ROMName, "WAR 2410", 8) == 0)
		Settings.Shutdown = false;

	APUTimingHacks();

	/* Specific game fixes */
	Settings.SecretOfEvermoreHack = match_na("SECRET OF EVERMORE");
	Settings.StarfoxHack = match_na("STAR FOX") || match_na("STAR WING");
	Settings.WinterGold = match_na("FX SKIING NINTENDO 96") || match_na("DIRT RACER") || Settings.StarfoxHack;

	HDMATimingHacks();

	if ((Settings.Chip & SA_1) == SA_1)
		SA1ShutdownAddressHacks();

	/* Other */

	if ((Settings.Chip & BS) == BS && Memory.LoROM && match_na("F-ZERO"))
		Memory.ROM[0x7fd0] = 0xFF; /* fix memory pack position bits */
}

void APUTimingHacks()
{
	if (match_id("CQ  ") || /* Stunt Racer FX */
	    strncmp(Memory.ROMId, "JG", 2) == 0 || /* Illusion of Gaia */
	    match_na("GAIA GENSOUKI 1 JPN"))
		IAPU.OneCycle = 13;
	else if (!strcmp(Memory.ROMName, "UMIHARAKAWASE"))
		IAPU.OneCycle = 20;
	else if (match_id("AVCJ") || /* RENDERING RANGER R2 */
	         match_na("THE FISHING MASTER") || /* Mark Davis - needs >= actual APU timing. (21 is .002 Mhz slower) */
	         !strncmp(Memory.ROMId, "ARF", 3) || /* Star Ocean */
	         !strncmp(Memory.ROMId, "ATV", 3) || /* Tales of Phantasia */
	         !strncasecmp(Memory.ROMName, "ActRaiser", 9) || /* Act Raiser 1 & 2 */
	         match_na("SOULBLAZER - 1 USA") || match_na("SOULBLADER - 1") || /* Soulblazer */
	         !strncmp(Memory.ROMId, "AQT", 3) || /* Terranigma */
	         !strncmp(Memory.ROMId, "E9 ", 3) || /* Robotrek */
	         match_na("SLAP STICK 1 JPN") ||
	         !strncmp(Memory.ROMId, "APR", 3) || /* ZENNIHON PURORESU2 */
	         !strncmp(Memory.ROMId, "A4B", 3) || /* Bomberman 4 */
	         !strncmp(Memory.ROMId, "Y7 ", 3) || /* UFO KAMEN YAKISOBAN */
	         !strncmp(Memory.ROMId, "Y9 ", 3) || /* Panic Bomber World */
	         !strncmp(Memory.ROMId, "APB", 3) ||
	         ((!strncmp(Memory.ROMName, "Parlor", 6) ||
	            match_na("HEIWA Parlor!Mini8") ||
	            match_na("SANKYO Fever! \xCC\xA8\xB0\xCA\xDE\xB0!")) &&
	            Memory.CompanyId == 0x168) ||
	         match_na("DARK KINGDOM") ||
	         match_na("ZAN3 SFC") ||
	         match_na("HIOUDEN") ||
	         match_na("\xC3\xDD\xBC\xC9\xB3\xC0") || /* Tenshi no Uta */
	         match_na("FORTUNE QUEST") ||
	         match_na("FISHING TO BASSING") ||
	         match_na("TokyoDome '95Battle 7") ||
	         match_na("OHMONO BLACKBASS") ||
	         match_na("SWORD WORLD SFC") ||
	         match_na("MASTERS") || /* Augusta 2 J */
	         match_na("SFC \xB6\xD2\xDD\xD7\xB2\xC0\xDE\xB0") || /* Kamen Rider */
	         match_na("LETs PACHINKO("))  /* A set of BS games */
		IAPU.OneCycle = 15;
	else
		IAPU.OneCycle  = DEFAULT_ONE_APU_CYCLE;
}

void HDMATimingHacks()
{
	Settings.H_Max = SNES_CYCLES_PER_SCANLINE;

	/* A Couple of HDMA related hacks - Lantus */
	if ((match_na("SFX SUPERBUTOUDEN2")) || (match_na("ALIEN vs. PREDATOR")) || (match_na("ALIENS vs. PREDATOR")) || (match_na("STONE PROTECTORS")) || (match_na("SUPER BATTLETANK 2")))
		Settings.H_Max = (SNES_CYCLES_PER_SCANLINE * 130) / 100;
	else if (match_na("HOME IMPROVEMENT"))
		Settings.H_Max = (SNES_CYCLES_PER_SCANLINE * 200) / 100;
	else if (match_id("ASRJ"))                                   /* Street Racer */
		Settings.H_Max = (SNES_CYCLES_PER_SCANLINE * 95) / 100;
	else if (!strncmp(Memory.ROMId, "A3R", 3) ||                 /* Power Rangers Fight */
	         !strncmp(Memory.ROMId, "AJE", 3))                   /* Clock Tower */
		Settings.H_Max = (SNES_CYCLES_PER_SCANLINE * 103) / 100;
	else if (!strncmp(Memory.ROMId, "A3M", 3))                   /* Mortal Kombat 3. Fixes cut off speech sample */
		Settings.H_Max = (SNES_CYCLES_PER_SCANLINE * 110) / 100;
	else if (match_na("\x0bd\x0da\x0b2\x0d4\x0b0\x0bd\x0de"))
		Settings.H_Max = (SNES_CYCLES_PER_SCANLINE * 101) / 100;
	else if (!strncmp(Memory.ROMId, "A9D", 3))                   /* Start Trek: Deep Sleep 9 */
		Settings.H_Max = (SNES_CYCLES_PER_SCANLINE * 110) / 100;

	Settings.HBlankStart = (256 * Settings.H_Max) / SNES_MAX_HCOUNTER;
}

void SA1ShutdownAddressHacks()
{
	/* SA-1 Speedup settings */
	SA1.WaitPC = 0;
	SA1.WaitByteAddress1 = NULL;
	SA1.WaitByteAddress2 = NULL;

	if (match_id("ZBPJ")) /* Itoi Shigesato no Bass Tsuri No.1 (J) */
	{
		SA1.WaitPC = 0x93f1;
		SA1.WaitByteAddress1 = Memory.FillRAM + 0x304a;
	}
	else if (match_id("AEVJ")) /* Daisenryaku Expert WWII (J) */
	{
		SA1.WaitPC = 0xd18d;
		SA1.WaitByteAddress1 = Memory.FillRAM + 0x3000;
	}
	else if (match_id("A2DJ")) /* Derby Jockey 2 (J) */
		SA1.WaitPC = 0x8b62;
	else if (match_id("AZIJ")) /* Dragon Ball Z - Hyper Dimension (J) */
	{
		SA1.WaitPC = 0x8083;
		SA1.WaitByteAddress1 = Memory.FillRAM + 0x3020;
	}
	else if (match_id("ZX3J")) /* SD Gundam G NEXT (J) */
	{
		SA1.WaitPC = 0x87f2;
		SA1.WaitByteAddress1 = Memory.FillRAM + 0x30c4;
	}
	else if (match_id("AARJ")) /* Shougi no Hanamichi (J) */
	{
		SA1.WaitPC = 0xf85a;
		SA1.WaitByteAddress1 = Memory.SRAM + 0x0c64;
		SA1.WaitByteAddress2 = Memory.SRAM + 0x0c66;
	}
	if (match_id("A23J")) /* Asahi Shinbun Rensai Katou Hifumi Kudan Shougi Shingiryu (J) */
	{
		SA1.WaitPC = 0x5037;
		SA1.WaitByteAddress1 = Memory.SRAM + 0x0c06;
		SA1.WaitByteAddress2 = Memory.SRAM + 0x0c08;
	}
	else if (match_id("AIIJ")) /* Taikyoku Igo - Idaten (J) */
	{
		SA1.WaitPC = 0x00be;
		SA1.WaitByteAddress1 = Memory.SRAM + 0x1002;
		SA1.WaitByteAddress2 = Memory.SRAM + 0x1004;
	}
	else if (match_id("AITJ")) /* Takemiya Masaki Kudan no Igo Taishou (J) */
		SA1.WaitPC = 0x80b7;
	else if (match_id("AJ6J")) /* J. League '96 Dream Stadium (J) */
		SA1.WaitPC = 0xf74a;
	else if (match_id("AJUJ")) /* Jumpin' Derby (J) */
		SA1.WaitPC = 0xd926;
	else if (match_id("AKAJ")) /* Kakinoki Shougi (J) */
		SA1.WaitPC = 0xf070;
	else if (match_id("AFJJ") || match_id("AFJE")) /* Hoshi no Kirby 3 (J), Kirby's Dream Land 3 (U) */
	{
		SA1.WaitPC = 0x82d4;
		SA1.WaitByteAddress1 = Memory.SRAM + 0x72a4;
	}
	else if (match_id("AKFJ")) /* Hoshi no Kirby - Super Deluxe (J) */
	{
		SA1.WaitPC = 0x8c93;
		SA1.WaitByteAddress1 = Memory.FillRAM + 0x300a;
		SA1.WaitByteAddress2 = Memory.FillRAM + 0x300e;
	}
	else if (match_id("AKFE")) /* Kirby Super Star (U) */
	{
		SA1.WaitPC = 0x8cb8;
		SA1.WaitByteAddress1 = Memory.FillRAM + 0x300a;
		SA1.WaitByteAddress2 = Memory.FillRAM + 0x300e;
	}
	else if (match_id("ARWJ") || match_id("ARWE")) /* Super Mario RPG (J), (U) */
	{
		SA1.WaitPC = 0x816f;
		SA1.WaitByteAddress1 = Memory.FillRAM + 0x3000;
	}
	else if (match_id("AVRJ")) /* Marvelous (J) */
	{
		SA1.WaitPC = 0x85f2;
		SA1.WaitByteAddress1 = Memory.FillRAM + 0x3024;
	}
	else if (match_id("AO3J")) /* Harukanaru Augusta 3 - Masters New (J) */
	{
		SA1.WaitPC = 0xdddb;
		SA1.WaitByteAddress1 = Memory.FillRAM + 0x37b4;
	}
	else if (match_id("AJOJ")) /* Jikkyou Oshaberi Parodius (J) */
		SA1.WaitPC = 0x84e5;
	else if (match_id("APBJ")) /* Super Bomberman - Panic Bomber W (J) */
		SA1.WaitPC = 0x857a;
	else if (match_id("AONJ")) /* Pebble Beach no Hatou New - Tournament Edition (J) */
	{
		SA1.WaitPC = 0xdf33;
		SA1.WaitByteAddress1 = Memory.FillRAM + 0x37b4;
	}
	else if (match_id("AEPE")) /* PGA European Tour (U) */
	{
		SA1.WaitPC = 0x3700;
		SA1.WaitByteAddress1 = Memory.FillRAM + 0x3102;
	}
	else if (match_id("A3GE")) /* PGA Tour 96 (U) */
	{
		SA1.WaitPC = 0x3700;
		SA1.WaitByteAddress1 = Memory.FillRAM + 0x3102;
	}
	else if (match_id("A4RE")) /* Power Rangers Zeo - Battle Racers (U) */
	{
		SA1.WaitPC = 0x9899;
		SA1.WaitByteAddress1 = Memory.FillRAM + 0x3000;
	}
	else if (match_id("AGFJ")) /* SD F-1 Grand Prix (J) */
		SA1.WaitPC = 0x81bc;
	else if (match_id("ASYJ")) /* Saikousoku Shikou Shougi Mahjong (J) */
	{
		SA1.WaitPC = 0xf2cc;
		SA1.WaitByteAddress1 = Memory.SRAM + 0x7ffe;
		SA1.WaitByteAddress2 = Memory.SRAM + 0x7ffc;
	}
	else if (match_id("AX2J")) /* Shougi Saikyou II (J) */
		SA1.WaitPC = 0xd675;
	else if (match_id("A4WJ")) /* Mini Yonku Shining Scorpion - Let's & Go!! (J) */
		SA1.WaitPC = 0x48be;
	else if (match_id("AHJJ")) /* Shin Shougi Club (J) */
	{
		SA1.WaitPC = 0x002a;
		SA1.WaitByteAddress1 = Memory.SRAM + 0x0806;
		SA1.WaitByteAddress2 = Memory.SRAM + 0x0808;
	}
	else if (match_id("AMSJ")) /* ｼｮｳｷﾞｻｲｷｮｳ */
		SA1.WaitPC = 0xCD6A;
	else if (match_id("IL")) /* ﾊﾌﾞﾒｲｼﾞﾝﾉｵﾓｼﾛｼｮｳｷﾞ */
		SA1.WaitPC = 0x8549;
	else if (match_id("ALXJ")) /* MASOUKISHIN */
	{
		SA1.WaitPC = 0xEC9C;
		SA1.WaitByteAddress1 = Memory.FillRAM + 0x3072;
	}
	else if (match_id("A3IJ")) /* SUPER SHOGI3 */
		SA1.WaitPC = 0xF669;
}
