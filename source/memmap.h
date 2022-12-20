#ifndef CHIMERASNES_MEMMAP_H_
#define CHIMERASNES_MEMMAP_H_

#include "chisnes.h"

#define MEMMAP_BLOCK_SIZE      (0x1000)
#define MEMMAP_NUM_BLOCKS      (0x1000000 / MEMMAP_BLOCK_SIZE)
#define MEMMAP_BLOCKS_PER_BANK (0x10000 / MEMMAP_BLOCK_SIZE)
#define MEMMAP_SHIFT           12
#define MEMMAP_MASK            (MEMMAP_BLOCK_SIZE - 1)

/* Extended ROM Formats */
enum
{
	NOPE,
	YEAH,
	BIGFIRST,
	SMALLFIRST
};

bool     LoadROM(const struct retro_game_info* game);
void     InitROM(bool);
bool     InitMemory();
void     DeinitMemory();
void     WriteProtectROM();
void     FixROMSpeed(int32_t FastROMSpeed);
void     MapRAM();
void     MapExtraRAM();
void     SetaDSPMap();
void     BSLoROMMap();
void     JumboLoROMMap(bool);
void     LoROMMap();
void     LoROM24MBSMap();
void     SRAM512KLoROMMap();
void     SRAM1024KLoROMMap();
void     SufamiTurboLoROMMap();
void     HiROMMap();
void     SuperFXROMMap();
void     TalesROMMap(bool);
void     AlphaROMMap();
void     SA1ROMMap();
void     SPC7110HiROMMap();
void     SPC7110Sram(uint8_t);
void     SetaDSPMap();
void     DSPMap();
void     CapcomProtectLoROMMap();
void     XBANDHiROMMap();
bool     match_na(const char*);
bool     match_lo_na(const char* str);
bool     match_hi_na(const char* str);
bool     match_id(const char*);
void     ApplyROMFixes();
void     APUTimingHacks();
void     HDMATimingHacks();
void     SA1ShutdownAddressHacks();
void     ShutdownHacks();
void     ParseSNESHeader(uint8_t*);
void     ResetSpeedMap();
uint8_t  GetByte(uint32_t Address);
uint16_t GetWord(uint32_t Address);
void     SetByte(uint8_t Byte, uint32_t Address);
void     SetWord(uint16_t Byte, uint32_t Address);
void     SetPCBase(uint32_t Address);
uint8_t* GetMemPointer(uint32_t Address);
uint8_t* GetBasePointer(uint32_t Address);

enum
{
	MAP_CPU,
	MAP_PPU,
	MAP_LOROM_SRAM,
	MAP_HIROM_SRAM,
	MAP_DSP,
	MAP_SA1RAM,
	MAP_BWRAM,
	MAP_BWRAM_BITMAP,
	MAP_BWRAM_BITMAP2,
	MAP_SPC7110_ROM,
	MAP_SPC7110_DRAM,
	MAP_RONLY_SRAM,
	MAP_CX4,
	MAP_OBC_RAM,
	MAP_SETA_DSP,
	MAP_XBAND,
	MAP_NONE,
	MAP_LAST
};

enum
{
	MAX_ROM_SIZE = 0x800000
};

enum
{
	MAP_TYPE_I_O,
	MAP_TYPE_ROM,
	MAP_TYPE_RAM
};

typedef struct
{
	bool     LoROM         : 1;
	int8_t   _CMemory_PAD1 : 7;
	char     CompanyId[3];
	char     ROMId[5];
	char     ROMName[ROM_NAME_LEN];
	uint8_t  ExtendedFormat;
	uint8_t  SRAMSize;
	uint8_t  ROMRegion;
	uint8_t  ROMSize;
	uint8_t  ROMSpeed;
	uint8_t  ROMType;
	uint8_t  MemorySpeed[MEMMAP_NUM_BLOCKS];
	uint8_t  BlockIsRAM[MEMMAP_NUM_BLOCKS];
	uint8_t  BlockIsROM[MEMMAP_NUM_BLOCKS];
	int16_t  _CMemory_PAD2 : 16;
	int32_t  _CMemory_PAD3 : 32;
	int32_t  HeaderCount;
	uint32_t CalculatedSize;
	uint32_t SRAMMask;
	uint8_t* BWRAM;
	uint8_t* CX4RAM;
	uint8_t* FillRAM;
	uint8_t* OBC1RAM;
	uint8_t* PSRAM;
	uint8_t* RAM;
	uint8_t* ROM;
	uint8_t* SRAM;
	uint8_t* VRAM;
	uint8_t* Map[MEMMAP_NUM_BLOCKS];
	uint8_t* WriteMap[MEMMAP_NUM_BLOCKS];
} CMemory;

extern CMemory Memory;
#endif
