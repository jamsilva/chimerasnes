#include "chisnes.h"
#include "math.h"
#include "memmap.h"
#include "ppu.h"

static int16_t CX4WFXVal;
static int16_t CX4WFYVal;
static int16_t CX4WFZVal;
static int16_t CX4WFX2Val;
static int16_t CX4WFY2Val;
static int16_t CX4WFDist;
static int16_t CX4WFScale;
static int16_t CX41FXVal;
static int16_t CX41FYVal;
static int16_t CX41FAngleRes;
static int16_t CX41FDist;
static int16_t CX41FDistVal;
static int32_t tanval;
static int32_t cx4x,  cx4y,  cx4z;
static int32_t cx4x2, cx4y2, cx4z2;

#define SIN_TO_COS_ANGLE(angle) \
	((angle + 0x7f) & 0x1ff)

static uint8_t CX4TestPattern[] =
{
	0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff,
	0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00,
	0xff, 0xff, 0x00, 0x00, 0x80, 0xff, 0xff, 0x7f,
	0x00, 0x80, 0x00, 0xff, 0x7f, 0x00, 0xff, 0x7f,
	0xff, 0x7f, 0xff, 0xff, 0x00, 0x00, 0x01, 0xff,
	0xff, 0xfe, 0x00, 0x01, 0x00, 0xff, 0xfe, 0x00
};

static INLINE int32_t SAR32(const int32_t b, const int32_t n)
{
	if (b < 0)
		return (b >> n) | (~0u << (32 - n));

	return b >> n;
}

static INLINE int64_t SAR64(const int64_t b, const int32_t n)
{
	if (b < 0)
		return (b >> n) | (~0u << (64 - n));

	return b >> n;
}

static void CX4TransfWireFrameCommon()
{
	cx4x = CX4WFXVal;
	cx4y = CX4WFYVal;

	/* Rotate X */
	tanval = -CX4WFX2Val << 9;
	cx4y2 = (cx4y * math_cos(tanval) - cx4z * math_sin(tanval)) >> 15;
	cx4z2 = (cx4y * math_sin(tanval) + cx4z * math_cos(tanval)) >> 15;

	/* Rotate Y */
	tanval = -CX4WFY2Val << 9;
	cx4x2 = (cx4x * math_cos(tanval) + cx4z2 * math_sin(tanval)) >> 15;
	cx4z = (cx4x * -math_sin(tanval) + cx4z2 * math_cos(tanval)) >> 15;

	/* Rotate Z */
	tanval = -CX4WFDist << 9;
	cx4x = (cx4x2 * math_cos(tanval) - cx4y2 * math_sin(tanval)) >> 15;
	cx4y = (cx4x2 * math_sin(tanval) + cx4y2 * math_cos(tanval)) >> 15;
}

static void CX4TransfWireFrame()
{
	int32_t scale, div;
	cx4z = CX4WFZVal - 0x95;
	CX4TransfWireFrameCommon();

	/* Scale */
	scale = (int32_t) CX4WFScale * 0x95;
	div = (0x90 * ((int32_t) cx4z + 0x95));
	CX4WFXVal = (int16_t) (cx4x * scale / div);
	CX4WFYVal = (int16_t) (cx4y * scale / div);
}

static void CX4TransfWireFrame2()
{
	cx4z = CX4WFZVal;
	CX4TransfWireFrameCommon();

	/* Scale */
	CX4WFXVal = (int16_t) (((int32_t) cx4x * CX4WFScale) >> 8);
	CX4WFYVal = (int16_t) (((int32_t) cx4y * CX4WFScale) >> 8);
}

static void CX4CalcWireFrame()
{
	CX4WFXVal = CX4WFX2Val - CX4WFXVal;
	CX4WFYVal = CX4WFY2Val - CX4WFYVal;

	if (MATH_ABS(CX4WFXVal) > MATH_ABS(CX4WFYVal))
	{
		CX4WFDist = MATH_ABS(CX4WFXVal) + 1;
		CX4WFYVal = (int16_t) (((int32_t) CX4WFYVal << 8) / MATH_ABS(CX4WFXVal));

		if (CX4WFXVal < 0)
			CX4WFXVal = -256;
		else
			CX4WFXVal = 256;
	}
	else if (CX4WFYVal != 0)
	{
		CX4WFDist = MATH_ABS(CX4WFYVal) + 1;
		CX4WFXVal = (int16_t) (((int32_t) CX4WFXVal << 8) / MATH_ABS(CX4WFYVal));

		if (CX4WFYVal < 0)
			CX4WFYVal = -256;
		else
			CX4WFYVal = 256;
	}
	else
		CX4WFDist = 0;
}

static void CX4ConvOAM()
{
	uint16_t globalX, globalY;
	uint8_t* OAMptr2;
	int16_t SprX, SprY;
	uint8_t SprName, SprAttr, SprCount, offset;
	uint8_t* m;
	int32_t prio, i;
	uint8_t* OAMptr = Memory.CX4RAM + (Memory.CX4RAM[0x626] << 2);

	for (m = Memory.CX4RAM + 0x1fd; m > OAMptr; m -= 4)
		*m = 0xe0; /* Clear OAM-to-be */

	if (Memory.CX4RAM[0x0620] == 0)
		return;

	globalX = READ_WORD(Memory.CX4RAM + 0x0621);
	globalY = READ_WORD(Memory.CX4RAM + 0x0623);
	OAMptr2 = Memory.CX4RAM + 0x200 + (Memory.CX4RAM[0x626] >> 2);
	SprCount = 128 - Memory.CX4RAM[0x626];
	offset = (Memory.CX4RAM[0x626] & 3) << 1;

	for (prio = 0x30; prio >= 0; prio -= 0x10)
	{
		uint8_t* srcptr = Memory.CX4RAM + 0x220;

		for (i = Memory.CX4RAM[0x0620]; i > 0 && SprCount > 0; i--, srcptr += 16)
		{
			uint8_t* sprptr;

			if ((srcptr[4] & 0x30) != prio)
				continue;

			SprX = READ_WORD(srcptr) - globalX;
			SprY = READ_WORD(srcptr + 2) - globalY;
			SprName = srcptr[5];
			SprAttr = srcptr[4] | srcptr[0x06];
			sprptr = GetMemPointer(READ_3WORD(srcptr + 7));

			if (*sprptr != 0)
			{
				int32_t SprCnt;
				int16_t X, Y;

				for (SprCnt = *sprptr++; SprCnt > 0 && SprCount > 0; SprCnt--, sprptr += 4)
				{
					X = (int8_t) sprptr[1];

					if (SprAttr & 0x40) /* flip X */
						X = -X - ((sprptr[0] & 0x20) ? 16 : 8);

					X += SprX;

					if (X < -16 || X > 272)
						continue;

					Y = (int8_t) sprptr[2];

					if (SprAttr & 0x80)
						Y = -Y - ((sprptr[0] & 0x20) ? 16 : 8);

					Y += SprY;

					if (Y < -16 || Y > 224)
						continue;

					OAMptr[0] = X & 0xff;
					OAMptr[1] = (uint8_t) Y;
					OAMptr[2] = SprName + sprptr[3];
					OAMptr[3] = SprAttr ^ (sprptr[0] & 0xc0);
					*OAMptr2 &= ~(3 << offset);

					if (X & 0x100)
						*OAMptr2 |= 1 << offset;

					if (sprptr[0] & 0x20)
						*OAMptr2 |= 2 << offset;

					OAMptr += 4;
					SprCount--;
					offset = (offset + 2) & 6;

					if (offset == 0)
						OAMptr2++;
				}
			}
			else if (SprCount > 0)
			{
				OAMptr[0] = (uint8_t) SprX;
				OAMptr[1] = (uint8_t) SprY;
				OAMptr[2] = SprName;
				OAMptr[3] = SprAttr;
				*OAMptr2 &= ~(3 << offset);

				if (SprX & 0x100)
					*OAMptr2 |= 3 << offset;
				else
					*OAMptr2 |= 2 << offset;

				OAMptr += 4;
				SprCount--;
				offset = (offset + 2) & 6;

				if (offset == 0)
					OAMptr2++;
			}
		}
	}
}

static void CX4DoScaleRotate(int32_t row_padding)
{
	int16_t  A, B, C, D;
	uint8_t  w, h;
	int32_t  Cx, Cy;
	int32_t  LineX, LineY;
	uint32_t X, Y;
	int32_t  outidx = 0;
	int32_t  x, y;
	uint8_t  bit = 0x80;
	int32_t  XScale = READ_WORD(Memory.CX4RAM + 0x1f8f);
	int32_t  YScale = READ_WORD(Memory.CX4RAM + 0x1f92);

	/* Calculate matrix */
	if (XScale & 0x8000)
		XScale = 0x7fff;

	if (YScale & 0x8000)
		YScale = 0x7fff;

	if (READ_WORD(Memory.CX4RAM + 0x1f80) == 0) /* no rotation */
	{
		A = (int16_t) XScale;
		B = 0;
		C = 0;
		D = (int16_t) YScale;
	}
	else if (READ_WORD(Memory.CX4RAM + 0x1f80) == 128) /* 90 degree rotation */
	{
		A = 0;
		B = (int16_t) -YScale;
		C = (int16_t)  XScale;
		D = 0;
	}
	else if (READ_WORD(Memory.CX4RAM + 0x1f80) == 256) /* 180 degree rotation */
	{
		A = (int16_t) -XScale;
		B = 0;
		C = 0;
		D = (int16_t) -YScale;
	}
	else if (READ_WORD(Memory.CX4RAM + 0x1f80) == 384) /* 270 degree rotation */
	{
		A = 0;
		B = (int16_t)  YScale;
		C = (int16_t) -XScale;
		D = 0;
	}
	else
	{
		int16_t AngleS = (READ_WORD(Memory.CX4RAM + 0x1f80) + 1) & 0x1ff;
		int16_t AngleC = SIN_TO_COS_ANGLE(AngleS) >> 1;
		AngleS >>= 1;
		A = (int16_t)   SAR32(SinTable[AngleC] * XScale, 15);
		B = (int16_t) (-SAR32(SinTable[AngleS] * YScale, 15));
		C = (int16_t)   SAR32(SinTable[AngleS] * XScale, 15);
		D = (int16_t)   SAR32(SinTable[AngleC] * YScale, 15);
	}

	/* Calculate Pixel Resolution */
	w = Memory.CX4RAM[0x1f89] & ~7;
	h = Memory.CX4RAM[0x1f8c] & ~7;

	/* Clear the output RAM */
	memset(Memory.CX4RAM, 0, (w + row_padding / 4) * h / 2);
	Cx = (int16_t) READ_WORD(Memory.CX4RAM + 0x1f83);
	Cy = (int16_t) READ_WORD(Memory.CX4RAM + 0x1f86);

	/* Calculate start position (i.e. (Ox, Oy) = (0, 0))
	   The low 12 bits are fractional, so (Cx<<12) gives us the Cx we want in
	   the function. We do Cx*A etc normally because the matrix parameters
	   already have the fractional parts. */
	LineX = (Cx << 12) - Cx * A - Cx * B;
	LineY = (Cy << 12) - Cy * C - Cy * D;

	for (y = 0; y < h; y++) /* Start loop */
	{
		X = LineX;
		Y = LineY;

		for (x = 0; x < w; x++)
		{
			if ((X >> 12) < w && (Y >> 12) < h)
			{
				uint32_t addr = (Y >> 12) * w + (X >> 12);
				uint8_t byte = Memory.CX4RAM[0x600 + (addr >> 1)];

				if (addr & 1)
					byte >>= 4;

				/* De-bitplanify */
				if (byte & 1)
					Memory.CX4RAM[outidx] |= bit;

				if (byte & 2)
					Memory.CX4RAM[outidx + 1] |= bit;

				if (byte & 4)
					Memory.CX4RAM[outidx + 16] |= bit;

				if (byte & 8)
					Memory.CX4RAM[outidx + 17] |= bit;
			}

			bit >>= 1;

			if (bit == 0)
			{
				bit = 0x80;
				outidx += 32;
			}

			X += A; /* Add 1 to output x => add an A and a C */
			Y += C;
		}

		outidx += 2 + row_padding;

		if (outidx & 0x10)
			outidx &= ~0x10;
		else
			outidx -= w * 4 + row_padding;

		LineX += B; /* Add 1 to output y => add a B and a D */
		LineY += D;
	}
}

static void CX4DrawLine(int32_t X1, int32_t Y1, int16_t Z1, int32_t X2, int32_t Y2, int16_t Z2, uint8_t Color)
{
	int32_t i;

	/* Transform coordinates */
	CX4WFXVal = (int16_t) X1;
	CX4WFYVal = (int16_t) Y1;
	CX4WFZVal = Z1;
	CX4WFScale = Memory.CX4RAM[0x1f90];
	CX4WFX2Val = Memory.CX4RAM[0x1f86];
	CX4WFY2Val = Memory.CX4RAM[0x1f87];
	CX4WFDist = Memory.CX4RAM[0x1f88];
	CX4TransfWireFrame2();
	X1 = (CX4WFXVal + 48) << 8;
	Y1 = (CX4WFYVal + 48) << 8;
	CX4WFXVal = (int16_t) X2;
	CX4WFYVal = (int16_t) Y2;
	CX4WFZVal = Z2;
	CX4TransfWireFrame2();
	X2 = (CX4WFXVal + 48) << 8;
	Y2 = (CX4WFYVal + 48) << 8;

	/* Get line info */
	CX4WFXVal  = (int16_t) (X1 >> 8);
	CX4WFYVal  = (int16_t) (Y1 >> 8);
	CX4WFX2Val = (int16_t) (X2 >> 8);
	CX4WFY2Val = (int16_t) (Y2 >> 8);
	CX4CalcWireFrame();
	X2 = (int16_t) CX4WFXVal;
	Y2 = (int16_t) CX4WFYVal;

	/* Render line */
	for (i = CX4WFDist ? CX4WFDist : 1; i > 0; i--) /*.loop */
	{
		if (X1 > 0xff && Y1 > 0xff && X1 < 0x6000 && Y1 < 0x6000)
		{
			uint16_t addr = (((Y1 >> 8) >> 3) << 8) - (((Y1 >> 8) >> 3) << 6) + (((X1 >> 8) >> 3) << 4) + ((Y1 >> 8) & 7) * 2;
			uint8_t bit = 0x80 >> ((X1 >> 8) & 7);
			Memory.CX4RAM[addr + 0x300] &= ~bit;
			Memory.CX4RAM[addr + 0x301] &= ~bit;

			if (Color & 1)
				Memory.CX4RAM[addr + 0x300] |= bit;

			if (Color & 2)
				Memory.CX4RAM[addr + 0x301] |= bit;
		}

		X1 += X2;
		Y1 += Y2;
	}
}

static void CX4DrawWireFrame()
{
	uint8_t* line = GetMemPointer(READ_3WORD(Memory.CX4RAM + 0x1f80));
	uint8_t* point1;
	uint8_t* point2;
	int16_t X1, Y1, Z1;
	int16_t X2, Y2, Z2;
	uint8_t Color;
	int32_t i;

	for (i = Memory.CX4RAM[0x0295]; i > 0; i--, line += 5)
	{
		if (line[0] == 0xff && line[1] == 0xff)
		{
			uint8_t* tmp = line - 5;

			while (tmp[2] == 0xff && tmp[3] == 0xff)
				tmp -= 5;

			point1 = GetMemPointer((Memory.CX4RAM[0x1f82] << 16) | (tmp[2] << 8) | tmp[3]);
		}
		else
			point1 = GetMemPointer((Memory.CX4RAM[0x1f82] << 16) | (line[0] << 8) | line[1]);

		point2 = GetMemPointer((Memory.CX4RAM[0x1f82] << 16) | (line[2] << 8) | line[3]);
		X1 = (point1[0] << 8) | point1[1];
		Y1 = (point1[2] << 8) | point1[3];
		Z1 = (point1[4] << 8) | point1[5];
		X2 = (point2[0] << 8) | point2[1];
		Y2 = (point2[2] << 8) | point2[3];
		Z2 = (point2[4] << 8) | point2[5];
		Color = line[4];
		CX4DrawLine(X1, Y1, Z1, X2, Y2, Z2, Color);
	}
}

static void CX4TransformLines()
{
	int32_t i;
	uint8_t* ptr = Memory.CX4RAM;
	uint8_t* ptr2;
	CX4WFX2Val = Memory.CX4RAM[0x1f83];
	CX4WFY2Val = Memory.CX4RAM[0x1f86];
	CX4WFDist = Memory.CX4RAM[0x1f89];
	CX4WFScale = Memory.CX4RAM[0x1f8c];

	/* transform vertices */

	for (i = READ_WORD(Memory.CX4RAM + 0x1f80); i > 0; i--, ptr += 0x10)
	{
		CX4WFXVal = READ_WORD(ptr + 1);
		CX4WFYVal = READ_WORD(ptr + 5);
		CX4WFZVal = READ_WORD(ptr + 9);
		CX4TransfWireFrame();

		/* displace */
		WRITE_WORD(ptr + 1, CX4WFXVal + 0x80);
		WRITE_WORD(ptr + 5, CX4WFYVal + 0x50);
	}

	WRITE_WORD(Memory.CX4RAM + 0x600, 23);
	WRITE_WORD(Memory.CX4RAM + 0x602, 0x60);
	WRITE_WORD(Memory.CX4RAM + 0x605, 0x40);
	WRITE_WORD(Memory.CX4RAM + 0x600 + 8, 23);
	WRITE_WORD(Memory.CX4RAM + 0x602 + 8, 0x60);
	WRITE_WORD(Memory.CX4RAM + 0x605 + 8, 0x40);
	ptr = Memory.CX4RAM + 0xb02;
	ptr2 = Memory.CX4RAM;

	for (i = READ_WORD(Memory.CX4RAM + 0xb00); i > 0; i--, ptr += 2, ptr2 += 8)
	{
		CX4WFXVal = READ_WORD(Memory.CX4RAM + (ptr[0] << 4) + 1);
		CX4WFYVal = READ_WORD(Memory.CX4RAM + (ptr[0] << 4) + 5);
		CX4WFX2Val = READ_WORD(Memory.CX4RAM + (ptr[1] << 4) + 1);
		CX4WFY2Val = READ_WORD(Memory.CX4RAM + (ptr[1] << 4) + 5);
		CX4CalcWireFrame();
		WRITE_WORD(ptr2 + 0x600, CX4WFDist ? CX4WFDist : 1);
		WRITE_WORD(ptr2 + 0x602, CX4WFXVal);
		WRITE_WORD(ptr2 + 0x605, CX4WFYVal);
	}
}

static INLINE uint16_t bmpData(uint16_t index)
{
	return ((index & 7u) << 1) | ((index & ~7u) << 6);
}

static void CX4BitPlaneWave()
{
	uint8_t* dst = Memory.CX4RAM;
	uint32_t waveptr = Memory.CX4RAM[0x1f83];
	uint16_t mask1 = 0xc0c0;
	uint16_t mask2 = 0x3f3f;
	uint16_t i, j;

	for (j = 0; j < 0x20; j++, dst += 16)
	{
		uint8_t* base = Memory.CX4RAM + (0xa00 | ((j & 1) << 4));

		do
		{
			int16_t height = (-((int8_t) Memory.CX4RAM[waveptr + 0xb00]) - 16) * 2;

			for (i = 0; i < 40; i++)
			{
				uint16_t tmp = READ_WORD(dst + bmpData(i)) & mask2;

				if (height >= 0)
				{
					if (height < 16)
						tmp |= mask1 & READ_WORD(base + height);
					else
						tmp |= mask1 & 0xff00;
				}

				WRITE_WORD(dst + bmpData(i), tmp);
				height += 2;
			}

			waveptr = (waveptr + 1) & 0x7f;
			mask1 = (mask1 >> 2) | (mask1 << 6);
			mask2 = (mask2 >> 2) | (mask2 << 6);
		} while (mask1 != 0xc0c0);
	}
}

static void CX4SprDisintegrate()
{
	uint32_t x, y, i, j;
	uint8_t width = Memory.CX4RAM[0x1f89];
	uint8_t height = Memory.CX4RAM[0x1f8c];
	int32_t Cx = (int16_t) READ_WORD(Memory.CX4RAM + 0x1f80);
	int32_t Cy = (int16_t) READ_WORD(Memory.CX4RAM + 0x1f83);
	int32_t scaleX = (int16_t) READ_WORD(Memory.CX4RAM + 0x1f86);
	int32_t scaleY = (int16_t) READ_WORD(Memory.CX4RAM + 0x1f8f);
	uint32_t StartX = -Cx * scaleX + (Cx << 8);
	uint32_t StartY = -Cy * scaleY + (Cy << 8);
	uint8_t* src = Memory.CX4RAM + 0x600;
	memset(Memory.CX4RAM, 0, width * height / 2);

	for (y = StartY, i = 0; i < height; i++, y += scaleY)
	{
		for (x = StartX, j = 0; j < width; j++, x += scaleX)
		{
			if ((x >> 8) < width && (y >> 8) < height && (y >> 8) * width + (x >> 8) < 0x2000)
			{
				uint8_t pixel = (j & 1) ? (*src >> 4) : *src;
				int32_t idx = (y >> 11) * width * 4 + (x >> 11) * 32 + ((y >> 8) & 7) * 2;
				uint8_t mask = 0x80 >> ((x >> 8) & 7);

				if (pixel & 1)
					Memory.CX4RAM[idx] |= mask;

				if (pixel & 2)
					Memory.CX4RAM[idx + 1] |= mask;

				if (pixel & 4)
					Memory.CX4RAM[idx + 16] |= mask;

				if (pixel & 8)
					Memory.CX4RAM[idx + 17] |= mask;
			}

			if (j & 1)
				src++;
		}
	}
}

static void CX4ProcessSprites()
{
	switch (Memory.CX4RAM[0x1f4d])
	{
		case 0x00: /* Build OAM */
			CX4ConvOAM();
			break;
		case 0x03: /* Scale/Rotate */
			CX4DoScaleRotate(0);
			break;
		case 0x05: /* Transform Lines */
			CX4TransformLines();
			break;
		case 0x07: /* Scale/Rotate */
			CX4DoScaleRotate(64);
			break;
		case 0x08: /* Draw wireframe */
			CX4DrawWireFrame();
			break;
		case 0x0b: /* Disintegrate */
			CX4SprDisintegrate();
			break;
		case 0x0c: /* Wave */
			CX4BitPlaneWave();
			break;
		default:
			break;
	}
}

void InitCX4()
{
	Memory.CX4RAM = &Memory.FillRAM [0x6000];
}

uint8_t GetCX4(uint16_t Address)
{
	if (Address == 0x7f5e)
		return 0;

	return Memory.CX4RAM[Address - 0x6000];
}

void SetCX4(uint8_t byte, uint16_t Address)
{
	int32_t i, tmp;
	Memory.CX4RAM[Address - 0x6000] = byte;

	if (Address == 0x7f47)
	{
		/* memmove required: Can overlap arbitrarily [Neb] */
		memmove(Memory.CX4RAM + (READ_WORD(Memory.CX4RAM + 0x1f45) & 0x1fff), GetMemPointer(READ_3WORD(Memory.CX4RAM + 0x1f40)), READ_WORD(Memory.CX4RAM + 0x1f43));
		return;
	}

	if (Address != 0x7f4f)
		return;

	if (Memory.CX4RAM[0x1f4d] == 0x0e && byte < 0x40 && (byte & 3) == 0)
	{
		Memory.CX4RAM[0x1f80] = byte >> 2;
		return;
	}

	switch (byte)
	{
		case 0x00: /* Sprite */
			CX4ProcessSprites();
			break;
		case 0x01: /* Draw wireframe */
			memset(Memory.CX4RAM + 0x300, 0, 16 * 12 * 3 * 4);
			CX4DrawWireFrame();
			break;
		case 0x05: /* Propulsion (?) */
			tmp = 0x10000;

			if (READ_WORD(Memory.CX4RAM + 0x1f83))
				tmp = SAR32((tmp / READ_WORD(Memory.CX4RAM + 0x1f83)) * READ_WORD(Memory.CX4RAM + 0x1f81), 8);

			WRITE_WORD(Memory.CX4RAM + 0x1f80, (uint16_t) tmp);
			break;
		case 0x0d: /* Set vector length */
			CX41FXVal = READ_WORD(Memory.CX4RAM + 0x1f80);
			CX41FYVal = READ_WORD(Memory.CX4RAM + 0x1f83);
			CX41FDistVal = READ_WORD(Memory.CX4RAM + 0x1f86);
			tanval = CX41FDistVal / math_sqrt((int32_t) CX41FYVal * CX41FYVal + (int32_t) CX41FXVal * CX41FXVal);
			CX41FYVal = (int16_t) (((int32_t) CX41FYVal * tanval * 99) / 100);
			CX41FXVal = (int16_t) (((int32_t) CX41FXVal * tanval * 98) / 100);
			WRITE_WORD(Memory.CX4RAM + 0x1f89, CX41FXVal);
			WRITE_WORD(Memory.CX4RAM + 0x1f8c, CX41FYVal);
			break;
		case 0x10: /* Polar to rectangular */
		{
			int16_t AngleS = (READ_WORD(Memory.CX4RAM + 0x1f80) + 1) & 0x1ff;
			int16_t AngleC = SIN_TO_COS_ANGLE(AngleS) >> 1;
			int32_t r1 = READ_WORD(Memory.CX4RAM + 0x1f83);
			AngleS >>= 1;

			if (r1 & 0x8000)
				r1 |= ~0x7fff;
			else
				r1 &= 0x7fff;

			r1 <<= 1;

			tmp = SAR32(r1 * SinTable[AngleC], 16);
			WRITE_3WORD(Memory.CX4RAM + 0x1f86, tmp);
			tmp = SAR32(r1 * SinTable[AngleS], 16);
			WRITE_3WORD(Memory.CX4RAM + 0x1f89, (tmp - SAR32(tmp, 6)));
			break;
		}
		case 0x13: /* Polar to rectangular */
		{
			int16_t AngleS = (READ_WORD(Memory.CX4RAM + 0x1f80) + 1) & 0x1ff;
			int16_t AngleC = SIN_TO_COS_ANGLE(AngleS) >> 1;
			AngleS >>= 1;
			tmp = SAR32((int32_t) READ_WORD(Memory.CX4RAM + 0x1f83) * SinTable[AngleC] * 2, 8);
			WRITE_3WORD(Memory.CX4RAM + 0x1f86, tmp);
			tmp = SAR32((int32_t) READ_WORD(Memory.CX4RAM + 0x1f83) * SinTable[AngleS] * 2, 8);
			WRITE_3WORD(Memory.CX4RAM + 0x1f89, tmp);
			break;
		}
		case 0x15: /* Pythagorean */
			CX41FXVal = READ_WORD(Memory.CX4RAM + 0x1f80);
			CX41FYVal = READ_WORD(Memory.CX4RAM + 0x1f83);
			CX41FDist = (int16_t) math_sqrt((int32_t) CX41FXVal * CX41FXVal + (int32_t) CX41FYVal * CX41FYVal);
			WRITE_WORD(Memory.CX4RAM + 0x1f80, CX41FDist);
			break;
		case 0x1f: /* atan */
			CX41FXVal = READ_WORD(Memory.CX4RAM + 0x1f80);
			CX41FYVal = READ_WORD(Memory.CX4RAM + 0x1f83);

			if (CX41FXVal != 0)
			{
				CX41FAngleRes = math_atan2(CX41FYVal, CX41FXVal) / 2;

				if (CX41FXVal < 0)
					CX41FAngleRes += 0x100;

				CX41FAngleRes &= 0x1FF;
			}
			else if (CX41FYVal > 0)
				CX41FAngleRes = 0x80;
			else
				CX41FAngleRes = 0x180;

			WRITE_WORD(Memory.CX4RAM + 0x1f86, CX41FAngleRes);
			break;
		case 0x22: /* Trapezoid */
		{
			int16_t angleS1 = (READ_WORD(Memory.CX4RAM + 0x1f8c) + 1) & 0x1ff;
			int16_t angleS2 = (READ_WORD(Memory.CX4RAM + 0x1f8f) + 1) & 0x1ff;
			int16_t angleC1 = SIN_TO_COS_ANGLE(angleS1) >> 1;
			int16_t angleC2 = SIN_TO_COS_ANGLE(angleS2) >> 1;
			int32_t j, tan1, tan2;
			int16_t y, left, right;
			angleS1 >>= 1;
			angleS2 >>= 1;
			tan1 = (SinTable[angleC1] != 0) ? ((((int32_t) SinTable[angleS1]) << 16) / SinTable[angleC1]) : (int32_t) 0x80000000;
			tan2 = (SinTable[angleC2] != 0) ? ((((int32_t) SinTable[angleS2]) << 16) / SinTable[angleC2]) : (int32_t) 0x80000000;
			y = READ_WORD(Memory.CX4RAM + 0x1f83) - READ_WORD(Memory.CX4RAM + 0x1f89);

			for (j = 0; j < 225; j++)
			{
				if (y >= 0)
				{
					left  = SAR32((int32_t) tan1 * y, 16) - READ_WORD(Memory.CX4RAM + 0x1f80) + READ_WORD(Memory.CX4RAM + 0x1f86);
					right = SAR32((int32_t) tan2 * y, 16) - READ_WORD(Memory.CX4RAM + 0x1f80) + READ_WORD(Memory.CX4RAM + 0x1f86) + READ_WORD(Memory.CX4RAM + 0x1f93);

					if (left < 0 && right < 0)
					{
						left = 1;
						right = 0;
					}
					else if (left < 0)
						left = 0;
					else if (right < 0)
						right = 0;

					if (left > 255 && right > 255)
					{
						left = 255;
						right = 254;
					}
					else if (left > 255)
						left = 255;
					else if (right > 255)
						right = 255;
				}
				else
				{
					left = 1;
					right = 0;
				}

				Memory.CX4RAM[j + 0x800] = (uint8_t) left;
				Memory.CX4RAM[j + 0x900] = (uint8_t) right;
				y++;
			}

			break;
		}
		case 0x25: /* Multiply */
		{
			int32_t foo = READ_3WORD(Memory.CX4RAM + 0x1f80);
			int32_t bar = READ_3WORD(Memory.CX4RAM + 0x1f83);
			foo *= bar;
			WRITE_3WORD(Memory.CX4RAM + 0x1f80, foo);
			break;
		}
		case 0x2d: /* Transform Coords */
			CX4WFXVal = READ_WORD(Memory.CX4RAM + 0x1f81);
			CX4WFYVal = READ_WORD(Memory.CX4RAM + 0x1f84);
			CX4WFZVal = READ_WORD(Memory.CX4RAM + 0x1f87);
			CX4WFX2Val = Memory.CX4RAM[0x1f89];
			CX4WFY2Val = Memory.CX4RAM[0x1f8a];
			CX4WFDist = Memory.CX4RAM[0x1f8b];
			CX4WFScale = READ_WORD(Memory.CX4RAM + 0x1f90);
			CX4TransfWireFrame2();
			WRITE_WORD(Memory.CX4RAM + 0x1f80, CX4WFXVal);
			WRITE_WORD(Memory.CX4RAM + 0x1f83, CX4WFYVal);
			break;
		case 0x40: /* Sum */
		{
			uint16_t sum = 0;

			for (i = 0; i < 0x800; sum += Memory.CX4RAM[i++]);

			WRITE_WORD(Memory.CX4RAM + 0x1f80, sum);
			break;
		}
		case 0x54: /* Square */
		{
			int64_t a = SAR64((int64_t) READ_3WORD(Memory.CX4RAM + 0x1f80) << 40, 10);
			a *= a;
			WRITE_3WORD(Memory.CX4RAM + 0x1f83, a);
			WRITE_3WORD(Memory.CX4RAM + 0x1f86, (a >> 24));
			break;
		}
		case 0x5c: /* Immediate Reg */
			memcpy(Memory.CX4RAM, CX4TestPattern, sizeof(CX4TestPattern));
			break;
		case 0x89: /* Immediate ROM */
			Memory.CX4RAM[0x1f80] = 0x36;
			Memory.CX4RAM[0x1f81] = 0x43;
			Memory.CX4RAM[0x1f82] = 0x05;
			break;
		default:
			break;
	}
}

uint8_t* GetBasePointerCX4(uint16_t Address)
{
	if (Address >= 0x7f40 && Address <= 0x7f5e)
		return NULL;

	return &Memory.CX4RAM[-0x6000];
}

uint8_t* GetMemPointerCX4(uint16_t Address)
{
	if (Address >= 0x7f40 && Address <= 0x7f5e)
		return NULL;

	return &Memory.CX4RAM[(Address & 0xffff) - 0x6000];
}
