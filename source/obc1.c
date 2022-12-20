#include "chisnes.h"
#include "memmap.h"
#include "obc1.h"

uint8_t GetOBC1(uint16_t Address)
{
	switch (Address)
	{
		case 0x7ff0:
		case 0x7ff1:
		case 0x7ff2:
		case 0x7ff3:
			return Memory.OBC1RAM[OBC1.basePtr + (OBC1.address << 2) + (Address & ~0x7ff0)];
		case 0x7ff4:
			return Memory.OBC1RAM[OBC1.basePtr + (OBC1.address >> 2) + 0x200];
	}

	return Memory.OBC1RAM[Address & 0x1fff];
}

void SetOBC1(uint8_t Byte, uint16_t Address)
{
	switch (Address)
	{
		case 0x7ff0:
		case 0x7ff1:
		case 0x7ff2:
		case 0x7ff3:
			Memory.OBC1RAM[OBC1.basePtr + (OBC1.address << 2) + (Address & ~0x7ff0)] = Byte;
			break;
		case 0x7ff4:
		{
			uint8_t Temp;
			Temp = Memory.OBC1RAM[OBC1.basePtr + (OBC1.address >> 2) + 0x200];
			Temp = (Temp & ~(3 << OBC1.shift)) | ((Byte & 3) << OBC1.shift);
			Memory.OBC1RAM[OBC1.basePtr + (OBC1.address >> 2) + 0x200] = Temp;
			break;
		}
		case 0x7ff5:
			if (Byte & 1)
				OBC1.basePtr = 0x1800;
			else
				OBC1.basePtr = 0x1c00;

			break;
		case 0x7ff6:
			OBC1.address = Byte & 0x7f;
			OBC1.shift = (Byte & 3) << 1;
			break;
	}

	Memory.OBC1RAM[Address & 0x1fff] = Byte;
}

void ResetOBC1()
{
	OBC1.address = 0;
	OBC1.basePtr = 0x1c00;
	OBC1.shift   = 0;
	memset(Memory.OBC1RAM, 0x00, 0x2000);
}

uint8_t* GetMemPointerOBC1(uint16_t Address)
{
	return Memory.FillRAM + (Address & 0xffff);
}
