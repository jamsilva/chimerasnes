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
	MAP_BSX,
	MAP_XBAND,
	MAP_NONE,
	MAP_LAST
};

enum
{
	MAX_ROM_SIZE = 0xC00000
};

enum
{
	MAP_TYPE_I_O,
	MAP_TYPE_ROM,
	MAP_TYPE_RAM
};

typedef enum
{
	WRAP_NONE,
	WRAP_BANK,
	WRAP_PAGE
} wrap_t;

typedef enum
{
	WRITE_01,
	WRITE_10
} writeorder_t;

typedef struct
{
	bool     LoROM        : 1;
	int8_t   _CMemory_PAD : 7;
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
	uint8_t  RAM[0x20000];
	uint8_t  SRAM[0x20000];
	uint8_t  VRAM[0x10000];
	uint8_t  FillRAM[MAX_ROM_SIZE + 0x200 + 0x8000];
	uint16_t CompanyId;
	int32_t  HeaderCount;
	uint32_t CalculatedSize;
	uint32_t SRAMMask;
	uint8_t* BWRAM;
	uint8_t* CX4RAM;
	uint8_t* OBC1RAM;
	uint8_t* PSRAM;
	uint8_t* BIOSROM;
	uint8_t* ROM;
	uint8_t* Map[MEMMAP_NUM_BLOCKS];
	uint8_t* WriteMap[MEMMAP_NUM_BLOCKS];
} CMemory;

extern CMemory* MemoryPtr;
#define Memory (*MemoryPtr)

bool     LoadROM(const struct retro_game_info* game, char* info_buf);
void     InitROM();
bool     InitMemory();
void     DeinitMemory();
void     FixROMSpeed(int32_t FastROMSpeed);
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
uint16_t GetWord(uint32_t Address, wrap_t w);
void     SetByte(uint8_t Byte, uint32_t Address);
void     SetWord(uint16_t Word, uint32_t Address, wrap_t w, writeorder_t o);
void     SetPCBase(uint32_t Address);
uint8_t* GetMemPointer(uint32_t Address);
uint8_t* GetBasePointer(uint32_t Address);

uint32_t map_mirror(uint32_t, uint32_t);
void     map_lorom(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
void     map_hirom(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
void     map_lorom_offset(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
void     map_hirom_offset(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
void     map_space(uint32_t, uint32_t, uint32_t, uint32_t, uint8_t*);
void     map_index(uint32_t, uint32_t, uint32_t, uint32_t, intptr_t, int32_t);
void     map_System();
void     map_WRAM();
void     map_LoROMSRAM();
void     map_HiROMSRAM();
void     map_DSP();
void     map_C4();
void     map_OBC1();
void     map_SetaDSP();
void     map_XBAND();
void     map_WriteProtectROM();
void     Map_Initialize();
void     Map_LoROMMap();
void     Map_NoMAD1LoROMMap();
void     Map_JumboLoROMMap();
void     Map_ROM24MBSLoROMMap();
void     Map_SRAM512KLoROMMap();
void     Map_SufamiTurboPseudoLoROMMap();
void     Map_SuperFXLoROMMap();
void     Map_SetaDSPLoROMMap();
void     Map_SDD1LoROMMap();
void     Map_SA1LoROMMap();
void     Map_HiROMMap();
void     Map_ExtendedHiROMMap();
void     Map_SPC7110HiROMMap();
#endif
