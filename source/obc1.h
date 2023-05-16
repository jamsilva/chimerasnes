#ifndef CHIMERASNES_OBC1_H_
#define CHIMERASNES_OBC1_H_

typedef struct
{
	uint16_t address;
	uint16_t basePtr;
	uint16_t shift;
} SOBC1;

extern SOBC1 OBC1;

uint8_t  GetOBC1(uint16_t Address);
void     SetOBC1(uint8_t Byte, uint16_t Address);
void     ResetOBC1();
uint8_t* GetBasePointerOBC1(uint16_t Address);
uint8_t* GetMemPointerOBC1(uint16_t Address);
#endif
