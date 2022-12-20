#include "chisnes.h"
#include "dsp.h"
#include "memmap.h"

uint8_t (*GetDSP)(uint16_t) = NULL;
void (*SetDSP)(uint8_t, uint16_t) = NULL;

void ResetDSP()
{
	memset(&DSP1, 0, sizeof(DSP1));
	DSP1.waiting4command = true;
	DSP1.first_parameter = true;
	memset(&DSP2, 0, sizeof(DSP2));
	DSP2.waiting4command = true;
	memset(&DSP3, 0, sizeof(DSP3));
	DSP3_Reset();
	memset(&DSP4, 0, sizeof(DSP4));
	DSP4.waiting4command = true;
}
