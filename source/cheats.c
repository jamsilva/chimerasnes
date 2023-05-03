#include <stdio.h>

#include "chisnes.h"
#include "cheats.h"
#include "memmap.h"

#define MAX_CHEATS 800

typedef struct
{
	bool     saved        : 1;
	int16_t  _SCheat_PAD1 : 15;
	uint8_t  byte;
	uint8_t  saved_byte;
	uint32_t address;
} SCheat;

typedef struct
{
	uint32_t num_cheats;
	SCheat   c[MAX_CHEATS];
} SCheatData;

SCheatData Cheat;

static INLINE uint8_t GetByteFree(uint32_t address)
{
	uint32_t Cycles = CPU.Cycles;
	uint16_t WaitPC = CPU.WaitPC;
	uint8_t byte = GetByte(address);
	CPU.WaitPC = WaitPC;
	CPU.Cycles = Cycles;
	return byte;
}

static INLINE void SetByteFree(uint8_t byte, uint32_t address)
{
	uint32_t Cycles = CPU.Cycles;
	uint16_t WaitPC = CPU.WaitPC;
	SetByte(byte, address);
	CPU.WaitPC = WaitPC;
	CPU.Cycles = Cycles;
}

static INLINE void ToUpper(char* c)
{
	if (*c >= 'a' && *c <= 'z')
		*c = *c + 'A' - 'a';
}

static bool AllHex(const char* code, int32_t len)
{
	int32_t i;

	for (i = 0; i < len; i++)
		if ((code [i] < '0' || code [i] > '9') &&
			(code [i] < 'a' || code [i] > 'f') &&
			(code [i] < 'A' || code [i] > 'F'))
			return false;

	return true;
}

static bool GameGenieToRaw(const char* code, uint32_t* address, uint8_t* byte)
{
	int32_t i, j;
	char new_code[12];
	static const char* real_hex = "0123456789ABCDEF";
	static const char* genie_hex = "DF4709156BC8A23E";
	uint32_t data = 0;

	if (strlen(code) != 9 || code[4] != '-' || !AllHex(code, 4) || !AllHex(code + 5, 4))
		return false;

	strcpy(new_code, "0x");
	strncpy(new_code + 2, code, 4);
	strcpy(new_code + 6, code + 5);

	for (i = 2; i < 10; i++)
	{
		ToUpper(&new_code[i]);

		for (j = 0; j < 16; j++)
		{
			if (new_code[i] == genie_hex[j])
			{
				new_code[i] = real_hex[j];
				break;
			}
		}

		if (j == 16)
			return false;
	}

	sscanf(new_code, "%x", &data);
	*byte = (uint8_t) (data >> 24);
	*address = ((data & 0x003c00) << 10) + ((data & 0x00003c) << 14) + ((data & 0xf00000) >> 8) + ((data & 0x000003) << 10) + ((data & 0x00c000) >> 6) + ((data & 0x0f0000) >> 12) + ((data & 0x0003c0) >> 6);
	return true;
}

static bool GoldFingerToRaw(const char* code, uint32_t* address, uint32_t* num_bytes, uint8_t* bytes)
{
	int32_t i;
	char tmp[15];

	if (strlen(code) != 14)
		return false;

	strncpy(tmp, code, 5);
	tmp[5] = 0;

	if (sscanf(tmp, "%x", address) != 1)
		return false;

	for (i = 0; i < 3; i++)
	{
		uint32_t byte;
		strncpy(tmp, code + 5 + i * 2, 2);
		tmp[2] = 0;

		if (sscanf(tmp, "%x", &byte) != 1)
			break;

		bytes[i] = (uint8_t) byte;
	}

	*num_bytes = i;
	return true;
}

static bool ProActionReplayToRaw(const char* code, uint32_t* address, uint8_t* byte)
{
	uint32_t data = 0;

	if (strlen(code) != 8 || !AllHex(code, 8) || sscanf(code, "%x", &data) != 1)
		return false;

	*address = data >> 8;
	*byte = (uint8_t) data;
	return true;
}

void AddCheat(const char* code)
{
	uint8_t  bytes[3];
	uint32_t i, address, num_bytes = 1;

	if (!GameGenieToRaw(code, &address, bytes) && !ProActionReplayToRaw(code, &address, bytes) && !GoldFingerToRaw(code, &address, &num_bytes, bytes))
		return; /* bad code, ignore */

	for (i = 0; i < num_bytes; ++i)
	{
		Cheat.c[Cheat.num_cheats].address = address + i;
		Cheat.c[Cheat.num_cheats].byte = bytes[i];
		Cheat.c[Cheat.num_cheats].saved = false;
		++Cheat.num_cheats;
	}

	ApplyCheats();
}

void ApplyCheats()
{
	uint32_t i;

	for (i = 0; i < Cheat.num_cheats; i++)
	{
		uint32_t address = Cheat.c[i].address;
		int32_t  block = (address & 0xffffff) >> MEMMAP_SHIFT;
		uint8_t* ptr = Memory.Map[block];

		if (!Cheat.c[i].saved)
		{
			Cheat.c[i].saved_byte = GetByteFree(address);
			Cheat.c[i].saved = true;
		}

		if (ptr >= (uint8_t*) MAP_LAST)
			ptr[address & 0xffff] = Cheat.c[i].byte;
		else
			SetByteFree(Cheat.c[i].byte, address);
	}
}

void RemoveCheats()
{
	uint32_t i;

	for (i = 0; i < Cheat.num_cheats; i++)
	{
		uint32_t address = Cheat.c[i].address;
		int32_t  block = (address & 0xffffff) >> MEMMAP_SHIFT;
		uint8_t* ptr = Memory.Map[block];

		if (!Cheat.c[i].saved)
			continue;

		if (ptr >= (uint8_t*) MAP_LAST)
			ptr[address & 0xffff] = Cheat.c[i].saved_byte;
		else
			SetByteFree(Cheat.c[i].saved_byte, address);
	}

	Cheat.num_cheats = 0;
}
