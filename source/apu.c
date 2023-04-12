#include "chisnes.h"
#include "spc700.h"
#include "apu.h"
#include "soundux.h"
#include "cpuexec.h"
#include "snesapu.h"

uint8_t APUROM[64] =
{
	0xCD, 0xEF, 0xBD, 0xE8, 0x00, 0xC6, 0x1D, 0xD0,
	0xFC, 0x8F, 0xAA, 0xF4, 0x8F, 0xBB, 0xF5, 0x78,
	0xCC, 0xF4, 0xD0, 0xFB, 0x2F, 0x19, 0xEB, 0xF4,
	0xD0, 0xFC, 0x7E, 0xF4, 0xD0, 0x0B, 0xE4, 0xF5,
	0xCB, 0xF4, 0xD7, 0x00, 0xFC, 0xD0, 0xF3, 0xAB,
	0x01, 0x10, 0xEF, 0x7E, 0xF4, 0x10, 0xEB, 0xBA,
	0xF6, 0xDA, 0x00, 0xBA, 0xF4, 0xC4, 0xF4, 0xDD,
	0x5D, 0xD0, 0xDB, 0x1F, 0x00, 0x00, 0xC0, 0xFF
};

bool InitAPU()
{
	IAPU.RAM = (uint8_t*) malloc(0x10000);

	if (IAPU.RAM == NULL)
	{
		DeinitAPU();
		return false;
	}

	InitAPUDSP();
	return true;
}

void DeinitAPU()
{
	if (IAPU.RAM == NULL)
		return;

	free(IAPU.RAM);
	IAPU.RAM = NULL;
}

void ResetAPU()
{
	int32_t i, j;
	Settings.APUEnabled = true;
	memset(IAPU.RAM, 0, 0x100);
	memset(IAPU.RAM + 0x20, 0xFF, 0x20);
	memset(IAPU.RAM + 0x60, 0xFF, 0x20);
	memset(IAPU.RAM + 0xA0, 0xFF, 0x20);
	memset(IAPU.RAM + 0xE0, 0xFF, 0x20);

	for (i = 1; i < 256; i++)
		memcpy(IAPU.RAM + (i << 8), IAPU.RAM, 0x100);

	memset(&APU, 0, sizeof(APU));
	memset(APU.OutPorts, 0, sizeof(APU.OutPorts));
	IAPU.DirectPage = IAPU.RAM;
	/* memmove converted: Different mallocs [Neb] */
	memcpy(&IAPU.RAM[0xffc0], APUROM, sizeof(APUROM));
	IAPU.PC = IAPU.RAM + IAPU.RAM[0xfffe] + (IAPU.RAM[0xffff] << 8);
	IAPU.Registers.YA.W = 0;
	IAPU.Registers.X = 0;
	IAPU.Registers.S = 0xef;
	IAPU.Registers.P = 0x02;
	APUUnpackStatus();
	IAPU.Registers.PC = 0;
	IAPU.Executing = Settings.APUEnabled;
	IAPU.WaitAddress1 = NULL;
	IAPU.WaitAddress2 = NULL;
	IAPU.WaitCounter = 1;
	EXT.t64Cnt &= ~7;
	EXT.APUTimerCounter = (int32_t) ((((uint64_t) SNES_CYCLES_PER_SECOND << FIXED_POINT_SHIFT)) / 64000);
	EXT.APUTimerCounter_err = EXT.APUTimerCounter & FIXED_POINT_REMAINDER;
	EXT.NextAPUTimerPos = APU.Cycles + (EXT.APUTimerCounter >> FIXED_POINT_SHIFT);
	IAPU.RAM[0xf0] = 0x0a; /* timers_enabled & ram_writable */
	APU.ShowROM = true;
	IAPU.RAM[0xf1] = 0x80;

	for (i = 0; i < 256; i++)
		APUCycles[i] = APUCycleLengths[i] * IAPU.OneCycle;

	ResetAPUDSP();
}

void SetAPUControl(uint8_t byte)
{
	int8_t i, j;

	for (i = 0, j = 1 ; i < 3 ; ++i, j <<= 1)
	{
		bool enableTimer = (bool) (byte & j);

		if (enableTimer && !APU.TimerEnabled[i])
		{
			APU.Timer[i] = 0;
			IAPU.RAM[0xfd + i] = 0;
			APU.TimerTarget[i] = IAPU.RAM[0xfa + i];

			if (APU.TimerTarget[i] == 0)
				APU.TimerTarget[i] = 0x100;
		}

		APU.TimerEnabled[i] = enableTimer;
	}

	if (byte & 0x10)
		IAPU.RAM[0xf4] = IAPU.RAM[0xf5] = 0;

	if (byte & 0x20)
		IAPU.RAM[0xf6] = IAPU.RAM[0xf7] = 0;

	if ((byte & 0x80) && !APU.ShowROM)
		memcpy(&IAPU.RAM[0xffc0], APUROM, sizeof(APUROM));

	APU.ShowROM = (bool) (byte & 0x80);
	IAPU.RAM[0xf1] = byte;
}

uint8_t APUGetByte(uint16_t addr)
{
	uint8_t r;

	switch (addr)
	{
		case 0xf0: /* TEST */
		case 0xf1: /* CONTROL */
		case 0xfa: /* T0TARGET */
		case 0xfb: /* T1TARGET */
		case 0xfc: /* T2TARGET - write-only registers */
			return 0x00;
		case 0xf2: /* DSPADDR */
		case 0xf8: /* RAM0 */
		case 0xf9: /* RAM1 */
			return IAPU.RAM[addr];
		case 0xf3: /* DSPDATA */
			r = IAPU.RAM[0xf2] & 0x7f;

			if ((r & 0x0f) == APU_ENVX)
				return APU.DSP[r] & 0x7f;

			return APU.DSP[r];
		case 0xf4: /* CPUIO0 */
		case 0xf5: /* CPUIO1 */
		case 0xf6: /* CPUIO2 */
		case 0xf7: /* CPUIO3 */
			IAPU.WaitAddress2 = IAPU.WaitAddress1;
			IAPU.WaitAddress1 = IAPU.PC;
			return IAPU.RAM[addr];
		case 0xfd: /* T0OUT */
		case 0xfe: /* T1OUT */
		case 0xff: /* T2OUT - 4-bit counter values */
			r = IAPU.RAM[addr] & 15;
			IAPU.WaitAddress2 = IAPU.WaitAddress1;
			IAPU.WaitAddress1 = IAPU.PC;
			IAPU.RAM[addr] = 0;
			return r;
		default:
			if (addr >= 0xffc0 && APU.ShowROM)
				return APUROM[addr & 0x3f];

			if (IAPU.RAM[0xf0] & 0x04) /* ram_disabled */
				return 0x5a; /* 0xff on mini-SNES */

			return IAPU.RAM[addr];
	}
}

void APUSetByte(uint8_t data, uint16_t addr)
{
	switch (addr)
	{
		case 0xf0: /* TEST */
			if (!APUCheckDirectPage()) /* writes only valid when P flag is clear */
				IAPU.RAM[addr] = data;

			return;
		case 0xf1: /* CONTROL */
			SetAPUControl(data);
			/* fall through */
		case 0xf2: /* DSPADDR */
		case 0xf8: /* RAM0 */
		case 0xf9: /* RAM1 */
			IAPU.RAM[addr] = data;
			return;
		case 0xf3: /* DSPDATA */
			if (!(IAPU.RAM[0xf2] & 0x80)) /* 0x80-0xff is a read-only mirror of 0x00-0x7f */
				APUDSPIn(IAPU.RAM[0xf2] & 0x7f, data);

			IAPU.RAM[addr] = data;
			return;
		case 0xf4: /* CPUIO0 */
		case 0xf5: /* CPUIO1 */
		case 0xf6: /* CPUIO2 */
		case 0xf7: /* CPUIO3 */
			if (Settings.PAL && Settings.SecretOfEvermoreHack)
				IAPU.RAM[addr] = data;

			APU.OutPorts[addr & 3] = data;
			return;
		case 0xfa: /* T0TARGET */
		case 0xfb: /* T1TARGET */
		case 0xfc: /* T2TARGET */
			IAPU.RAM[addr] = data;
			APU.TimerTarget[addr - 0xfa] = (data == 0 ? 0x100 : data);
			return;
		case 0xfd: /* T0OUT */
		case 0xfe: /* T1OUT */
		case 0xff: /* T2OUT - read-only registers */
			return;
	}

	if (addr >= 0xffc0 && APU.ShowROM)
		return;

	if ((IAPU.RAM[0xf0] & 0x02) && !(IAPU.RAM[0xf0] & 0x04)) /* writes to $ffc0-$ffff always go to apuram, even if the iplrom is enabled */
		IAPU.RAM[addr] = data;
}
