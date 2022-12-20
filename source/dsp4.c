#include "chisnes.h"
#include "dsp.h"
#include "memmap.h"

#define DSP4_CLEAR_OUT()    \
	{                       \
		DSP4.out_count = 0; \
		DSP4.out_index = 0; \
	}

#define DSP4_WRITE_BYTE(d)                             \
	{                                                  \
		WRITE_WORD(DSP4.output + DSP4.out_count, (d)); \
		DSP4.out_count++; \
	}

#define DSP4_WRITE_WORD(d)                             \
	{                                                  \
		WRITE_WORD(DSP4.output + DSP4.out_count, (d)); \
		DSP4.out_count += 2; \
	}

#ifndef MSB_FIRST
	#define DSP4_WRITE_16_WORD(d)                          \
		{                                                  \
			memcpy(DSP4.output + DSP4.out_count, (d), 32); \
			DSP4.out_count += 32;                          \
		}
#else
	#define DSP4_WRITE_16_WORD(d)            \
		{                                    \
			for (int32_t p = 0; p < 16; p++) \
				DSP4_WRITE_WORD((d)[p]);     \
		}
#endif

/* used to wait for dsp i/o */
#define DSP4_WAIT(x)   \
	DSP4.in_index = 0; \
	DSP4.Logic = (x);  \
	return

/* 1.7.8 -> 1.15.16 */
#define SEX8(a) \
	(((int32_t) ((int16_t) (a))) << 8)

/* 1.15.0 -> 1.15.16 */
#define SEX16(a) \
	(((int32_t) ((int16_t) (a))) << 16)

static int16_t DSP4_READ_WORD();
static int32_t DSP4_READ_DWORD();
static int16_t DSP4_Inverse(int16_t);
static void    DSP4_Multiply(int16_t, int16_t, int32_t*);
static void    DSP4_OP01();
static void    DSP4_OP03();
static void    DSP4_OP05();
static void    DSP4_OP06();
static void    DSP4_OP07();
static void    DSP4_OP08();
static void    DSP4_OP09();
static void    DSP4_OP0A(int16_t, int16_t*, int16_t*, int16_t*, int16_t*);
static void    DSP4_OP0B(bool*, int16_t, int16_t, int16_t, bool, bool);
static void    DSP4_OP0D();
static void    DSP4_OP0E();
static void    DSP4_OP0F();
static void    DSP4_OP10();
static void    DSP4_OP11(int16_t, int16_t, int16_t, int16_t, int16_t*);
static void    DSP4_SetByte();
static void    DSP4_GetByte();

static int16_t DSP4_READ_WORD()
{
	int16_t out = READ_WORD(DSP4.parameters + DSP4.in_index);
	DSP4.in_index += 2;
	return out;
}

static int32_t DSP4_READ_DWORD()
{
	int32_t out = READ_DWORD(DSP4.parameters + DSP4.in_index);
	DSP4.in_index += 4;
	return out;
}

static int16_t DSP4_Inverse(int16_t value)
{
	static const uint16_t div_lut[64] = /* Attention: This lookup table is not verified */
	{
		0x0000, 0x8000, 0x4000, 0x2aaa, 0x2000, 0x1999, 0x1555, 0x1249,
		0x1000, 0x0e38, 0x0ccc, 0x0ba2, 0x0aaa, 0x09d8, 0x0924, 0x0888,
		0x0800, 0x0787, 0x071c, 0x06bc, 0x0666, 0x0618, 0x05d1, 0x0590,
		0x0555, 0x051e, 0x04ec, 0x04bd, 0x0492, 0x0469, 0x0444, 0x0421,
		0x0400, 0x03e0, 0x03c3, 0x03a8, 0x038e, 0x0375, 0x035e, 0x0348,
		0x0333, 0x031f, 0x030c, 0x02fa, 0x02e8, 0x02d8, 0x02c8, 0x02b9,
		0x02aa, 0x029c, 0x028f, 0x0282, 0x0276, 0x026a, 0x025e, 0x0253,
		0x0249, 0x023e, 0x0234, 0x022b, 0x0222, 0x0219, 0x0210, 0x0208
	};

	/* saturate bounds */
	if (value < 0)
		value = 0;

	if (value > 63)
		value = 63;

	return div_lut[value];
}

static void DSP4_Multiply(int16_t Multiplicand, int16_t Multiplier, int32_t* Product)
{
	*Product = (Multiplicand * Multiplier << 1) >> 1;
}

static void DSP4_OP01()
{
	DSP4.waiting4command = false;

	switch (DSP4.Logic) /* op flow control */
	{
		case 1:
			goto resume1;
			break;
		case 2:
			goto resume2;
			break;
		case 3:
			goto resume3;
			break;
	}

	/* process initial inputs */

	/* sort inputs */
	DSP4.world_y = DSP4_READ_DWORD();
	DSP4.poly_bottom[0][0] = DSP4_READ_WORD();
	DSP4.poly_top[0][0] = DSP4_READ_WORD();
	DSP4.poly_cx[1][0] = DSP4_READ_WORD();
	DSP4.viewport_bottom = DSP4_READ_WORD();
	DSP4.world_x = DSP4_READ_DWORD();
	DSP4.poly_cx[0][0] = DSP4_READ_WORD();
	DSP4.poly_ptr[0][0] = DSP4_READ_WORD();
	DSP4.world_yofs = DSP4_READ_WORD();
	DSP4.world_dy = DSP4_READ_DWORD();
	DSP4.world_dx = DSP4_READ_DWORD();
	DSP4.distance = DSP4_READ_WORD();
	DSP4_READ_WORD(); /* 0x0000 */
	DSP4.world_xenv = DSP4_READ_DWORD();
	DSP4.world_ddy = DSP4_READ_WORD();
	DSP4.world_ddx = DSP4_READ_WORD();
	DSP4.view_yofsenv = DSP4_READ_WORD();

	/* initial (x, y, offset) at starting raster line */
	DSP4.view_x1 = (DSP4.world_x + DSP4.world_xenv) >> 16;
	DSP4.view_y1 = DSP4.world_y >> 16;
	DSP4.view_xofs1 = DSP4.world_x >> 16;
	DSP4.view_yofs1 = DSP4.world_yofs;
	DSP4.view_turnoff_x = 0;
	DSP4.view_turnoff_dx = 0;

	/* first raster line */
	DSP4.poly_raster[0][0] = DSP4.poly_bottom[0][0];

	do
	{
		/* process one iteration of projection */

		/* perspective projection of world (x, y, scroll) points */
		/* based on the current projection lines */
		DSP4.view_x2 = (((DSP4.world_x + DSP4.world_xenv) >> 16) * DSP4.distance >> 15) + (DSP4.view_turnoff_x * DSP4.distance >> 15);
		DSP4.view_y2 = (DSP4.world_y >> 16) * DSP4.distance >> 15;
		DSP4.view_xofs2 = DSP4.view_x2;
		DSP4.view_yofs2 = (DSP4.world_yofs * DSP4.distance >> 15) + DSP4.poly_bottom[0][0] - DSP4.view_y2;

		/* 1. World x-location before transformation */
		/* 2. Viewer x-position at the next */
		/* 3. World y-location before perspective projection */
		/* 4. Viewer y-position below the horizon */
		/* 5. Number of raster lines drawn in this iteration */
		DSP4_CLEAR_OUT();
		DSP4_WRITE_WORD((DSP4.world_x + DSP4.world_xenv) >> 16);
		DSP4_WRITE_WORD(DSP4.view_x2);
		DSP4_WRITE_WORD(DSP4.world_y >> 16);
		DSP4_WRITE_WORD(DSP4.view_y2);

		/* determine # of raster lines used */
		DSP4.segments = DSP4.poly_raster[0][0] - DSP4.view_y2;

		/* prevent overdraw */
		if (DSP4.view_y2 >= DSP4.poly_raster[0][0])
			DSP4.segments = 0;
		else
			DSP4.poly_raster[0][0] = DSP4.view_y2;

		if (DSP4.view_y2 < DSP4.poly_top[0][0]) /* don't draw outside the window */
		{
			DSP4.segments = 0;

			if (DSP4.view_y1 >= DSP4.poly_top[0][0]) /* flush remaining raster lines */
				DSP4.segments = DSP4.view_y1 - DSP4.poly_top[0][0];
		}

		DSP4_WRITE_WORD(DSP4.segments);

		if (DSP4.segments) /* scan next command if no SR check needed */
		{
			int32_t px_dx, py_dy;
			int32_t x_scroll, y_scroll;

			/* linear interpolation (lerp) between projected points */
			px_dx = (DSP4.view_xofs2 - DSP4.view_xofs1) * DSP4_Inverse(DSP4.segments) << 1;
			py_dy = (DSP4.view_yofs2 - DSP4.view_yofs1) * DSP4_Inverse(DSP4.segments) << 1;

			/* starting step values */
			x_scroll = SEX16(DSP4.poly_cx[0][0] + DSP4.view_xofs1);
			y_scroll = SEX16(-DSP4.viewport_bottom + DSP4.view_yofs1 + DSP4.view_yofsenv + DSP4.poly_cx[1][0] - DSP4.world_yofs);

			for (DSP4.lcv = 0; DSP4.lcv < DSP4.segments; DSP4.lcv++) /* rasterize line */
			{
				/* 1. HDMA memory pointer (bg1) */
				/* 2. vertical scroll offset ($210E) */
				/* 3. horizontal scroll offset ($210D) */
				DSP4_WRITE_WORD(DSP4.poly_ptr[0][0]);
				DSP4_WRITE_WORD((y_scroll + 0x8000) >> 16);
				DSP4_WRITE_WORD((x_scroll + 0x8000) >> 16);

				/* update memory address */
				DSP4.poly_ptr[0][0] -= 4;

				/* update screen values */
				x_scroll += px_dx;
				y_scroll += py_dy;
			}
		}

		/* Post-update */

		/* update new viewer (x, y, scroll) to last raster line drawn */
		DSP4.view_x1 = DSP4.view_x2;
		DSP4.view_y1 = DSP4.view_y2;
		DSP4.view_xofs1 = DSP4.view_xofs2;
		DSP4.view_yofs1 = DSP4.view_yofs2;

		/* add deltas for projection lines */
		DSP4.world_dx += SEX8(DSP4.world_ddx);
		DSP4.world_dy += SEX8(DSP4.world_ddy);

		/* update projection lines */
		DSP4.world_x += (DSP4.world_dx + DSP4.world_xenv);
		DSP4.world_y += DSP4.world_dy;

		/* update road turnoff position */
		DSP4.view_turnoff_x += DSP4.view_turnoff_dx;

		/* command check */

		/* scan next command */
		DSP4.in_count = 2;
		DSP4_WAIT(1);

	resume1:
		/* check for termination */
		DSP4.distance = DSP4_READ_WORD();

		if (DSP4.distance == -0x8000)
			break;

		if ((uint16_t) DSP4.distance == 0x8001) /* road turnoff */
		{
			DSP4.in_count = 6;
			DSP4_WAIT(2);

	resume2:
			DSP4.distance = DSP4_READ_WORD();
			DSP4.view_turnoff_x = DSP4_READ_WORD();
			DSP4.view_turnoff_dx = DSP4_READ_WORD();

			/* factor in new changes */
			DSP4.view_x1 += (DSP4.view_turnoff_x * DSP4.distance >> 15);
			DSP4.view_xofs1 += (DSP4.view_turnoff_x * DSP4.distance >> 15);

			/* update stepping values */
			DSP4.view_turnoff_x += DSP4.view_turnoff_dx;

			DSP4.in_count = 2;
			DSP4_WAIT(1);
		}

		/* already have 2 bytes read */
		DSP4.in_count = 6;
		DSP4_WAIT(3);

	resume3:
		/* inspect inputs */
		DSP4.world_ddy = DSP4_READ_WORD();
		DSP4.world_ddx = DSP4_READ_WORD();
		DSP4.view_yofsenv = DSP4_READ_WORD();

		/* no envelope here */
		DSP4.world_xenv = 0;
	} while (1);

	/* terminate op */
	DSP4.waiting4command = true;
}

static void DSP4_OP03()
{
	DSP4.OAM_RowMax = 33;
	memset(DSP4.OAM_Row, 0, 64);
}

static void DSP4_OP05()
{
	DSP4.OAM_index = 0;
	DSP4.OAM_bits = 0;
	memset(DSP4.OAM_attr, 0, 32);
	DSP4.sprite_count = 0;
}

static void DSP4_OP06()
{
	DSP4_CLEAR_OUT();
	DSP4_WRITE_16_WORD(DSP4.OAM_attr);
}

static void DSP4_OP07()
{
	DSP4.waiting4command = false;

	switch (DSP4.Logic) /* op flow control */
	{
		case 1:
			goto resume1;
			break;
		case 2:
			goto resume2;
			break;
	}

	/* sort inputs */

	DSP4.world_y = DSP4_READ_DWORD();
	DSP4.poly_bottom[0][0] = DSP4_READ_WORD();
	DSP4.poly_top[0][0] = DSP4_READ_WORD();
	DSP4.poly_cx[1][0] = DSP4_READ_WORD();
	DSP4.viewport_bottom = DSP4_READ_WORD();
	DSP4.world_x = DSP4_READ_DWORD();
	DSP4.poly_cx[0][0] = DSP4_READ_WORD();
	DSP4.poly_ptr[0][0] = DSP4_READ_WORD();
	DSP4.world_yofs = DSP4_READ_WORD();
	DSP4.distance = DSP4_READ_WORD();
	DSP4.view_y2 = DSP4_READ_WORD();
	DSP4.view_dy = DSP4_READ_WORD() * DSP4.distance >> 15;
	DSP4.view_x2 = DSP4_READ_WORD();
	DSP4.view_dx = DSP4_READ_WORD() * DSP4.distance >> 15;
	DSP4.view_yofsenv = DSP4_READ_WORD();

	/* initial (x, y, offset) at starting raster line */
	DSP4.view_x1 = DSP4.world_x >> 16;
	DSP4.view_y1 = DSP4.world_y >> 16;
	DSP4.view_xofs1 = DSP4.view_x1;
	DSP4.view_yofs1 = DSP4.world_yofs;

	/* first raster line */
	DSP4.poly_raster[0][0] = DSP4.poly_bottom[0][0];

	do
	{
		/* process one iteration of projection */

		/* add shaping */
		DSP4.view_x2 += DSP4.view_dx;
		DSP4.view_y2 += DSP4.view_dy;

		/* vertical scroll calculation */
		DSP4.view_xofs2 = DSP4.view_x2;
		DSP4.view_yofs2 = (DSP4.world_yofs * DSP4.distance >> 15) + DSP4.poly_bottom[0][0] - DSP4.view_y2;

		/* 1. Viewer x-position at the next */
		/* 2. Viewer y-position below the horizon */
		/* 3. Number of raster lines drawn in this iteration */
		DSP4_CLEAR_OUT();
		DSP4_WRITE_WORD(DSP4.view_x2);
		DSP4_WRITE_WORD(DSP4.view_y2);

		/* determine # of raster lines used */
		DSP4.segments = DSP4.view_y1 - DSP4.view_y2;

		/* prevent overdraw */
		if (DSP4.view_y2 >= DSP4.poly_raster[0][0])
			DSP4.segments = 0;
		else
			DSP4.poly_raster[0][0] = DSP4.view_y2;

		if (DSP4.view_y2 < DSP4.poly_top[0][0]) /* don't draw outside the window */
		{
			DSP4.segments = 0;

			if (DSP4.view_y1 >= DSP4.poly_top[0][0]) /* flush remaining raster lines */
				DSP4.segments = DSP4.view_y1 - DSP4.poly_top[0][0];
		}

		DSP4_WRITE_WORD(DSP4.segments);

		if (DSP4.segments) /* scan next command if no SR check needed */
		{
			int32_t px_dx, py_dy;
			int32_t x_scroll, y_scroll;

			/* linear interpolation (lerp) between projected points */
			px_dx = (DSP4.view_xofs2 - DSP4.view_xofs1) * DSP4_Inverse(DSP4.segments) << 1;
			py_dy = (DSP4.view_yofs2 - DSP4.view_yofs1) * DSP4_Inverse(DSP4.segments) << 1;

			/* starting step values */
			x_scroll = SEX16(DSP4.poly_cx[0][0] + DSP4.view_xofs1);
			y_scroll = SEX16(-DSP4.viewport_bottom + DSP4.view_yofs1 + DSP4.view_yofsenv + DSP4.poly_cx[1][0] - DSP4.world_yofs);

			for (DSP4.lcv = 0; DSP4.lcv < DSP4.segments; DSP4.lcv++) /* rasterize line */
			{
				/* 1. HDMA memory pointer (bg2) */
				/* 2. vertical scroll offset ($2110) */
				/* 3. horizontal scroll offset ($210F) */
				DSP4_WRITE_WORD(DSP4.poly_ptr[0][0]);
				DSP4_WRITE_WORD((y_scroll + 0x8000) >> 16);
				DSP4_WRITE_WORD((x_scroll + 0x8000) >> 16);

				/* update memory address */
				DSP4.poly_ptr[0][0] -= 4;

				/* update screen values */
				x_scroll += px_dx;
				y_scroll += py_dy;
			}
		}

		/* Post-update */

		/* update new viewer (x, y, scroll) to last raster line drawn */
		DSP4.view_x1 = DSP4.view_x2;
		DSP4.view_y1 = DSP4.view_y2;
		DSP4.view_xofs1 = DSP4.view_xofs2;
		DSP4.view_yofs1 = DSP4.view_yofs2;

		/* command check */

		/* scan next command */
		DSP4.in_count = 2;
		DSP4_WAIT(1);

	resume1:
		/* check for opcode termination */
		DSP4.distance = DSP4_READ_WORD();

		if (DSP4.distance == -0x8000)
			break;

		/* already have 2 bytes in queue */
		DSP4.in_count = 10;
		DSP4_WAIT(2);

	resume2:
		/* inspect inputs */
		DSP4.view_y2 = DSP4_READ_WORD();
		DSP4.view_dy = DSP4_READ_WORD() * DSP4.distance >> 15;
		DSP4.view_x2 = DSP4_READ_WORD();
		DSP4.view_dx = DSP4_READ_WORD() * DSP4.distance >> 15;
		DSP4.view_yofsenv = DSP4_READ_WORD();
	} while (1);

	DSP4.waiting4command = true;
}

static void DSP4_OP08()
{
	int16_t win_left, win_right;
	int16_t view_x[2], view_y[2];
	int16_t envelope[2][2];
	DSP4.waiting4command = false;

	switch (DSP4.Logic) /* op flow control */
	{
		case 1:
			goto resume1;
			break;
		case 2:
			goto resume2;
			break;
	}

	/* process initial inputs for two polygons */

	/* clip values */
	DSP4.poly_clipRt[0][0] = DSP4_READ_WORD();
	DSP4.poly_clipRt[0][1] = DSP4_READ_WORD();
	DSP4.poly_clipRt[1][0] = DSP4_READ_WORD();
	DSP4.poly_clipRt[1][1] = DSP4_READ_WORD();
	DSP4.poly_clipLf[0][0] = DSP4_READ_WORD();
	DSP4.poly_clipLf[0][1] = DSP4_READ_WORD();
	DSP4.poly_clipLf[1][0] = DSP4_READ_WORD();
	DSP4.poly_clipLf[1][1] = DSP4_READ_WORD();

	/* unknown (constant) (ex. 1P/2P = $00A6, $00A6, $00A6, $00A6) */
	DSP4_READ_WORD();
	DSP4_READ_WORD();
	DSP4_READ_WORD();
	DSP4_READ_WORD();

	/* unknown (constant) (ex. 1P/2P = $00A5, $00A5, $00A7, $00A7) */
	DSP4_READ_WORD();
	DSP4_READ_WORD();
	DSP4_READ_WORD();
	DSP4_READ_WORD();

	/* polygon centering (left, right) */
	DSP4.poly_cx[0][0] = DSP4_READ_WORD();
	DSP4.poly_cx[0][1] = DSP4_READ_WORD();
	DSP4.poly_cx[1][0] = DSP4_READ_WORD();
	DSP4.poly_cx[1][1] = DSP4_READ_WORD();

	/* HDMA pointer locations */
	DSP4.poly_ptr[0][0] = DSP4_READ_WORD();
	DSP4.poly_ptr[0][1] = DSP4_READ_WORD();
	DSP4.poly_ptr[1][0] = DSP4_READ_WORD();
	DSP4.poly_ptr[1][1] = DSP4_READ_WORD();

	/* starting raster line below the horizon */
	DSP4.poly_bottom[0][0] = DSP4_READ_WORD();
	DSP4.poly_bottom[0][1] = DSP4_READ_WORD();
	DSP4.poly_bottom[1][0] = DSP4_READ_WORD();
	DSP4.poly_bottom[1][1] = DSP4_READ_WORD();

	/* top boundary line to clip */
	DSP4.poly_top[0][0] = DSP4_READ_WORD();
	DSP4.poly_top[0][1] = DSP4_READ_WORD();
	DSP4.poly_top[1][0] = DSP4_READ_WORD();
	DSP4.poly_top[1][1] = DSP4_READ_WORD();

	/* unknown */
	/* (ex. 1P = $2FC8, $0034, $FF5C, $0035) */
	/* (ex. 2P = $3178, $0034, $FFCC, $0035) */
	/* (ex. 2P = $2FC8, $0034, $FFCC, $0035) */
	DSP4_READ_WORD();
	DSP4_READ_WORD();
	DSP4_READ_WORD();
	DSP4_READ_WORD();

	/* look at guidelines for both polygon shapes */
	DSP4.distance = DSP4_READ_WORD();
	view_x[0] = DSP4_READ_WORD();
	view_y[0] = DSP4_READ_WORD();
	view_x[1] = DSP4_READ_WORD();
	view_y[1] = DSP4_READ_WORD();

	/* envelope shaping guidelines (one frame only) */
	envelope[0][0] = DSP4_READ_WORD();
	envelope[0][1] = DSP4_READ_WORD();
	envelope[1][0] = DSP4_READ_WORD();
	envelope[1][1] = DSP4_READ_WORD();

	/* starting base values to project from */
	DSP4.poly_start[0] = view_x[0];
	DSP4.poly_start[1] = view_x[1];

	/* starting raster lines to begin drawing */
	DSP4.poly_raster[0][0] = view_y[0];
	DSP4.poly_raster[0][1] = view_y[0];
	DSP4.poly_raster[1][0] = view_y[1];
	DSP4.poly_raster[1][1] = view_y[1];

	/* starting distances */
	DSP4.poly_plane[0] = DSP4.distance;
	DSP4.poly_plane[1] = DSP4.distance;

	/* re-center coordinates */
	win_left = DSP4.poly_cx[0][0] - view_x[0] + envelope[0][0];
	win_right = DSP4.poly_cx[0][1] - view_x[0] + envelope[0][1];

	/* saturate offscreen data for polygon #1 */
	if (win_left < DSP4.poly_clipLf[0][0])
		win_left = DSP4.poly_clipLf[0][0];

	if (win_left > DSP4.poly_clipRt[0][0])
		win_left = DSP4.poly_clipRt[0][0];

	if (win_right < DSP4.poly_clipLf[0][1])
		win_right = DSP4.poly_clipLf[0][1];

	if (win_right > DSP4.poly_clipRt[0][1])
		win_right = DSP4.poly_clipRt[0][1];

	/* initial output for polygon #1 */
	DSP4_CLEAR_OUT();
	DSP4_WRITE_BYTE(win_left & 0xff);
	DSP4_WRITE_BYTE(win_right & 0xff);

	do
	{
		int16_t polygon;

		/* command check */

		/* scan next command */
		DSP4.in_count = 2;
		DSP4_WAIT(1);

	resume1:
		/* terminate op */
		DSP4.distance = DSP4_READ_WORD();

		if (DSP4.distance == -0x8000)
			break;

		/* already have 2 bytes in queue */
		DSP4.in_count = 16;
		DSP4_WAIT(2);

	resume2:
		/* look at guidelines for both polygon shapes */
		view_x[0] = DSP4_READ_WORD();
		view_y[0] = DSP4_READ_WORD();
		view_x[1] = DSP4_READ_WORD();
		view_y[1] = DSP4_READ_WORD();

		/* envelope shaping guidelines (one frame only) */
		envelope[0][0] = DSP4_READ_WORD();
		envelope[0][1] = DSP4_READ_WORD();
		envelope[1][0] = DSP4_READ_WORD();
		envelope[1][1] = DSP4_READ_WORD();

		/* projection begins */

		/* init */
		DSP4_CLEAR_OUT();

		for (polygon = 0; polygon < 2; polygon++) /* solid polygon renderer - 2 shapes */
		{
			int32_t left_inc, right_inc;
			int16_t x1_final, x2_final;
			int16_t env[2][2];
			int16_t poly;

			/* # raster lines to draw */
			DSP4.segments = DSP4.poly_raster[polygon][0] - view_y[polygon];

			if (DSP4.segments > 0) /* prevent overdraw */
			{
				/* bump drawing cursor */
				DSP4.poly_raster[polygon][0] = view_y[polygon];
				DSP4.poly_raster[polygon][1] = view_y[polygon];
			}
			else
				DSP4.segments = 0;

			if (view_y[polygon] < DSP4.poly_top[polygon][0]) /* don't draw outside the window */
			{
				DSP4.segments = 0;

				if (view_y[polygon] >= DSP4.poly_top[polygon][0]) /* flush remaining raster lines */
					DSP4.segments = view_y[polygon] - DSP4.poly_top[polygon][0];
			}

			/* tell user how many raster structures to read in */
			DSP4_WRITE_WORD(DSP4.segments);

			/* normal parameters */
			poly = polygon;

			/* scan next command if no SR check needed */
			if (DSP4.segments)
			{
				int32_t w_left, w_right;

				/* road turnoff selection */
				if ((uint16_t) envelope[polygon][0] == (uint16_t) 0xc001)
					poly = 1;
				else if (envelope[polygon][1] == 0x3fff)
					poly = 1;

				/* left side of polygon */

				/* perspective correction on additional shaping parameters */
				env[0][0] = envelope[polygon][0] * DSP4.poly_plane[poly] >> 15;
				env[0][1] = envelope[polygon][0] * DSP4.distance >> 15;

				/* project new shapes (left side) */
				x1_final = view_x[poly] + env[0][0];
				x2_final = DSP4.poly_start[poly] + env[0][1];

				/* interpolate between projected points with shaping */
				left_inc = (x2_final - x1_final) * DSP4_Inverse(DSP4.segments) << 1;
				if (DSP4.segments == 1)
					left_inc = -left_inc;

				/* right side of polygon */

				/* perspective correction on additional shaping parameters */
				env[1][0] = envelope[polygon][1] * DSP4.poly_plane[poly] >> 15;
				env[1][1] = envelope[polygon][1] * DSP4.distance >> 15;

				/* project new shapes (right side) */
				x1_final = view_x[poly] + env[1][0];
				x2_final = DSP4.poly_start[poly] + env[1][1];

				/* interpolate between projected points with shaping */
				right_inc = (x2_final - x1_final) * DSP4_Inverse(DSP4.segments) << 1;

				if (DSP4.segments == 1)
					right_inc = -right_inc;

				/* update each point on the line */

				w_left = SEX16(DSP4.poly_cx[polygon][0] - DSP4.poly_start[poly] + env[0][0]);
				w_right = SEX16(DSP4.poly_cx[polygon][1] - DSP4.poly_start[poly] + env[1][0]);

				/* update distance drawn into world */
				DSP4.poly_plane[polygon] = DSP4.distance;

				for (DSP4.lcv = 0; DSP4.lcv < DSP4.segments; DSP4.lcv++) /* rasterize line */
				{
					int16_t x_left, x_right;

					/* project new coordinates */
					w_left += left_inc;
					w_right += right_inc;

					/* grab integer portion, drop fraction (no rounding) */
					x_left = w_left >> 16;
					x_right = w_right >> 16;

					/* saturate offscreen data */
					if (x_left < DSP4.poly_clipLf[polygon][0])
						x_left = DSP4.poly_clipLf[polygon][0];

					if (x_left > DSP4.poly_clipRt[polygon][0])
						x_left = DSP4.poly_clipRt[polygon][0];

					if (x_right < DSP4.poly_clipLf[polygon][1])
						x_right = DSP4.poly_clipLf[polygon][1];

					if (x_right > DSP4.poly_clipRt[polygon][1])
						x_right = DSP4.poly_clipRt[polygon][1];

					/* 1. HDMA memory pointer */
					/* 2. Left window position ($2126/$2128) */
					/* 3. Right window position ($2127/$2129) */
					DSP4_WRITE_WORD(DSP4.poly_ptr[polygon][0]);
					DSP4_WRITE_BYTE(x_left & 0xff);
					DSP4_WRITE_BYTE(x_right & 0xff);

					/* update memory pointers */
					DSP4.poly_ptr[polygon][0] -= 4;
					DSP4.poly_ptr[polygon][1] -= 4;
				} /* end rasterize line */
			}

			/* Post-update */

			/* new projection spot to continue rasterizing from */
			DSP4.poly_start[polygon] = view_x[poly];
		} /* end polygon rasterizer */
	} while (1);

	/* unknown output */
	DSP4_CLEAR_OUT();
	DSP4_WRITE_WORD(0);
	DSP4.waiting4command = true;
}

static void DSP4_OP09()
{
	DSP4.waiting4command = false;

	switch (DSP4.Logic) /* op flow control */
	{
		case 1:
			goto resume1;
			break;
		case 2:
			goto resume2;
			break;
		case 3:
			goto resume3;
			break;
		case 4:
			goto resume4;
			break;
		case 5:
			goto resume5;
			break;
		case 6:
			goto resume6;
			break;
	}

	/* process initial inputs */

	/* grab screen information */
	DSP4.viewport_cx = DSP4_READ_WORD();
	DSP4.viewport_cy = DSP4_READ_WORD();
	DSP4_READ_WORD(); /* 0x0000 */
	DSP4.viewport_left = DSP4_READ_WORD();
	DSP4.viewport_right = DSP4_READ_WORD();
	DSP4.viewport_top = DSP4_READ_WORD();
	DSP4.viewport_bottom = DSP4_READ_WORD();

	/* starting raster line below the horizon */
	DSP4.poly_bottom[0][0] = DSP4.viewport_bottom - DSP4.viewport_cy;
	DSP4.poly_raster[0][0] = 0x100;

	do
	{
		/* check for new sprites */
		DSP4.in_count = 4;
		DSP4_WAIT(1);

	resume1:
		/* raster overdraw check */
		DSP4.raster = DSP4_READ_WORD();

		/* continue updating the raster line where overdraw begins */
		if (DSP4.raster < DSP4.poly_raster[0][0])
		{
			DSP4.sprite_clipy = DSP4.viewport_bottom - (DSP4.poly_bottom[0][0] - DSP4.raster);
			DSP4.poly_raster[0][0] = DSP4.raster;
		}

		/* identify sprite */

		/* op termination */
		DSP4.distance = DSP4_READ_WORD();

		if (DSP4.distance == -0x8000)
			goto terminate;

		/* no sprite */
		if (DSP4.distance == 0x0000)
			continue;

		/* process projection information */

		/* vehicle sprite */
		if ((uint16_t) DSP4.distance == 0x9000)
		{
			int16_t car_left, car_right, car_back;
			int16_t impact_left, impact_back;
			int16_t world_spx, world_spy;
			int16_t view_spx, view_spy;
			uint16_t energy;

			/* we already have 4 bytes we want */
			DSP4.in_count = 14;
			DSP4_WAIT(2);

		resume2:
			/* filter inputs */
			energy = DSP4_READ_WORD();
			impact_back = DSP4_READ_WORD();
			car_back = DSP4_READ_WORD();
			impact_left = DSP4_READ_WORD();
			car_left = DSP4_READ_WORD();
			DSP4.distance = DSP4_READ_WORD();
			car_right = DSP4_READ_WORD();

			/* calculate car's world (x, y) values */
			world_spx = car_right - car_left;
			world_spy = car_back;

			/* add in collision vector [needs bit-twiddling] */
			world_spx -= energy * (impact_left - car_left) >> 16;
			world_spy -= energy * (car_back - impact_back) >> 16;

			/* perspective correction for world (x, y) */
			view_spx = world_spx * DSP4.distance >> 15;
			view_spy = world_spy * DSP4.distance >> 15;

			/* convert to screen values */
			DSP4.sprite_x = DSP4.viewport_cx + view_spx;
			DSP4.sprite_y = DSP4.viewport_bottom - (DSP4.poly_bottom[0][0] - view_spy);

			/* make the car's (x)-coordinate available */
			DSP4_CLEAR_OUT();
			DSP4_WRITE_WORD(world_spx);

			/* grab a few remaining vehicle values */
			DSP4.in_count = 4;
			DSP4_WAIT(3);

		resume3:
			/* add vertical lift factor */
			DSP4.sprite_y += DSP4_READ_WORD();
		}
		/* terrain sprite */
		else
		{
			int16_t world_spx, world_spy;
			int16_t view_spx, view_spy;

			/* we already have 4 bytes we want */
			DSP4.in_count = 10;
			DSP4_WAIT(4);

		resume4:
			/* sort loop inputs */
			DSP4.poly_cx[0][0] = DSP4_READ_WORD();
			DSP4.poly_raster[0][1] = DSP4_READ_WORD();
			world_spx = DSP4_READ_WORD();
			world_spy = DSP4_READ_WORD();

			/* compute base raster line from the bottom */
			DSP4.segments = DSP4.poly_bottom[0][0] - DSP4.raster;

			/* perspective correction for world (x, y) */
			view_spx = world_spx * DSP4.distance >> 15;
			view_spy = world_spy * DSP4.distance >> 15;

			/* convert to screen values */
			DSP4.sprite_x = DSP4.viewport_cx + view_spx - DSP4.poly_cx[0][0];
			DSP4.sprite_y = DSP4.viewport_bottom - DSP4.segments + view_spy;
		}

		/* default sprite size: 16x16 */
		DSP4.sprite_size = 1;
		DSP4.sprite_attr = DSP4_READ_WORD();

		/* convert tile data to SNES OAM format */

		do
		{
			int16_t sp_x, sp_y, sp_attr, sp_dattr;
			int16_t sp_dx, sp_dy;
			int16_t pixels;
			uint16_t header;
			bool draw;
			DSP4.in_count = 2;
			DSP4_WAIT(5);

		resume5:
			draw = true;

			/* opcode termination */
			DSP4.raster = DSP4_READ_WORD();

			if (DSP4.raster == -0x8000)
				goto terminate;

			if (DSP4.raster == 0x0000 && !DSP4.sprite_size) /* stop code */
				break;

			if (DSP4.raster == 0x0000) /* toggle sprite size */
			{
				DSP4.sprite_size = !DSP4.sprite_size;
				continue;
			}

			/* check for valid sprite header */
			header = DSP4.raster;
			header >>= 8;

			if (header != 0x20 && header != 0x2e && /* This is for attractor sprite */
			    header != 0x40 && header != 0x60 && header != 0xa0 && header != 0xc0 && header != 0xe0)
				break;

			/* read in rest of sprite data */
			DSP4.in_count = 4;
			DSP4_WAIT(6);

		resume6:
			draw = true;

			/* process tile data */

			/* sprite deltas */
			sp_dattr = DSP4.raster;
			sp_dy = DSP4_READ_WORD();
			sp_dx = DSP4_READ_WORD();

			/* update coordinates to screen space */
			sp_x = DSP4.sprite_x + sp_dx;
			sp_y = DSP4.sprite_y + sp_dy;

			/* update sprite nametable/attribute information */
			sp_attr = DSP4.sprite_attr + sp_dattr;

			/* allow partially visibile tiles */
			pixels = DSP4.sprite_size ? 15 : 7;
			DSP4_CLEAR_OUT();

			/* transparent tile to clip off parts of a sprite (overdraw) */
			if (DSP4.sprite_clipy - pixels <= sp_y && sp_y <= DSP4.sprite_clipy && sp_x >= DSP4.viewport_left - pixels && sp_x <= DSP4.viewport_right && DSP4.sprite_clipy >= DSP4.viewport_top - pixels && DSP4.sprite_clipy <= DSP4.viewport_bottom)
				DSP4_OP0B(&draw, sp_x, DSP4.sprite_clipy, 0x00EE, DSP4.sprite_size, 0);

			/* normal sprite tile */
			if (sp_x >= DSP4.viewport_left - pixels && sp_x <= DSP4.viewport_right && sp_y >= DSP4.viewport_top - pixels && sp_y <= DSP4.viewport_bottom && sp_y <= DSP4.sprite_clipy)
				DSP4_OP0B(&draw, sp_x, sp_y, sp_attr, DSP4.sprite_size, 0);

			/* no following OAM data */
			DSP4_OP0B(&draw, 0, 0x0100, 0, 0, 1);
		} while (1);
	} while (1);

terminate:
	DSP4.waiting4command = true;
}

static void DSP4_OP0A(int16_t n2, int16_t* o1, int16_t* o2, int16_t* o3, int16_t* o4)
{
	const uint16_t OP0A_Values[16] =
	{
		0x0000, 0x0030, 0x0060, 0x0090, 0x00c0, 0x00f0, 0x0120, 0x0150,
		0xfe80, 0xfeb0, 0xfee0, 0xff10, 0xff40, 0xff70, 0xffa0, 0xffd0
	};

	*o4 = OP0A_Values[(n2 & 0x000f)];
	*o3 = OP0A_Values[(n2 & 0x00f0) >> 4];
	*o2 = OP0A_Values[(n2 & 0x0f00) >> 8];
	*o1 = OP0A_Values[(n2 & 0xf000) >> 12];
}

static void DSP4_OP0B(bool* draw, int16_t sp_x, int16_t sp_y, int16_t sp_attr, bool size, bool stop)
{
	/* align to nearest 8-pixel row */
	int16_t Row1 = (sp_y >> 3) & 0x1f;
	int16_t Row2 = (Row1 + 1) & 0x1f;

	if (!((sp_y < 0) || ((sp_y & 0x01ff) < 0x00eb))) /* check boundaries */
		*draw = 0;

	if (size)
	{
		if (DSP4.OAM_Row[Row1] + 1 >= DSP4.OAM_RowMax)
			*draw = 0;

		if (DSP4.OAM_Row[Row2] + 1 >= DSP4.OAM_RowMax)
			*draw = 0;
	}
	else if (DSP4.OAM_Row[Row1] >= DSP4.OAM_RowMax)
		*draw = 0;

	/* emulator fail-safe (unknown if this really exists) */
	if (DSP4.sprite_count >= 128)
		*draw = 0;

	if (*draw)
	{
		if (size) /* Row tiles */
		{
			DSP4.OAM_Row[Row1] += 2;
			DSP4.OAM_Row[Row2] += 2;
		}
		else
			DSP4.OAM_Row[Row1]++;

		/* yield OAM output */
		DSP4_WRITE_WORD(1);

		/* pack OAM data: x, y, name, attr */
		DSP4_WRITE_BYTE(sp_x & 0xff);
		DSP4_WRITE_BYTE(sp_y & 0xff);
		DSP4_WRITE_WORD(sp_attr);
		DSP4.sprite_count++;

		/* OAM: size, msb data */
		/* save post-oam table data for future retrieval */
		DSP4.OAM_attr[DSP4.OAM_index] |= ((sp_x < 0 || sp_x > 255) << DSP4.OAM_bits);
		DSP4.OAM_bits++;
		DSP4.OAM_attr[DSP4.OAM_index] |= (size << DSP4.OAM_bits);
		DSP4.OAM_bits++;

		if (DSP4.OAM_bits == 16) /* move to next byte in buffer */
		{
			DSP4.OAM_bits = 0;
			DSP4.OAM_index++;
		}
	}
	else if (stop) /* yield no OAM output */
		DSP4_WRITE_WORD(0);
}

static void DSP4_OP0D()
{
	DSP4.waiting4command = false;

	switch (DSP4.Logic) /* op flow control */
	{
		case 1:
			goto resume1;
			break;
		case 2:
			goto resume2;
			break;
	}

	/* process initial inputs */

	/* sort inputs */
	DSP4.world_y = DSP4_READ_DWORD();
	DSP4.poly_bottom[0][0] = DSP4_READ_WORD();
	DSP4.poly_top[0][0] = DSP4_READ_WORD();
	DSP4.poly_cx[1][0] = DSP4_READ_WORD();
	DSP4.viewport_bottom = DSP4_READ_WORD();
	DSP4.world_x = DSP4_READ_DWORD();
	DSP4.poly_cx[0][0] = DSP4_READ_WORD();
	DSP4.poly_ptr[0][0] = DSP4_READ_WORD();
	DSP4.world_yofs = DSP4_READ_WORD();
	DSP4.world_dy = DSP4_READ_DWORD();
	DSP4.world_dx = DSP4_READ_DWORD();
	DSP4.distance = DSP4_READ_WORD();
	DSP4_READ_WORD(); /* 0x0000 */
	DSP4.world_xenv = SEX8(DSP4_READ_WORD());
	DSP4.world_ddy = DSP4_READ_WORD();
	DSP4.world_ddx = DSP4_READ_WORD();
	DSP4.view_yofsenv = DSP4_READ_WORD();

	/* initial (x, y, offset) at starting raster line */
	DSP4.view_x1 = (DSP4.world_x + DSP4.world_xenv) >> 16;
	DSP4.view_y1 = DSP4.world_y >> 16;
	DSP4.view_xofs1 = DSP4.world_x >> 16;
	DSP4.view_yofs1 = DSP4.world_yofs;

	/* first raster line */
	DSP4.poly_raster[0][0] = DSP4.poly_bottom[0][0];

	do
	{
		/* process one iteration of projection */

		/* perspective projection of world (x, y, scroll) points */
		/* based on the current projection lines */
		DSP4.view_x2 = (((DSP4.world_x + DSP4.world_xenv) >> 16) * DSP4.distance >> 15) + (DSP4.view_turnoff_x * DSP4.distance >> 15);
		DSP4.view_y2 = (DSP4.world_y >> 16) * DSP4.distance >> 15;
		DSP4.view_xofs2 = DSP4.view_x2;
		DSP4.view_yofs2 = (DSP4.world_yofs * DSP4.distance >> 15) + DSP4.poly_bottom[0][0] - DSP4.view_y2;

		/* 1. World x-location before transformation */
		/* 2. Viewer x-position at the current */
		/* 3. World y-location before perspective projection */
		/* 4. Viewer y-position below the horizon */
		/* 5. Number of raster lines drawn in this iteration */
		DSP4_CLEAR_OUT();
		DSP4_WRITE_WORD((DSP4.world_x + DSP4.world_xenv) >> 16);
		DSP4_WRITE_WORD(DSP4.view_x2);
		DSP4_WRITE_WORD(DSP4.world_y >> 16);
		DSP4_WRITE_WORD(DSP4.view_y2);

		/* determine # of raster lines used */
		DSP4.segments = DSP4.view_y1 - DSP4.view_y2;

		/* prevent overdraw */
		if (DSP4.view_y2 >= DSP4.poly_raster[0][0])
			DSP4.segments = 0;
		else
			DSP4.poly_raster[0][0] = DSP4.view_y2;

		if (DSP4.view_y2 < DSP4.poly_top[0][0]) /* don't draw outside the window */
		{
			DSP4.segments = 0;

			if (DSP4.view_y1 >= DSP4.poly_top[0][0]) /* flush remaining raster lines */
				DSP4.segments = DSP4.view_y1 - DSP4.poly_top[0][0];
		}

		DSP4_WRITE_WORD(DSP4.segments);

		if (DSP4.segments) /* scan next command if no SR check needed */
		{
			int32_t px_dx, py_dy;
			int32_t x_scroll, y_scroll;

			/* linear interpolation (lerp) between projected points */
			px_dx = (DSP4.view_xofs2 - DSP4.view_xofs1) * DSP4_Inverse(DSP4.segments) << 1;
			py_dy = (DSP4.view_yofs2 - DSP4.view_yofs1) * DSP4_Inverse(DSP4.segments) << 1;

			/* starting step values */
			x_scroll = SEX16(DSP4.poly_cx[0][0] + DSP4.view_xofs1);
			y_scroll = SEX16(-DSP4.viewport_bottom + DSP4.view_yofs1 + DSP4.view_yofsenv + DSP4.poly_cx[1][0] - DSP4.world_yofs);

			for (DSP4.lcv = 0; DSP4.lcv < DSP4.segments; DSP4.lcv++) /* rasterize line */
			{
				/* 1. HDMA memory pointer (bg1) */
				/* 2. vertical scroll offset ($210E) */
				/* 3. horizontal scroll offset ($210D) */
				DSP4_WRITE_WORD(DSP4.poly_ptr[0][0]);
				DSP4_WRITE_WORD((y_scroll + 0x8000) >> 16);
				DSP4_WRITE_WORD((x_scroll + 0x8000) >> 16);

				/* update memory address */
				DSP4.poly_ptr[0][0] -= 4;

				/* update screen values */
				x_scroll += px_dx;
				y_scroll += py_dy;
			}
		}

		/* Post-update */

		/* update new viewer (x, y, scroll) to last raster line drawn */
		DSP4.view_x1 = DSP4.view_x2;
		DSP4.view_y1 = DSP4.view_y2;
		DSP4.view_xofs1 = DSP4.view_xofs2;
		DSP4.view_yofs1 = DSP4.view_yofs2;

		/* add deltas for projection lines */
		DSP4.world_dx += SEX8(DSP4.world_ddx);
		DSP4.world_dy += SEX8(DSP4.world_ddy);

		/* update projection lines */
		DSP4.world_x += (DSP4.world_dx + DSP4.world_xenv);
		DSP4.world_y += DSP4.world_dy;

		/* command check */

		/* scan next command */
		DSP4.in_count = 2;
		DSP4_WAIT(1);

	resume1:
		/* inspect input */
		DSP4.distance = DSP4_READ_WORD();

		if (DSP4.distance == -0x8000) /* terminate op */
			break;

		/* already have 2 bytes in queue */
		DSP4.in_count = 6;
		DSP4_WAIT(2);

	resume2:
		/* inspect inputs */
		DSP4.world_ddy = DSP4_READ_WORD();
		DSP4.world_ddx = DSP4_READ_WORD();
		DSP4.view_yofsenv = DSP4_READ_WORD();

		/* no envelope here */
		DSP4.world_xenv = 0;
	} while (1);

	DSP4.waiting4command = true;
}

static void DSP4_OP0E()
{
	DSP4.OAM_RowMax = 16;
	memset(DSP4.OAM_Row, 0, 64);
}

static void DSP4_OP0F()
{
	DSP4.waiting4command = false;

	switch (DSP4.Logic) /* op flow control */
	{
		case 1:
			goto resume1;
			break;
		case 2:
			goto resume2;
			break;
		case 3:
			goto resume3;
			break;
		case 4:
			goto resume4;
			break;
	}

	/* process initial inputs */

	/* sort inputs */
	DSP4_READ_WORD(); /* 0x0000 */
	DSP4.world_y = DSP4_READ_DWORD();
	DSP4.poly_bottom[0][0] = DSP4_READ_WORD();
	DSP4.poly_top[0][0] = DSP4_READ_WORD();
	DSP4.poly_cx[1][0] = DSP4_READ_WORD();
	DSP4.viewport_bottom = DSP4_READ_WORD();
	DSP4.world_x = DSP4_READ_DWORD();
	DSP4.poly_cx[0][0] = DSP4_READ_WORD();
	DSP4.poly_ptr[0][0] = DSP4_READ_WORD();
	DSP4.world_yofs = DSP4_READ_WORD();
	DSP4.world_dy = DSP4_READ_DWORD();
	DSP4.world_dx = DSP4_READ_DWORD();
	DSP4.distance = DSP4_READ_WORD();
	DSP4_READ_WORD(); /* 0x0000 */
	DSP4.world_xenv = DSP4_READ_DWORD();
	DSP4.world_ddy = DSP4_READ_WORD();
	DSP4.world_ddx = DSP4_READ_WORD();
	DSP4.view_yofsenv = DSP4_READ_WORD();

	/* initial (x, y, offset) at starting raster line */
	DSP4.view_x1 = (DSP4.world_x + DSP4.world_xenv) >> 16;
	DSP4.view_y1 = DSP4.world_y >> 16;
	DSP4.view_xofs1 = DSP4.world_x >> 16;
	DSP4.view_yofs1 = DSP4.world_yofs;
	DSP4.view_turnoff_x = 0;
	DSP4.view_turnoff_dx = 0;

	/* first raster line */
	DSP4.poly_raster[0][0] = DSP4.poly_bottom[0][0];

	do
	{
		/* process one iteration of projection */

		/* perspective projection of world (x, y, scroll) points */
		/* based on the current projection lines */
		DSP4.view_x2 = ((DSP4.world_x + DSP4.world_xenv) >> 16) * DSP4.distance >> 15;
		DSP4.view_y2 = (DSP4.world_y >> 16) * DSP4.distance >> 15;
		DSP4.view_xofs2 = DSP4.view_x2;
		DSP4.view_yofs2 = (DSP4.world_yofs * DSP4.distance >> 15) + DSP4.poly_bottom[0][0] - DSP4.view_y2;

		/* 1. World x-location before transformation */
		/* 2. Viewer x-position at the next */
		/* 3. World y-location before perspective projection */
		/* 4. Viewer y-position below the horizon */
		/* 5. Number of raster lines drawn in this iteration */
		DSP4_CLEAR_OUT();
		DSP4_WRITE_WORD((DSP4.world_x + DSP4.world_xenv) >> 16);
		DSP4_WRITE_WORD(DSP4.view_x2);
		DSP4_WRITE_WORD(DSP4.world_y >> 16);
		DSP4_WRITE_WORD(DSP4.view_y2);

		/* determine # of raster lines used */
		DSP4.segments = DSP4.poly_raster[0][0] - DSP4.view_y2;

		/* prevent overdraw */
		if (DSP4.view_y2 >= DSP4.poly_raster[0][0])
			DSP4.segments = 0;
		else
			DSP4.poly_raster[0][0] = DSP4.view_y2;

		if (DSP4.view_y2 < DSP4.poly_top[0][0]) /* don't draw outside the window */
		{
			DSP4.segments = 0;

			if (DSP4.view_y1 >= DSP4.poly_top[0][0]) /* flush remaining raster lines */
				DSP4.segments = DSP4.view_y1 - DSP4.poly_top[0][0];
		}

		DSP4_WRITE_WORD(DSP4.segments);

		if (DSP4.segments) /* scan next command if no SR check needed */
		{
			int32_t px_dx, py_dy;
			int32_t x_scroll, y_scroll;

			for (DSP4.lcv = 0; DSP4.lcv < 4; DSP4.lcv++)
			{
				int16_t dist;
				int16_t color, red, green, blue;

				/* grab inputs */
				DSP4.in_count = 4;
				DSP4_WAIT(1);

			resume1:
				dist = DSP4_READ_WORD();
				color = DSP4_READ_WORD();

				/* U1+B5+G5+R5 */
				red = color & 0x1f;
				green = (color >> 5) & 0x1f;
				blue = (color >> 10) & 0x1f;

				/* dynamic lighting */
				red = (red * dist >> 15) & 0x1f;
				green = (green * dist >> 15) & 0x1f;
				blue = (blue * dist >> 15) & 0x1f;
				color = red | (green << 5) | (blue << 10);
				DSP4_CLEAR_OUT();
				DSP4_WRITE_WORD(color);
			}

			/* linear interpolation (lerp) between projected points */
			px_dx = (DSP4.view_xofs2 - DSP4.view_xofs1) * DSP4_Inverse(DSP4.segments) << 1;
			py_dy = (DSP4.view_yofs2 - DSP4.view_yofs1) * DSP4_Inverse(DSP4.segments) << 1;

			/* starting step values */
			x_scroll = SEX16(DSP4.poly_cx[0][0] + DSP4.view_xofs1);
			y_scroll = SEX16(-DSP4.viewport_bottom + DSP4.view_yofs1 + DSP4.view_yofsenv + DSP4.poly_cx[1][0] - DSP4.world_yofs);

			for (DSP4.lcv = 0; DSP4.lcv < DSP4.segments; DSP4.lcv++) /* rasterize line */
			{
				/* 1. HDMA memory pointer */
				/* 2. vertical scroll offset ($210E) */
				/* 3. horizontal scroll offset ($210D) */
				DSP4_WRITE_WORD(DSP4.poly_ptr[0][0]);
				DSP4_WRITE_WORD((y_scroll + 0x8000) >> 16);
				DSP4_WRITE_WORD((x_scroll + 0x8000) >> 16);

				/* update memory address */
				DSP4.poly_ptr[0][0] -= 4;

				/* update screen values */
				x_scroll += px_dx;
				y_scroll += py_dy;
			}
		}

		/* Post-update */

		/* update new viewer (x, y, scroll) to last raster line drawn */
		DSP4.view_x1 = DSP4.view_x2;
		DSP4.view_y1 = DSP4.view_y2;
		DSP4.view_xofs1 = DSP4.view_xofs2;
		DSP4.view_yofs1 = DSP4.view_yofs2;

		/* add deltas for projection lines */
		DSP4.world_dx += SEX8(DSP4.world_ddx);
		DSP4.world_dy += SEX8(DSP4.world_ddy);

		/* update projection lines */
		DSP4.world_x += (DSP4.world_dx + DSP4.world_xenv);
		DSP4.world_y += DSP4.world_dy;

		/* update road turnoff position */
		DSP4.view_turnoff_x += DSP4.view_turnoff_dx;

		/* command check */

		/* scan next command */
		DSP4.in_count = 2;
		DSP4_WAIT(2);

	resume2:
		/* check for termination */
		DSP4.distance = DSP4_READ_WORD();

		if (DSP4.distance == -0x8000)
			break;

		if ((uint16_t) DSP4.distance == 0x8001) /* road splice */
		{
			DSP4.in_count = 6;
			DSP4_WAIT(3);

		resume3:
			DSP4.distance = DSP4_READ_WORD();
			DSP4.view_turnoff_x = DSP4_READ_WORD();
			DSP4.view_turnoff_dx = DSP4_READ_WORD();

			/* factor in new changes */
			DSP4.view_x1 += (DSP4.view_turnoff_x * DSP4.distance >> 15);
			DSP4.view_xofs1 += (DSP4.view_turnoff_x * DSP4.distance >> 15);

			/* update stepping values */
			DSP4.view_turnoff_x += DSP4.view_turnoff_dx;

			DSP4.in_count = 2;
			DSP4_WAIT(2);
		}

		/* already have 2 bytes in queue */
		DSP4.in_count = 6;
		DSP4_WAIT(4);

	resume4:
		/* inspect inputs */
		DSP4.world_ddy = DSP4_READ_WORD();
		DSP4.world_ddx = DSP4_READ_WORD();
		DSP4.view_yofsenv = DSP4_READ_WORD();

		/* no envelope here */
		DSP4.world_xenv = 0;
	} while (1);

	/* terminate op */
	DSP4.waiting4command = true;
}

static void DSP4_OP10()
{
	DSP4.waiting4command = false;

	switch (DSP4.Logic) /* op flow control */
	{
		case 1:
			goto resume1;
			break;
		case 2:
			goto resume2;
			break;
		case 3:
			goto resume3;
			break;
	}

	/* sort inputs */
	DSP4_READ_WORD(); /* 0x0000 */
	DSP4.world_y = DSP4_READ_DWORD();
	DSP4.poly_bottom[0][0] = DSP4_READ_WORD();
	DSP4.poly_top[0][0] = DSP4_READ_WORD();
	DSP4.poly_cx[1][0] = DSP4_READ_WORD();
	DSP4.viewport_bottom = DSP4_READ_WORD();
	DSP4.world_x = DSP4_READ_DWORD();
	DSP4.poly_cx[0][0] = DSP4_READ_WORD();
	DSP4.poly_ptr[0][0] = DSP4_READ_WORD();
	DSP4.world_yofs = DSP4_READ_WORD();
	DSP4.distance = DSP4_READ_WORD();
	DSP4.view_y2 = DSP4_READ_WORD();
	DSP4.view_dy = DSP4_READ_WORD() * DSP4.distance >> 15;
	DSP4.view_x2 = DSP4_READ_WORD();
	DSP4.view_dx = DSP4_READ_WORD() * DSP4.distance >> 15;
	DSP4.view_yofsenv = DSP4_READ_WORD();

	/* initial (x, y, offset) at starting raster line */
	DSP4.view_x1 = DSP4.world_x >> 16;
	DSP4.view_y1 = DSP4.world_y >> 16;
	DSP4.view_xofs1 = DSP4.view_x1;
	DSP4.view_yofs1 = DSP4.world_yofs;

	/* first raster line */
	DSP4.poly_raster[0][0] = DSP4.poly_bottom[0][0];

	do
	{
		/* process one iteration of projection */

		/* add shaping */
		DSP4.view_x2 += DSP4.view_dx;
		DSP4.view_y2 += DSP4.view_dy;

		/* vertical scroll calculation */
		DSP4.view_xofs2 = DSP4.view_x2;
		DSP4.view_yofs2 = (DSP4.world_yofs * DSP4.distance >> 15) + DSP4.poly_bottom[0][0] - DSP4.view_y2;

		/* 1. Viewer x-position at the next */
		/* 2. Viewer y-position below the horizon */
		/* 3. Number of raster lines drawn in this iteration */
		DSP4_CLEAR_OUT();
		DSP4_WRITE_WORD(DSP4.view_x2);
		DSP4_WRITE_WORD(DSP4.view_y2);

		/* determine # of raster lines used */
		DSP4.segments = DSP4.view_y1 - DSP4.view_y2;

		/* prevent overdraw */
		if (DSP4.view_y2 >= DSP4.poly_raster[0][0])
			DSP4.segments = 0;
		else
			DSP4.poly_raster[0][0] = DSP4.view_y2;

		if (DSP4.view_y2 < DSP4.poly_top[0][0]) /* don't draw outside the window */
		{
			DSP4.segments = 0;

			if (DSP4.view_y1 >= DSP4.poly_top[0][0]) /* flush remaining raster lines */
				DSP4.segments = DSP4.view_y1 - DSP4.poly_top[0][0];
		}

		DSP4_WRITE_WORD(DSP4.segments);

		if (DSP4.segments) /* scan next command if no SR check needed */
		{
			for (DSP4.lcv = 0; DSP4.lcv < 4; DSP4.lcv++)
			{
				int16_t dist;
				int16_t color, red, green, blue;

				/* grab inputs */
				DSP4.in_count = 4;
				DSP4_WAIT(1);

			resume1:
				dist = DSP4_READ_WORD();
				color = DSP4_READ_WORD();

				/* U1+B5+G5+R5 */
				red = color & 0x1f;
				green = (color >> 5) & 0x1f;
				blue = (color >> 10) & 0x1f;

				/* dynamic lighting */
				red = (red * dist >> 15) & 0x1f;
				green = (green * dist >> 15) & 0x1f;
				blue = (blue * dist >> 15) & 0x1f;
				color = red | (green << 5) | (blue << 10);
				DSP4_CLEAR_OUT();
				DSP4_WRITE_WORD(color);
			}
		}

		if (DSP4.segments) /* scan next command if no SR check needed */
		{
			int32_t px_dx, py_dy;
			int32_t x_scroll, y_scroll;

			/* linear interpolation (lerp) between projected points */
			px_dx = (DSP4.view_xofs2 - DSP4.view_xofs1) * DSP4_Inverse(DSP4.segments) << 1;
			py_dy = (DSP4.view_yofs2 - DSP4.view_yofs1) * DSP4_Inverse(DSP4.segments) << 1;

			/* starting step values */
			x_scroll = SEX16(DSP4.poly_cx[0][0] + DSP4.view_xofs1);
			y_scroll = SEX16(-DSP4.viewport_bottom + DSP4.view_yofs1 + DSP4.view_yofsenv + DSP4.poly_cx[1][0] - DSP4.world_yofs);

			for (DSP4.lcv = 0; DSP4.lcv < DSP4.segments; DSP4.lcv++) /* rasterize line */
			{
				/* 1. HDMA memory pointer (bg2) */
				/* 2. vertical scroll offset ($2110) */
				/* 3. horizontal scroll offset ($210F) */
				DSP4_WRITE_WORD(DSP4.poly_ptr[0][0]);
				DSP4_WRITE_WORD((y_scroll + 0x8000) >> 16);
				DSP4_WRITE_WORD((x_scroll + 0x8000) >> 16);

				/* update memory address */
				DSP4.poly_ptr[0][0] -= 4;

				/* update screen values */
				x_scroll += px_dx;
				y_scroll += py_dy;
			}
		}

		/* Post-update */

		/* update new viewer (x, y, scroll) to last raster line drawn */
		DSP4.view_x1 = DSP4.view_x2;
		DSP4.view_y1 = DSP4.view_y2;
		DSP4.view_xofs1 = DSP4.view_xofs2;
		DSP4.view_yofs1 = DSP4.view_yofs2;

		/* command check */

		/* scan next command */
		DSP4.in_count = 2;
		DSP4_WAIT(2);

	resume2:
		/* check for opcode termination */
		DSP4.distance = DSP4_READ_WORD();

		if (DSP4.distance == -0x8000)
			break;

		/* already have 2 bytes in queue */
		DSP4.in_count = 10;
		DSP4_WAIT(3);

	resume3:
		/* inspect inputs */
		DSP4.view_y2 = DSP4_READ_WORD();
		DSP4.view_dy = DSP4_READ_WORD() * DSP4.distance >> 15;
		DSP4.view_x2 = DSP4_READ_WORD();
		DSP4.view_dx = DSP4_READ_WORD() * DSP4.distance >> 15;
	} while (1);

	DSP4.waiting4command = true;
}

static void DSP4_OP11(int16_t A, int16_t B, int16_t C, int16_t D, int16_t* M)
{
	/* 0x155 = 341 = Horizontal Width of the Screen */
	*M = ((A * 0x0155 >> 2) & 0xf000) | ((B * 0x0155 >> 6) & 0x0f00) | ((C * 0x0155 >> 10) & 0x00f0) | ((D * 0x0155 >> 14) & 0x000f);
}

static void DSP4_SetByte()
{
	/* clear pending read */
	if (DSP4.out_index < DSP4.out_count)
	{
		DSP4.out_index++;
		return;
	}

	if (DSP4.waiting4command)
	{
		if (DSP4.half_command)
		{
			DSP4.command |= (DSP4.byte << 8);
			DSP4.in_index = 0;
			DSP4.waiting4command = false;
			DSP4.half_command = false;
			DSP4.out_count = 0;
			DSP4.out_index = 0;
			DSP4.Logic = 0;

			switch (DSP4.command)
			{
				case 0x0000:
					DSP4.in_count = 4;
					break;
				case 0x0001:
					DSP4.in_count = 44;
					break;
				case 0x0003:
					DSP4.in_count = 0;
					break;
				case 0x0005:
					DSP4.in_count = 0;
					break;
				case 0x0006:
					DSP4.in_count = 0;
					break;
				case 0x0007:
					DSP4.in_count = 34;
					break;
				case 0x0008:
					DSP4.in_count = 90;
					break;
				case 0x0009:
					DSP4.in_count = 14;
					break;
				case 0x000a:
					DSP4.in_count = 6;
					break;
				case 0x000b:
					DSP4.in_count = 6;
					break;
				case 0x000d:
					DSP4.in_count = 42;
					break;
				case 0x000e:
					DSP4.in_count = 0;
					break;
				case 0x000f:
					DSP4.in_count = 46;
					break;
				case 0x0010:
					DSP4.in_count = 36;
					break;
				case 0x0011:
					DSP4.in_count = 8;
					break;
				default:
					DSP4.waiting4command = true;
					break;
			}
		}
		else
		{
			DSP4.command = DSP4.byte;
			DSP4.half_command = true;
		}
	}
	else
	{
		DSP4.parameters[DSP4.in_index] = DSP4.byte;
		DSP4.in_index++;
	}

	if (DSP4.waiting4command || DSP4.in_count != DSP4.in_index)
		return;

	/* Actually execute the command */
	DSP4.waiting4command = true;
	DSP4.out_index = 0;
	DSP4.in_index = 0;

	switch (DSP4.command)
	{
		case 0x0000: /* 16-bit multiplication */
		{
			int16_t multiplier, multiplicand;
			int32_t product;
			multiplier = DSP4_READ_WORD();
			multiplicand = DSP4_READ_WORD();
			DSP4_Multiply(multiplicand, multiplier, &product);
			DSP4_CLEAR_OUT();
			DSP4_WRITE_WORD(product);
			DSP4_WRITE_WORD(product >> 16);
			break;
		}
		case 0x0001: /* single-player track projection */
			DSP4_OP01();
			break;
		case 0x0003: /* single-player selection */
			DSP4_OP03();
			break;
		case 0x0005: /* clear OAM */
			DSP4_OP05();
			break;
		case 0x0006: /* transfer OAM */
			DSP4_OP06();
			break;
		case 0x0007: /* single-player track turnoff projection */
			DSP4_OP07();
			break;
		case 0x0008: /* solid polygon projection */
			DSP4_OP08();
			break;
		case 0x0009: /* sprite projection */
			DSP4_OP09();
			break;
		case 0x000A: /* unknown */
		{
			DSP4_READ_WORD();
			int16_t in2a = DSP4_READ_WORD();
			DSP4_READ_WORD();
			int16_t out1a, out2a, out3a, out4a;
			DSP4_OP0A(in2a, &out2a, &out1a, &out4a, &out3a);
			DSP4_CLEAR_OUT();
			DSP4_WRITE_WORD(out1a);
			DSP4_WRITE_WORD(out2a);
			DSP4_WRITE_WORD(out3a);
			DSP4_WRITE_WORD(out4a);
			break;
		}
		case 0x000B: /* set OAM */
		{
			int16_t sp_x = DSP4_READ_WORD();
			int16_t sp_y = DSP4_READ_WORD();
			int16_t sp_attr = DSP4_READ_WORD();
			bool draw = true;
			DSP4_CLEAR_OUT();
			DSP4_OP0B(&draw, sp_x, sp_y, sp_attr, 0, 1);
			break;
		}
		case 0x000D: /* multi-player track projection */
			DSP4_OP0D();
			break;
		case 0x000E: /* multi-player selection */
			DSP4_OP0E();
			break;
		case 0x000F: /* single-player track projection with lighting */
			DSP4_OP0F();
			break;
		case 0x0010: /* single-player track turnoff projection with lighting */
			DSP4_OP10();
			break;
		case 0x0011: /* unknown: horizontal mapping command */
		{
			int16_t a, b, c, d, m;
			d = DSP4_READ_WORD();
			c = DSP4_READ_WORD();
			b = DSP4_READ_WORD();
			a = DSP4_READ_WORD();
			DSP4_OP11(a, b, c, d, &m);
			DSP4_CLEAR_OUT();
			DSP4_WRITE_WORD(m);
			break;
		}
		default:
			break;
	}
}

static void DSP4_GetByte()
{
	if (!DSP4.out_count)
	{
		DSP4.byte = 0xff;
		return;
	}

	DSP4.byte = (uint8_t) DSP4.output[DSP4.out_index & 0x1FF];
	DSP4.out_index++;

	if (DSP4.out_count == DSP4.out_index)
		DSP4.out_count = 0;
}

void DSP4SetByte(uint8_t byte, uint16_t address)
{
	if (address >= DSP0.boundary)
		return;

	DSP4.byte = byte;
	DSP4.address = address;
	DSP4_SetByte();
}

uint8_t DSP4GetByte(uint16_t address)
{
	if (address >= DSP0.boundary)
		return 0x80;

	DSP4.address = address;
	DSP4_GetByte();
	return DSP4.byte;
}
