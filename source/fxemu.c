#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fxemu.h"
#include "fxinst.h"
#include "ppu.h"

/* The FX chip emulator's internal variables */
extern FXRegs_s FXRegs; /* This will be initialized when loading a ROM */
extern FXInfo_s SuperFX;
extern uint32_t superfx_speed_per_line;

extern void ClearIRQSource(uint32_t source);
extern void SetIRQSource(uint32_t source);

uint8_t GetSuperFX(uint16_t address)
{
	uint8_t byte = Memory.FillRAM[address];

	if (address == 0x3030)
		CPU.WaitAddress = CPU.PCAtOpcodeStart;

	if (address == 0x3031)
	{
		ClearIRQSource(GSU_IRQ_SOURCE);
		Memory.FillRAM[0x3031] = byte & 0x7f;
	}

	return byte;
}

void SetSuperFX(uint8_t Byte, uint16_t Address)
{
	uint8_t old_fill_ram;

	if ((Settings.Chip & GSU) != GSU)
		return;

	old_fill_ram = Memory.FillRAM[Address];
	Memory.FillRAM[Address] = Byte;

	switch (Address)
	{
		case 0x3030:
			if (!((old_fill_ram ^ Byte) & FLG_G))
				break;

			Memory.FillRAM[Address] = Byte;

			if (Byte & FLG_G) /* Go flag has been changed */
				SuperFXExec();
			else
				FxFlushCache();

			break;
		case 0x3034:
		case 0x3036:
			Memory.FillRAM[Address] &= 0x7f;
			break;
		case 0x3038:
			fx_dirtySCBR();
			break;
		case 0x303c:
			fx_updateRamBank(Byte);
			break;
		case 0x301f:
			Memory.FillRAM[0x3000 + GSU_SFR] |= FLG_G;
			SuperFXExec();
			break;
		default:
			break;
	}
}

void SuperFXExec()
{
	if ((Memory.FillRAM[0x3000 + GSU_SFR] & FLG_G) && (Memory.FillRAM[0x3000 + GSU_SCMR] & 0x18) == 0x18)
	{
		uint16_t GSUStatus;
		FxEmulate((Memory.FillRAM[0x3000 + GSU_CLSR] & 1) ? superfx_speed_per_line * 2 : superfx_speed_per_line);
		GSUStatus = Memory.FillRAM[0x3000 + GSU_SFR] | (Memory.FillRAM[0x3000 + GSU_SFR + 1] << 8);

		if ((GSUStatus & (FLG_G | FLG_IRQ)) == FLG_IRQ)
			SetIRQSource(GSU_IRQ_SOURCE);
	}
}

void FxFlushCache()
{
	FXRegs.vCacheBaseReg = 0;
	FXRegs.bCacheActive = false;
}

void fx_updateRamBank(uint8_t Byte)
{
	/* Update BankReg and Bank pointer */
	FXRegs.vRamBankReg = (uint32_t) Byte & (FX_RAM_BANKS - 1);
	FXRegs.pvRamBank = FXRegs.apvRamBank[Byte & 0x3];
}

static INLINE void fx_readRegisterSpaceForCheck()
{
	R15 = FXRegs.pvRegisters[30];
	R15 |= ((uint32_t) FXRegs.pvRegisters[31]) << 8;
}

static void fx_readRegisterSpaceForUse()
{
	static uint32_t avHeight[] = {128, 160, 192, 256};
	static uint32_t avMult[] = {16, 32, 32, 64};
	uint8_t* p = FXRegs.pvRegisters;
	int32_t i;

	/* Update R0 - R14 */
	for (i = 0; i < 15; i++)
	{
		FXRegs.avReg[i] = *p++;
		FXRegs.avReg[i] += ((uint32_t) (*p++)) << 8;
	}

	/* Update other registers */
	FXRegs.vStatusReg = (uint32_t) FXRegs.pvRegisters[GSU_SFR];
	FXRegs.vStatusReg |= ((uint32_t) FXRegs.pvRegisters[GSU_SFR + 1]) << 8;
	FXRegs.vPrgBankReg = (uint32_t) FXRegs.pvRegisters[GSU_PBR];
	FXRegs.vRomBankReg = (uint32_t) FXRegs.pvRegisters[GSU_ROMBR];
	FXRegs.vRamBankReg = ((uint32_t) FXRegs.pvRegisters[GSU_RAMBR]) & (FX_RAM_BANKS - 1);
	FXRegs.vCacheBaseReg = (uint32_t) FXRegs.pvRegisters[GSU_CBR];
	FXRegs.vCacheBaseReg |= ((uint32_t) FXRegs.pvRegisters[GSU_CBR + 1]) << 8;

	/* Update status register variables */
	FXRegs.vZero = !(FXRegs.vStatusReg & FLG_Z);
	FXRegs.vSign = (FXRegs.vStatusReg & FLG_S) << 12;
	FXRegs.vOverflow = (FXRegs.vStatusReg & FLG_OV) << 16;
	FXRegs.vCarry = (FXRegs.vStatusReg & FLG_CY) >> 2;

	/* Set bank pointers */
	FXRegs.pvRamBank = FXRegs.apvRamBank[FXRegs.vRamBankReg & 0x3];
	FXRegs.pvRomBank = FXRegs.apvRomBank[FXRegs.vRomBankReg];
	FXRegs.pvPrgBank = FXRegs.apvRomBank[FXRegs.vPrgBankReg];

	/* Set screen pointers */
	FXRegs.pvScreenBase = &FXRegs.pvRam[USEX8(FXRegs.pvRegisters[GSU_SCBR]) << 10];
	i = (int32_t) (!!(FXRegs.pvRegisters[GSU_SCMR] & 0x04));
	i |= ((int32_t) (!!(FXRegs.pvRegisters[GSU_SCMR] & 0x20))) << 1;
	FXRegs.vScreenHeight = FXRegs.vScreenRealHeight = avHeight[i];
	FXRegs.vMode = FXRegs.pvRegisters[GSU_SCMR] & 0x03;

	if (i == 3)
		FXRegs.vScreenSize = 32768;
	else
		FXRegs.vScreenSize = FXRegs.vScreenHeight * avMult[FXRegs.vMode] << 2;

	if (FXRegs.vPlotOptionReg & 0x10)
		FXRegs.vScreenHeight = 256; /* OBJ Mode (for drawing into sprites) */

	if (FXRegs.pvScreenBase + FXRegs.vScreenSize > FXRegs.pvRam + (FXRegs.nRamBanks * 65536))
		FXRegs.pvScreenBase = FXRegs.pvRam + (FXRegs.nRamBanks * 65536) - FXRegs.vScreenSize;

	fx_OpcodeTable[0x04c] = fx_PlotTable[FXRegs.vMode];
	fx_OpcodeTable[0x14c] = fx_PlotTable[FXRegs.vMode + 5];
	fx_OpcodeTable[0x24c] = fx_PlotTable[FXRegs.vMode];
	fx_OpcodeTable[0x34c] = fx_PlotTable[FXRegs.vMode + 5];

	if (FXRegs.vMode != FXRegs.vPrevMode || FXRegs.vPrevScreenHeight != FXRegs.vScreenHeight || FXRegs.vSCBRDirty)
		fx_computeScreenPointers();
}

void fx_dirtySCBR()
{
	FXRegs.vSCBRDirty = true;
}

void fx_computeScreenPointers()
{
	int32_t i, j, condition, mask, result;
	uint32_t apvIncrement, vMode, xIncrement;
	FXRegs.vSCBRDirty = false;

	/* Make a list of pointers to the start of each screen column*/
	vMode = FXRegs.vMode;
	condition = vMode - 2;
	mask = (condition | -condition) >> 31;
	result = (vMode & mask) | (3 & ~mask);
	vMode = result + 1;
	FXRegs.x[0] = 0;
	FXRegs.apvScreen[0] = FXRegs.pvScreenBase;
	apvIncrement = vMode << 4;

	if (FXRegs.vScreenHeight == 256)
	{
		FXRegs.x[16] = vMode << 12;
		FXRegs.apvScreen[16] = FXRegs.pvScreenBase + (vMode << 13);
		apvIncrement <<= 4;
		xIncrement = vMode << 4;

		for (i = 1, j = 17; i < 16; i++, j++)
		{
			FXRegs.x[i] = FXRegs.x[i - 1] + xIncrement;
			FXRegs.apvScreen[i] = FXRegs.apvScreen[i - 1] + apvIncrement;
			FXRegs.x[j] = FXRegs.x[j - 1] + xIncrement;
			FXRegs.apvScreen[j] = FXRegs.apvScreen[j - 1] + apvIncrement;
		}
	}
	else
	{
		xIncrement = (vMode * FXRegs.vScreenHeight) << 1;

		for (i = 1; i < 32; i++)
		{
			FXRegs.x[i] = FXRegs.x[i - 1] + xIncrement;
			FXRegs.apvScreen[i] = FXRegs.apvScreen[i - 1] + apvIncrement;
		}
	}

	FXRegs.vPrevMode = FXRegs.vMode;
	FXRegs.vPrevScreenHeight = FXRegs.vScreenHeight;
}

static INLINE void fx_writeRegisterSpaceAfterCheck()
{
	FXRegs.pvRegisters[30] = (uint8_t)  R15;
	FXRegs.pvRegisters[31] = (uint8_t) (R15 >> 8);
}

static void fx_writeRegisterSpaceAfterUse()
{
	int32_t i;
	uint8_t* p = FXRegs.pvRegisters;

	for (i = 0; i < 15; i++)
	{
		*p++ = (uint8_t)  FXRegs.avReg[i];
		*p++ = (uint8_t) (FXRegs.avReg[i] >> 8);
	}

	/* Update status register */
	if (USEX16(FXRegs.vZero) == 0)
		SF(Z);
	else
		CF(Z);

	if (FXRegs.vSign & 0x8000)
		SF(S);
	else
		CF(S);

	if (FXRegs.vOverflow >= 0x8000 || FXRegs.vOverflow < -0x8000)
		SF(OV);
	else
		CF(OV);

	if (FXRegs.vCarry)
		SF(CY);
	else
		CF(CY);

	FXRegs.pvRegisters[GSU_SFR] = (uint8_t) FXRegs.vStatusReg;
	FXRegs.pvRegisters[GSU_SFR + 1] = (uint8_t) (FXRegs.vStatusReg >> 8);
	FXRegs.pvRegisters[GSU_PBR] = (uint8_t) FXRegs.vPrgBankReg;
	FXRegs.pvRegisters[GSU_ROMBR] = (uint8_t) FXRegs.vRomBankReg;
	FXRegs.pvRegisters[GSU_RAMBR] = (uint8_t) FXRegs.vRamBankReg;
	FXRegs.pvRegisters[GSU_CBR] = (uint8_t) FXRegs.vCacheBaseReg;
	FXRegs.pvRegisters[GSU_CBR + 1] = (uint8_t) (FXRegs.vCacheBaseReg >> 8);
}

void FxReset(FXInfo_s* psFXInfo) /* Reset the FxChip */
{
	int32_t i;
	memset(&FXRegs, 0, sizeof(FXRegs_s)); /* Clear all internal variables */
	FXRegs.pvSreg = FXRegs.pvDreg = &R0; /* Set default registers */

	/* Set RAM and ROM pointers */
	FXRegs.pvRegisters = psFXInfo->pvRegisters;
	FXRegs.nRamBanks = psFXInfo->nRamBanks;
	FXRegs.pvRam = psFXInfo->pvRam;
	FXRegs.nRomBanks = psFXInfo->nRomBanks;
	FXRegs.pvRom = psFXInfo->pvRom;
	FXRegs.vPrevScreenHeight = ~0;
	FXRegs.vPrevMode = ~0;

	if (FXRegs.nRomBanks > 0x20) /* The GSU can't access more than 2mb (16mbits) */
		FXRegs.nRomBanks = 0x20;

	memset(FXRegs.pvRegisters, 0, 0x300); /* Clear FxChip register space */
	FXRegs.pvRegisters[0x3b] = 0; /* Set FxChip version Number */

	for (i = 0; i < 256; i++) /* Make ROM bank table */
	{
		uint32_t b = i & 0x7f;

		if (b >= 0x40)
		{
			if (FXRegs.nRomBanks > 2)
				b %= FXRegs.nRomBanks;
			else
				b &= 1;

			FXRegs.apvRomBank[i] = &FXRegs.pvRom[b << 16];
		}
		else
		{
			b %= FXRegs.nRomBanks * 2;
			FXRegs.apvRomBank[i] = &FXRegs.pvRom[(b << 16) + 0x200000];
		}
	}

	for (i = 0; i < 4; i++) /* Make RAM bank table */
	{
		FXRegs.apvRamBank[i] = &FXRegs.pvRam[(i % FXRegs.nRamBanks) << 16];
		FXRegs.apvRomBank[0x70 + i] = FXRegs.apvRamBank[i];
	}

	FXRegs.vPipe = 0x01; /* Start with a nop in the pipe */
	fx_readRegisterSpaceForCheck();
	fx_readRegisterSpaceForUse();
}

static bool fx_checkStartAddress()
{
	if (FXRegs.bCacheActive && R15 >= FXRegs.vCacheBaseReg && R15 < (FXRegs.vCacheBaseReg + 512)) /* Check if we start inside the cache */
		return true;

	/*  Check if we're in an unused area */

	if (FXRegs.vPrgBankReg >= 0x60 && FXRegs.vPrgBankReg <= 0x6f)
		return false;

	if (FXRegs.vPrgBankReg >= 0x74)
		return false;

	return (SCMR >> 3) & 0x03; /* False if in RAM without the RAN flag or in ROM without the RON flag */
}

void FxEmulate(uint32_t nInstructions) /* Execute until the next stop instruction */
{
	uint32_t vCount;
	fx_readRegisterSpaceForCheck(); /* Read registers and initialize GSU session */

	if (!fx_checkStartAddress()) /* Check if the start address is valid */
	{
		CF(G);
		fx_writeRegisterSpaceAfterCheck();
		return;
	}

	fx_readRegisterSpaceForUse();

	/* Execute GSU session */
	CF(IRQ);
	fx_run(nInstructions);

	/* Store GSU registers */
	fx_writeRegisterSpaceAfterCheck();
	fx_writeRegisterSpaceAfterUse();
}
