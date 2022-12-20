#include "chisnes.h"
#include "memmap.h"
#include "xband.h"

uint8_t GetXBAND(uint32_t Address)
{
	switch (Address & 0xF0FFFF)
	{
		case 0xF0C1B2:
			return 0x46;
		case 0xF0C1B6:
		case 0xF0C1BA:
			return Memory.SRAM[Address & 0xFFFF] & 0x7F;
		case 0xF0C1BE:
			return Memory.SRAM[Address & 0xFFFF] & 0xFE;
		default:
			return Memory.SRAM[Address & 0xFFFF];
	}
}

void SetXBAND(uint8_t Byte, uint32_t Address)
{
	switch (Address & 0xF0FFFF)
	{
		case 0xF0FC02:
		case 0xF0FE00:
			return;
		default:
			Memory.SRAM[Address & 0xFFFF] = Byte;
			return;
	}
}
