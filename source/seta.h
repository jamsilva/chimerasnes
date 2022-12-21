#ifndef CHIMERASNES_SETA_H_
#define CHIMERASNES_SETA_H_

#include "port.h"

uint8_t NullGet(uint32_t Address);
void    NullSet(uint8_t Byte, uint32_t Address);
uint8_t GetST010(uint32_t Address);
void    SetST010(uint8_t Byte, uint32_t Address);

extern void    (*SetSETA)(uint8_t, uint32_t);
extern uint8_t (*GetSETA)(uint32_t);

typedef struct
{
	bool    control_enable : 1;
	int8_t  _SST010_PAD1   : 7;
	uint8_t execute;
	uint8_t op_reg;
	uint8_t input_params[16];
	uint8_t output_params[16];
} SST010;
#endif
