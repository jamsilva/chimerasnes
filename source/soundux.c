#include "chisnes.h"
#include "apu.h"
#include "soundux.h"

extern int8_t FilterTaps[8];

void SetFilterCoefficient(int32_t tap, int32_t value)
{
	FilterTaps[tap & 7] = value;
}
