#ifndef CHIMERASNES_FXINST_H_
#define CHIMERASNES_FXINST_H_

/*
 * FxChip(GSU) register space specification (Register address space 3000-32ff)
 *
 * The 16 generic 16 bit registers:
 * (Some have a special function in special circumstances)
 * 3000 - R0   default source/destination register
 * 3002 - R1   pixel plot X position register
 * 3004 - R2   pixel plot Y position register
 * 3006 - R3
 * 3008 - R4   lower 16 bit result of lmult
 * 300a - R5
 * 300c - R6   multiplier for fmult and lmult
 * 300e - R7   fixed point texel X position for merge
 * 3010 - R8   fixed point texel Y position for merge
 * 3012 - R9
 * 3014 - R10
 * 3016 - R11  return address set by link
 * 3018 - R12  loop counter
 * 301a - R13  loop point address
 * 301c - R14  rom address for getb, getbh, getbl, getbs
 * 301e - R15  program counter
 * 3020-302f - unused
 *
 * Other internal registers
 * 3030 - SFR  status flag register (16bit)
 * 3032 -   unused
 * 3033 - BRAMR Backup RAM register (8bit)
 * 3034 - PBR  program bank register (8bit)
 * 3035 -   unused
 * 3036 - ROMBR   rom bank register (8bit)
 * 3037 - CFGR control flags register (8bit)
 * 3038 - SCBR screen base register (8bit)
 * 3039 - CLSR clock speed register (8bit)
 * 303a - SCMR screen mode register (8bit)
 * 303b - VCR  version code register (8bit) (read only)
 * 303c - RAMBR   ram bank register (8bit)
 * 303d - unused
 * 303e - CBR  cache base register (16bit)
 * 3040-30ff - unused
 * 3100-32ff - CACHERAM 512 bytes of GSU cache memory
 *
 * SFR status flag register bits:
 *  0   -
 *  1   Z   Zero flag
 *  2   CY  Carry flag
 *  3   S   Sign flag
 *  4   OV  Overflow flag
 *  5   G   Go flag (set to 1 when the GSU is running)
 *  6   R   Set to 1 when reading ROM using R14 address
 *  7   -
 *  8   ALT1   Mode set-up flag for the next instruction
 *  9   ALT2   Mode set-up flag for the next instruction
 * 10   IL  Immediate lower 8-bit flag
 * 11   IH  Immediate higher 8-bit flag
 * 12   B   Set to 1 when the WITH instruction is executed
 * 13   -
 * 14   -
 * 15   IRQ Set to 1 when GSU caused an interrupt
 *              Set to 0 when read by 658c16
 *
 * BRAMR = 0, BackupRAM is disabled
 * BRAMR = 1, BackupRAM is enabled
 *
 * CFGR control flags register bits:
 *  0   -
 *  1   -
 *  2   -
 *  3   -
 *  4   -
 *  5   MS0 Multiplier speed, 0=standard, 1=high speed
 *  6   -
 *  7   IRQ Set to 1 when GSU interrupt request is masked
 *
 * CLSR clock speed register bits:
 *  0   CLSR   clock speed, 0 = 10.7Mhz, 1 = 21.4Mhz
 *
 * SCMR screen mode register bits:
 *  0 MD0   color depth mode bit 0
 *  1 MD1   color depth mode bit 1
 *  2 HT0   screen height bit 1
 *  3 RAN   RAM access control
 *  4 RON   ROM access control
 *  5 HT1   screen height bit 2
 *  6 -
 *  7 -
 *
 * RON = 0  SNES CPU has ROM access
 * RON = 1  GSU has ROM access
 * RAN = 0  SNES has game pak RAM access
 * RAN = 1  GSU has game pak RAM access
 *
 * HT1  HT0  Screen height mode
 *  0    0   128 pixels high
 *  0    1   160 pixels high
 *  1    0   192 pixels high
 *  1    1   OBJ mode
 *
 * MD1  MD0  Color depth mode
 *  0    0   4 color mode
 *  0    1   16 color mode
 *  1    0   not used
 *  1    1   256 color mode
 *
 * CBR cache base register bits:
 * 15-4       Specify base address for data to cache from ROM or RAM
 *  3-0       Are 0 when address is read
 *
 * Write access to the program counter (301e) from
 * the SNES-CPU will start the GSU, and it will not
 * stop until it reaches a stop instruction.
 */

/* Number of banks in GSU RAM */
#define FX_RAM_BANKS 4

typedef struct
{
	uint8_t   bCacheActive;

	/* FxChip registers */
	int8_t    _FXRegs_s_PAD1 : 8;
	uint8_t   vRomBuffer;               /* Current byte read by R14 */
	uint8_t   vPipe;                    /* Instructionset pipe */
	uint32_t  avReg[16];                /* 16 Generic registers */
	uint32_t  vColorReg;                /* Internal color register */
	uint32_t  vPlotOptionReg;           /* Plot option register */
	uint32_t  vStatusReg;               /* Status register */
	uint32_t  vPrgBankReg;              /* Program bank index register */
	uint32_t  vRomBankReg;              /* Rom bank index register */
	uint32_t  vRamBankReg;              /* Ram bank index register */
	uint32_t  vCacheBaseReg;            /* Cache base address register */
	uint32_t  vLastRamAdr;              /* Last RAM address accessed */
	uint32_t  vSCBRDirty;               /* if SCBR is written, our cached screen pointers need updating */
	uint32_t* pvDreg;                   /* Pointer to current destination register */
	uint32_t* pvSreg;                   /* Pointer to current source register */

	/* status register optimization stuff */
	int32_t   vOverflow;                /* (v >= 0x8000 || v < -0x8000) */
	uint32_t  vCarry;                   /* a value of 1 or 0 */
	uint32_t  vSign;                    /* v & 0x8000 */
	uint32_t  vZero;                    /* v == 0 */

	/* Other emulator variables */
	int32_t   x[32];
	uint32_t  vScreenHeight;            /* 128, 160, 192 or 256 (could be overriden by cmode) */
	uint32_t  vScreenRealHeight;        /* 128, 160, 192 or 256 */
	uint32_t  vPrevScreenHeight;
	uint32_t  vScreenSize;
	uint32_t  nRamBanks;                /* Number of 64kb-banks in FxRam (Don't confuse it with SNES-Ram!!!) */
	uint32_t  nRomBanks;                /* Number of 32kb-banks in Cart-ROM */
	uint32_t  vMode;                    /* Color depth/mode */
	uint32_t  vPrevMode;                /* Previous depth */
	uint8_t*  pvRegisters;              /* 768 bytes located in the memory at address 0x3000 */
	uint8_t*  pvRam;                    /* Pointer to FxRam */
	uint8_t*  pvRom;                    /* Pointer to Cart-ROM */
	uint8_t*  pvScreenBase;
	uint8_t*  apvScreen[32];            /* Pointer to each of the 32 screen colums */
	uint8_t*  pvRamBank;                /* Pointer to current RAM-bank */
	uint8_t*  pvRomBank;                /* Pointer to current ROM-bank */
	uint8_t*  pvPrgBank;                /* Pointer to current program ROM-bank */
	uint8_t*  apvRamBank[FX_RAM_BANKS]; /* Ram bank table (max 256kb) */
	uint8_t*  apvRomBank[256];          /* Rom bank table */
} FXRegs_s;

extern FXRegs_s FXRegs;

enum /* GSU registers */
{
	GSU_SFR   = 0x030,
	GSU_BRAMR = 0x033,
	GSU_PBR   = 0x034,
	GSU_ROMBR = 0x036,
	GSU_CFGR  = 0x037,
	GSU_SCBR  = 0x038,
	GSU_CLSR  = 0x039,
	GSU_SCMR  = 0x03a,
	GSU_VCR   = 0x03b,
	GSU_RAMBR = 0x03c,
	GSU_CBR   = 0x03e
};

enum /* SFR flags */
{
	FLG_Z    = (1 << 1),
	FLG_CY   = (1 << 2),
	FLG_S    = (1 << 3),
	FLG_OV   = (1 << 4),
	FLG_G    = (1 << 5),
	FLG_R    = (1 << 6),
	FLG_ALT1 = (1 << 8),
	FLG_ALT2 = (1 << 9),
	FLG_IL   = (1 << 10),
	FLG_IH   = (1 << 11),
	FLG_B    = (1 << 12),
	FLG_IRQ  = (1 << 15)
};

/* Test flag */
#define TF(a) (FXRegs.vStatusReg &   FLG_##a)
#define CF(a) (FXRegs.vStatusReg &= ~FLG_##a)
#define SF(a) (FXRegs.vStatusReg |=  FLG_##a)

/* Test and set flag if condition, clear if not */
#define TS(a, b) (FXRegs.vStatusReg = ((FXRegs.vStatusReg & (~FLG_##a)) | ((!!(##b)) * FLG_##a)))

/* Testing ALT1 & ALT2 bits */
#define ALT0 (!TF(ALT1) && !TF(ALT2))
#define ALT1 ( TF(ALT1) && !TF(ALT2))
#define ALT2 (!TF(ALT1) &&  TF(ALT2))
#define ALT3 ( TF(ALT1) &&  TF(ALT2))

/* Sign extend from 8/16 bit to 32 bit */
#define SEX8(a)  ((int32_t) ((int8_t)  (a)))
#define SEX16(a) ((int32_t) ((int16_t) (a)))

/* Unsign extend from 8/16 bit to 32 bit */
#define USEX8(a)   ((uint32_t) ((uint8_t)  (a)))
#define USEX16(a)  ((uint32_t) ((uint16_t) (a)))
#define SUSEX16(a) ((int32_t)  ((uint16_t) (a)))

/* Set/Clr Sign and Zero flag */
#define TSZ(num) \
	TS(S, (num & 0x8000)); \
	TS(Z, (!USEX16(num)))

/* Clear flags */
#define CLRFLAGS                                         \
	FXRegs.vStatusReg &= ~(FLG_ALT1 | FLG_ALT2 | FLG_B); \
	FXRegs.pvDreg = FXRegs.pvSreg = &R0

/* Read current RAM-Bank */
#define RAM(adr) (FXRegs.pvRamBank[USEX16(adr)])

/* Read current ROM-Bank */
#define ROM(idx) (FXRegs.pvRomBank[USEX16(idx)])

/* Access the current value in the pipe */
#define PIPE FXRegs.vPipe

/* Access data in the current program bank */
#define PRGBANK(idx) FXRegs.pvPrgBank[USEX16(idx)]

/* Update pipe from ROM */
#define FETCHPIPE \
	PIPE = PRGBANK(R15)

/* Access source register */
#define SREG (*FXRegs.pvSreg)

/* Access destination register */
#define DREG (*FXRegs.pvDreg)

/* Read R14 */
#define READR14 FXRegs.vRomBuffer = ROM(R14)

/* Test and/or read R14 */
#define TESTR14                \
	if (FXRegs.pvDreg == &R14) \
		READR14

/* Access to registers */
#define R0    FXRegs.avReg[0]
#define R1    FXRegs.avReg[1]
#define R2    FXRegs.avReg[2]
#define R3    FXRegs.avReg[3]
#define R4    FXRegs.avReg[4]
#define R5    FXRegs.avReg[5]
#define R6    FXRegs.avReg[6]
#define R7    FXRegs.avReg[7]
#define R8    FXRegs.avReg[8]
#define R9    FXRegs.avReg[9]
#define R10   FXRegs.avReg[10]
#define R11   FXRegs.avReg[11]
#define R12   FXRegs.avReg[12]
#define R13   FXRegs.avReg[13]
#define R14   FXRegs.avReg[14]
#define R15   FXRegs.avReg[15]
#define SFR   FXRegs.vStatusReg
#define PBR   FXRegs.vPrgBankReg
#define ROMBR FXRegs.vRomBankReg
#define RAMBR FXRegs.vRamBankReg
#define CBR   FXRegs.vCacheBaseReg
#define SCBR  USEX8(FXRegs.pvRegisters[GSU_SCBR])
#define SCMR  USEX8(FXRegs.pvRegisters[GSU_SCMR])
#define COLR  FXRegs.vColorReg
#define POR   FXRegs.vPlotOptionReg
#define BRAMR USEX8(FXRegs.pvRegisters[GSU_BRAMR])
#define VCR   USEX8(FXRegs.pvRegisters[GSU_VCR])
#define CFGR  USEX8(FXRegs.pvRegisters[GSU_CFGR])
#define CLSR  USEX8(FXRegs.pvRegisters[GSU_CLSR])

extern void (*fx_OpcodeTable[])();
extern void (*fx_PlotTable[])();

void fx_run(uint32_t nInstructions);
#endif
