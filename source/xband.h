#ifndef CHIMERASNES_XBAND_H_
#define CHIMERASNES_XBAND_H_

uint8_t GetXBAND(uint32_t Address);
void SetXBAND(uint8_t Byte, uint32_t Address);

static INLINE uint8_t* GetBasePointerXBAND(uint32_t Address)
{
	(void) Address;
	return Memory.SRAM;
}

static INLINE uint8_t* GetMemPointerXBAND(uint32_t Address)
{
	return Memory.SRAM + (Address & 0xFFFF);
}
#endif
