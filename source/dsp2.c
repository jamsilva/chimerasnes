#include "chisnes.h"
#include "dsp.h"
#include "memmap.h"

static void DSP2_Op01();
static void DSP2_Op05();
static void DSP2_Op0D();

static void DSP2_Op01() /* convert bitmap to bitplane tile */
{
	/* Op01 size is always 32 bytes input and output. The hardware does strange things if you vary the size */
	int32_t j;
	uint8_t c0, c1, c2, c3;
	uint8_t* p1 = DSP2.parameters;
	uint8_t* p2a = DSP2.output;
	uint8_t* p2b = &DSP2.output[16]; /* halfway */

	for (j = 0; j < 8; j++) /* Process 8 blocks of 4 bytes each */
	{
		c0 = *p1++;
		c1 = *p1++;
		c2 = *p1++;
		c3 = *p1++;
		*p2a++ = (c0 & 0x10) << 3 | (c0 & 0x01) << 6 | (c1 & 0x10) << 1 | (c1 & 0x01) << 4 | (c2 & 0x10) >> 1 | (c2 & 0x01) << 2 | (c3 & 0x10) >> 3 | (c3 & 0x01);
		*p2a++ = (c0 & 0x20) << 2 | (c0 & 0x02) << 5 | (c1 & 0x20) | (c1 & 0x02) << 3 | (c2 & 0x20) >> 2 | (c2 & 0x02) << 1 | (c3 & 0x20) >> 4 | (c3 & 0x02) >> 1;
		*p2b++ = (c0 & 0x40) << 1 | (c0 & 0x04) << 4 | (c1 & 0x40) >> 1 | (c1 & 0x04) << 2 | (c2 & 0x40) >> 3 | (c2 & 0x04) | (c3 & 0x40) >> 5 | (c3 & 0x04) >> 2;
		*p2b++ = (c0 & 0x80) | (c0 & 0x08) << 3 | (c1 & 0x80) >> 2 | (c1 & 0x08) << 1 | (c2 & 0x80) >> 4 | (c2 & 0x08) >> 1 | (c3 & 0x80) >> 6 | (c3 & 0x08) >> 3;
	}
}

static void DSP2_Op05() /* replace bitmap using transparent color */
{
	/* Overlay bitmap with transparency.
	   Input:
	     Bitmap 1:  i[0] <=> i[size-1]
	     Bitmap 2:  i[size] <=> i[2*size-1]

	   Output:
	     Bitmap 3:  o[0] <=> o[size-1]

	   Processing: Process all 4-bit pixels (nibbles) in the bitmap
	     if ( BM2_pixel == transparent_color )
	        pixelout = BM1_pixel
	     else
	        pixelout = BM2_pixel */

	/* The max size bitmap is limited to 255 because the size parameter is a byte
	   I think size=0 is an error.  The behavior of the chip on size=0 is to
	   return the last value written to DR if you read DR on Op05 with
	   size = 0.  I don't think it's worth implementing this quirk unless it's
	   proven necessary. */
	uint8_t  c1, c2;
	int32_t  n;
	uint8_t* p1    = DSP2.parameters;
	uint8_t* p2    = &DSP2.parameters[DSP2.Op05Len];
	uint8_t* p3    = DSP2.output;
	uint8_t  color = DSP2.Op05Transparent & 0x0f;

	for (n = 0; n < DSP2.Op05Len; n++)
	{
		c1 = *p1++;
		c2 = *p2++;
		*p3++ = (((c2 >> 4) == color) ? c1 & 0xf0 : c2 & 0xf0) | (((c2 & 0x0f) == color) ? c1 & 0x0f : c2 & 0x0f);
	}
}

static void DSP2_Op0D() /* scale bitmap */
{
	int32_t i;

	for (i = 0 ; i < DSP2.Op0DOutLen ; i++) /* (Modified) Overload's algorithm */
	{
		int32_t j = i << 1;
		int32_t pixel_offset_low = ((j * DSP2.Op0DInLen) / DSP2.Op0DOutLen) >> 1;
		int32_t pixel_offset_high = (((j + 1) * DSP2.Op0DInLen) / DSP2.Op0DOutLen) >> 1;
		uint8_t pixel_low = DSP2.parameters[pixel_offset_low] >> 4;
		uint8_t pixel_high = DSP2.parameters[pixel_offset_high] & 0x0f;
		DSP2.output[i] = (pixel_low << 4) | pixel_high;
	}
}

void DSP2SetByte(uint8_t byte, uint16_t address)
{
	if ((address & 0xf000) != 0x6000 && (address < 0x8000 || address >= 0xc000))
		return;

	if (DSP2.waiting4command)
	{
		DSP2.command = byte;
		DSP2.in_index = 0;
		DSP2.waiting4command = false;

		switch (byte)
		{
			case 0x03:
			case 0x05:
			case 0x06:
				DSP2.in_count = 1;
				break;
			case 0x0D:
				DSP2.in_count = 2;
				break;
			case 0x09:
				DSP2.in_count = 4;
				break;
			case 0x01:
				DSP2.in_count = 32;
				break;
			default:
				DSP2.in_count = 0;
				break;
		}
	}
	else
	{
		DSP2.parameters[DSP2.in_index] = byte;
		DSP2.in_index++;
	}

	if (DSP2.in_count != DSP2.in_index)
		return;

	DSP2.waiting4command = true;
	DSP2.out_index = 0;

	switch (DSP2.command)
	{
		case 0x01:
			DSP2.out_count = 32;
			DSP2_Op01();
			break;
		case 0x03:
			DSP2.Op05Transparent = DSP2.parameters[0]; /* set transparent color */
			break;
		case 0x05:
			if (DSP2.Op05HasLen)
			{
				DSP2.Op05HasLen = false;
				DSP2.out_count = DSP2.Op05Len;
				DSP2_Op05();
			}
			else
			{
				DSP2.Op05Len = DSP2.parameters[0];
				DSP2.in_index = 0;
				DSP2.in_count = 2 * DSP2.Op05Len;
				DSP2.Op05HasLen = true;

				if (byte)
					DSP2.waiting4command = false;
			}

			break;
		case 0x06:
			if (DSP2.Op06HasLen)
			{
				int32_t i, j;
				DSP2.Op06HasLen = false;
				DSP2.out_count = DSP2.Op06Len;

				for (i = 0, j = DSP2.Op06Len - 1; i < DSP2.Op06Len; i++, j--) /* Reverse bitmap. Input: size, bitmap */
					DSP2.output[j] = (DSP2.parameters[i] << 4) | (DSP2.parameters[i] >> 4);
			}
			else
			{
				DSP2.Op06Len = DSP2.parameters[0];
				DSP2.in_index = 0;
				DSP2.in_count = DSP2.Op06Len;
				DSP2.Op06HasLen = true;

				if (byte)
					DSP2.waiting4command = false;
			}

			break;
		case 0x09: /* Multiply */
		{
			uint32_t temp;
			DSP2.Op09Word1 = DSP2.parameters[0] | (DSP2.parameters[1] << 8);
			DSP2.Op09Word2 = DSP2.parameters[2] | (DSP2.parameters[3] << 8);
			DSP2.out_count = 4;
			temp = DSP2.Op09Word1 * DSP2.Op09Word2;
			DSP2.output[0] = temp & 0xFF;
			DSP2.output[1] = (temp >> 8) & 0xFF;
			DSP2.output[2] = (temp >> 16) & 0xFF;
			DSP2.output[3] = (temp >> 24) & 0xFF;
			break;
		}
		case 0x0D:
			if (DSP2.Op0DHasLen)
			{
				DSP2.Op0DHasLen = false;
				DSP2.out_count = DSP2.Op0DOutLen;
				DSP2_Op0D();
			}
			else
			{
				DSP2.Op0DInLen = DSP2.parameters[0];
				DSP2.Op0DOutLen = DSP2.parameters[1];
				DSP2.in_index = 0;
				DSP2.in_count = (DSP2.Op0DInLen + 1) >> 1;
				DSP2.Op0DHasLen = true;

				if (byte)
					DSP2.waiting4command = false;
			}

			break;
		default:
			break;
	}
}

uint8_t DSP2GetByte(uint16_t address)
{
	uint8_t t;

	if ((address & 0xf000) != 0x6000 && (address < 0x8000 || address >= 0xc000))
		return 0x80;

	if (!DSP2.out_count)
		return 0xff;

	t = (uint8_t) DSP2.output[DSP2.out_index];
	DSP2.out_index++;

	if (DSP2.out_count == DSP2.out_index)
		DSP2.out_count = 0;

	return t;
}
