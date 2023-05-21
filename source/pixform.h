#ifndef CHIMERASNES_PIXFORM_H_
#define CHIMERASNES_PIXFORM_H_

/* RGB565 format - the only format supported by this core */
#define BUILD_PIXEL(R, G, B) \
	(((int32_t) (R) << 11) | ((int32_t) (G) << 6) | (int32_t) (B))

#define BUILD_PIXEL2(R, G, B) \
	(((int32_t) (R) << 11) | ((int32_t) (G) << 5) | (int32_t) (B))

#define DECOMPOSE_PIXEL(PIX, R, G, B) \
	{ \
		(R) =  (PIX) >> 11; \
		(G) = ((PIX) >> 6) & 0x1f; \
		(B) =  (PIX) & 0x1f; \
	}

#define SPARE_RGB_BIT_MASK (1 << 5)

#define MAX_RED            31
#define MAX_GREEN          63
#define MAX_BLUE           31
#define RED_SHIFT_BITS     11
#define GREEN_SHIFT_BITS   6
#define BLUE_SHIFT_BITS    0
#define RED_LOW_BIT_MASK   0x0800
#define GREEN_LOW_BIT_MASK 0x0020
#define BLUE_LOW_BIT_MASK  0x0001
#define RED_HI_BIT_MASK    0x8000
#define GREEN_HI_BIT_MASK  0x0400
#define BLUE_HI_BIT_MASK   0x0010
#define FIRST_COLOR_MASK   0xF800
#define SECOND_COLOR_MASK  0x07E0
#define THIRD_COLOR_MASK   0x001F
#define ALPHA_BITS_MASK    0x0000

#define GREEN_HI_BIT               ((MAX_GREEN + 1) >> 1)
#define RGB_LOW_BITS_MASK          (RED_LOW_BIT_MASK | GREEN_LOW_BIT_MASK | BLUE_LOW_BIT_MASK)
#define RGB_HI_BITS_MASK           (RED_HI_BIT_MASK  | GREEN_HI_BIT_MASK  | BLUE_HI_BIT_MASK)
#define RGB_HI_BITS_MASKx2         (RGB_HI_BITS_MASK << 1)
#define RGB_REMOVE_LOW_BITS_MASK   (~RGB_LOW_BITS_MASK)
#define FIRST_THIRD_COLOR_MASK     (FIRST_COLOR_MASK | THIRD_COLOR_MASK)
#define TWO_LOW_BITS_MASK          (RGB_LOW_BITS_MASK | (RGB_LOW_BITS_MASK << 1))
#define HIGH_BITS_SHIFTED_TWO_MASK (((FIRST_COLOR_MASK | SECOND_COLOR_MASK | THIRD_COLOR_MASK) & ~TWO_LOW_BITS_MASK) >> 2)
#endif
