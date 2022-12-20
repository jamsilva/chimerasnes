#ifndef CHIMERASNES_FXEMU_H_
#define CHIMERASNES_FXEMU_H_

#include "chisnes.h"

/* The FXInfo_s structure, the link between the FX emulator and the SNES Emulator */
typedef struct
{
	uint32_t nRamBanks;   /* Number of 64kb-banks in GSU-RAM/BackupRAM (banks 0x70-0x73) */
	uint32_t nRomBanks;   /* Number of 32kb-banks in Cart-ROM */
	uint8_t* pvRam;       /* Pointer to GSU-RAM */
	uint8_t* pvRom;       /* Pointer to Cart-ROM */
	uint8_t* pvRegisters; /* 768 bytes located in the memory at address 0x3000 */
} FXInfo_s;

extern FXInfo_s SuperFX;

uint8_t GetSuperFX(uint16_t address);
void    SetSuperFX(uint8_t Byte, uint16_t Address);
void    SuperFXExec();
void    FxReset(FXInfo_s* psFXInfo);       /* Reset the FxChip */
void    FxEmulate(uint32_t nInstructions); /* Execute until the next stop instruction */
void    FxFlushCache();                    /* Write access to the cache - Called when the G flag in SFR is set to zero */
void    fx_dirtySCBR();                    /* SCBR write seen.  We need to update our cached screen pointers */
void    fx_updateRamBank(uint8_t Byte);    /* Update RamBankReg and RAM Bank pointer */
void    fx_computeScreenPointers();
#endif
