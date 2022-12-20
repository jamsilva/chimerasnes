#ifndef CHIMERASNES_MATH_H_
#define CHIMERASNES_MATH_H_

#include "port.h"

#define MATH_ABS(X) \
	((X) <  0  ? -(X) : (X))

#define MATH_MIN(A, B) \
	((A) < (B) ?  (A) : (B))

#define MATH_MAX(A, B) \
	((A) > (B) ?  (A) : (B))

extern const int16_t AtanTable[256];
extern const int16_t MulTable[256];
extern const int16_t SinTable[256];

int16_t math_atan2(int16_t x, int16_t y);
int16_t math_cos(int16_t angle);
int16_t math_sin(int16_t angle);
int32_t math_sqrt(int32_t val);
#endif
