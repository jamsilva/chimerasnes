#ifndef CHIMERASNES_DSP_H_
#define CHIMERASNES_DSP_H_

enum
{
	M_DSP1_LOROM_S,
	M_DSP1_LOROM_L,
	M_DSP1_HIROM,
	M_DSP2_LOROM,
	M_DSP3_LOROM,
	M_DSP4_LOROM
};

typedef struct
{
	uint32_t maptype;
	uint32_t boundary;
} SDSP0;

typedef struct
{
	bool     waiting4command : 1;
	bool     first_parameter : 1;
	int8_t   _SDSP1_PAD1     : 6;
	uint8_t  command;
	uint8_t  parameters[512];
	uint8_t  output[512];
	int16_t  CentreX;
	int16_t  CentreY;
	int16_t  VOffset;
	int16_t  VPlane_C;
	int16_t  VPlane_E;
	int16_t  _SDSP1_PAD2     : 16;
	uint32_t in_count;
	uint32_t in_index;
	uint32_t out_count;
	uint32_t out_index;

	/* Azimuth and Zenith angles */
	int16_t SinAas;
	int16_t CosAas;
	int16_t SinAzs;
	int16_t CosAzs;

	/* Clipped Zenith angle */
	int16_t SinAZS;
	int16_t CosAZS;
	int16_t SecAZS_C1;
	int16_t SecAZS_E1;
	int16_t SecAZS_C2;
	int16_t SecAZS_E2;

	int16_t Nx;
	int16_t Ny;
	int16_t Nz;
	int16_t Gx;
	int16_t Gy;
	int16_t Gz;
	int16_t C_Les;
	int16_t E_Les;
	int16_t G_Les;

	int16_t matrixA[3][3];
	int16_t matrixB[3][3];
	int16_t matrixC[3][3];

	int16_t Op00Multiplicand;
	int16_t Op00Multiplier;
	int16_t Op00Result;

	int16_t Op20Multiplicand;
	int16_t Op20Multiplier;
	int16_t Op20Result;

	int16_t Op10Coefficient;
	int16_t Op10Exponent;
	int16_t Op10CoefficientR;
	int16_t Op10ExponentR;

	int16_t Op04Angle;
	int16_t Op04Radius;
	int16_t Op04Sin;
	int16_t Op04Cos;

	int16_t Op0CA;
	int16_t Op0CX1;
	int16_t Op0CY1;
	int16_t Op0CX2;
	int16_t Op0CY2;

	int16_t Op02FX;
	int16_t Op02FY;
	int16_t Op02FZ;
	int16_t Op02LFE;
	int16_t Op02LES;
	int16_t Op02AAS;
	int16_t Op02AZS;
	int16_t Op02VOF;
	int16_t Op02VVA;
	int16_t Op02CX;
	int16_t Op02CY;

	int16_t Op0AVS;
	int16_t Op0AA;
	int16_t Op0AB;
	int16_t Op0AC;
	int16_t Op0AD;

	int16_t Op06X;
	int16_t Op06Y;
	int16_t Op06Z;
	int16_t Op06H;
	int16_t Op06V;
	int16_t Op06M;

	int16_t Op01m;
	int16_t Op01Zr;
	int16_t Op01Xr;
	int16_t Op01Yr;

	int16_t Op11m;
	int16_t Op11Zr;
	int16_t Op11Xr;
	int16_t Op11Yr;

	int16_t Op21m;
	int16_t Op21Zr;
	int16_t Op21Xr;
	int16_t Op21Yr;

	int16_t Op0DX;
	int16_t Op0DY;
	int16_t Op0DZ;
	int16_t Op0DF;
	int16_t Op0DL;
	int16_t Op0DU;

	int16_t Op1DX;
	int16_t Op1DY;
	int16_t Op1DZ;
	int16_t Op1DF;
	int16_t Op1DL;
	int16_t Op1DU;

	int16_t Op2DX;
	int16_t Op2DY;
	int16_t Op2DZ;
	int16_t Op2DF;
	int16_t Op2DL;
	int16_t Op2DU;

	int16_t Op03F;
	int16_t Op03L;
	int16_t Op03U;
	int16_t Op03X;
	int16_t Op03Y;
	int16_t Op03Z;

	int16_t Op13F;
	int16_t Op13L;
	int16_t Op13U;
	int16_t Op13X;
	int16_t Op13Y;
	int16_t Op13Z;

	int16_t Op23F;
	int16_t Op23L;
	int16_t Op23U;
	int16_t Op23X;
	int16_t Op23Y;
	int16_t Op23Z;

	int16_t Op14Zr;
	int16_t Op14Xr;
	int16_t Op14Yr;
	int16_t Op14U;
	int16_t Op14F;
	int16_t Op14L;
	int16_t Op14Zrr;
	int16_t Op14Xrr;
	int16_t Op14Yrr;

	int16_t Op0EH;
	int16_t Op0EV;
	int16_t Op0EX;
	int16_t Op0EY;

	int16_t Op0BX;
	int16_t Op0BY;
	int16_t Op0BZ;
	int16_t Op0BS;

	int16_t Op1BX;
	int16_t Op1BY;
	int16_t Op1BZ;
	int16_t Op1BS;

	int16_t Op2BX;
	int16_t Op2BY;
	int16_t Op2BZ;
	int16_t Op2BS;

	int16_t Op28X;
	int16_t Op28Y;
	int16_t Op28Z;
	int16_t Op28R;

	int16_t Op1CX;
	int16_t Op1CY;
	int16_t Op1CZ;
	int16_t Op1CXBR;
	int16_t Op1CYBR;
	int16_t Op1CZBR;
	int16_t Op1CXAR;
	int16_t Op1CYAR;
	int16_t Op1CZAR;
	int16_t Op1CX1;
	int16_t Op1CY1;
	int16_t Op1CZ1;
	int16_t Op1CX2;
	int16_t Op1CY2;
	int16_t Op1CZ2;

	uint16_t Op0FRamsize;
	uint16_t Op0FPass;

	int16_t Op2FUnknown;
	int16_t Op2FSize;

	int16_t Op08X;
	int16_t Op08Y;
	int16_t Op08Z;
	int16_t Op08Ll;
	int16_t Op08Lh;

	int16_t Op18X;
	int16_t Op18Y;
	int16_t Op18Z;
	int16_t Op18R;
	int16_t Op18D;

	int16_t Op38X;
	int16_t Op38Y;
	int16_t Op38Z;
	int16_t Op38R;
	int16_t Op38D;
} SDSP1;

typedef struct
{
	bool     waiting4command : 1;
	bool     Op05HasLen      : 1;
	bool     Op06HasLen      : 1;
	bool     Op0DHasLen      : 1;
	int16_t  _SDSP2_PAD1     : 12;
	uint8_t  command;
	uint8_t  Op05Transparent;
	uint8_t  parameters[512];
	uint8_t  output[512];
	uint16_t Op09Word1;
	uint16_t Op09Word2;
	int32_t  Op05Len;
	int32_t  Op06Len;
	int32_t  Op0DInLen;
	int32_t  Op0DOutLen;
	uint32_t in_count;
	uint32_t in_index;
	uint32_t out_count;
	uint32_t out_index;
} SDSP2;

typedef struct
{
	uint8_t  BaseLength;
	uint8_t  LZLength;
	uint8_t  Bitmap[8];
	uint8_t  Bitplane[8];
	uint8_t  CodeLengths[8];
	int16_t  AddLo;
	int16_t  AddHi;
	uint16_t BaseCode;
	uint16_t BaseCodes;
	uint16_t BitCommand;
	uint16_t BitCount;
	uint16_t BitsLeft;
	uint16_t BMIndex;
	uint16_t BPIndex;
	uint16_t Codewords;
	uint16_t DR;
	uint16_t Index;
	uint16_t LZCode;
	uint16_t MemoryIndex;
	uint16_t Outwords;
	uint16_t ReqBits;
	uint16_t ReqData;
	uint16_t SR;
	uint16_t Symbol;
	int16_t  WinLo;
	int16_t  WinHi;
	uint16_t X;
	uint16_t Y;
	int16_t  op3e_x;
	int16_t  op3e_y;
	int16_t  op1e_cell;
	int16_t  op1e_lcv_radius;
	int16_t  op1e_lcv_steps;
	int16_t  op1e_lcv_turns;
	int16_t  op1e_min_radius;
	int16_t  op1e_max_radius;
	int16_t  op1e_max_path_radius;
	int16_t  op1e_max_search_radius;
	int16_t  op1e_search;
	int16_t  op1e_turn;
	int16_t  op1e_x;
	int16_t  op1e_y;
	int16_t  op1e_cost[0x2000];
	int16_t  op1e_terrain[0x2000];
	int16_t  op1e_weight[0x2000];
	uint16_t Count;
	uint16_t CodeOffsets[8];
	uint16_t Codes[512];
} SDSP3;

typedef struct
{
	bool     waiting4command : 1;
	bool     half_command    : 1;
	bool     sprite_size     : 1;  /* sprite size: 8x8 or 16x16 */
	int16_t  _SDSP4_PAD1     : 13;
	int8_t   Logic;                /* controls op flow */
	uint8_t  byte;
	uint8_t  parameters[512];
	uint8_t  output[512];
	int16_t  lcv;                  /* loop-control variable */
	int16_t  distance;             /* z-position into virtual world */
	int16_t  OAM_bits;             /* offset into OAM table */
	int16_t  OAM_index;            /* index into OAM table */
	int16_t  OAM_RowMax;           /* maximum number of tiles per 8 aligned pixels (row) */
	int16_t  raster;               /* current raster line */
	int16_t  segments;             /* number of raster lines drawn */
	int16_t  sprite_attr;          /* obj attributes */
	int16_t  sprite_clipy;         /* visible line to clip pixels off */
	int16_t  sprite_count;
	int16_t  sprite_x;             /* projected x-pos of sprite */
	int16_t  sprite_y;             /* projected y-pos of sprite */
	int16_t  view_dx;              /* view x-delta factor */
	int16_t  view_dy;              /* view y-delta factor */
	int16_t  view_turnoff_dx;      /* road turnoff delta factor */
	int16_t  view_turnoff_x;       /* road turnoff data */
	int16_t  view_x1;              /* current viewer-x */
	int16_t  view_xofs1;           /* current viewer x-vertical scroll */
	int16_t  view_x2;              /* future viewer-x */
	int16_t  view_xofs2;           /* future viewer x-vertical scroll */
	int16_t  view_y1;              /* current viewer-y */
	int16_t  view_yofs1;           /* current viewer y-vertical scroll */
	int16_t  view_y2;              /* future viewer-y */
	int16_t  view_yofs2;           /* future viewer y-vertical scroll */
	int16_t  view_yofsenv;         /* y-scroll shaping factor */
	int16_t  viewport_cx;          /* x-center of viewport window */
	int16_t  viewport_cy;          /* y-center of render window */
	int16_t  viewport_left;        /* x-left of viewport */
	int16_t  viewport_right;       /* x-right of viewport */
	int16_t  viewport_top;         /* y-top of viewport */
	int16_t  viewport_bottom;      /* y-bottom of viewport */
	int16_t  world_ddx;            /* x-delta increment */
	int16_t  world_ddy;            /* y-delta increment */
	int16_t  world_yofs;           /* world y-vertical scroll */
	int16_t  OAM_attr[16];         /* OAM (size, MSB) data */
	int16_t  OAM_Row[32];          /* current number of tiles per row */
	int16_t  poly_plane[2];        /* previous z-plane distance */
	int16_t  poly_clipLf[2][2];    /* left clip boundary */
	int16_t  poly_clipRt[2][2];    /* right clip boundary */
	int16_t  poly_cx[2][2];        /* center for left/right points */
	int16_t  poly_ptr[2][2];       /* HDMA structure pointers */
	int16_t  poly_raster[2][2];    /* current raster line below horizon */
	int16_t  poly_start[2];        /* current projection points */
	int16_t  poly_top[2][2];       /* top clip boundary */
	int16_t  poly_bottom[2][2];    /* bottom clip boundary */
	uint16_t address;
	uint16_t command;
	int32_t  world_dx;             /* projection line x-delta */
	int32_t  world_dy;             /* projection line y-delta */
	int32_t  world_x;              /* line of x-projection in world */
	int32_t  world_y;              /* line of y-projection in world */
	int32_t  world_xenv;           /* world x-shaping factor */
	uint32_t in_count;
	uint32_t in_index;
	uint32_t out_count;
	uint32_t out_index;
} SDSP4;

extern SDSP0 DSP0;
extern SDSP1 DSP1;
extern SDSP2 DSP2;
extern SDSP3 DSP3;
extern SDSP4 DSP4;

void    ResetDSP();
uint8_t DSP1GetByte(uint16_t);
void    DSP1SetByte(uint8_t, uint16_t);
uint8_t DSP2GetByte(uint16_t);
void    DSP2SetByte(uint8_t, uint16_t);
uint8_t DSP3GetByte(uint16_t);
void    DSP3SetByte(uint8_t, uint16_t);
void    DSP3_Reset();
uint8_t DSP4GetByte(uint16_t);
void    DSP4SetByte(uint8_t, uint16_t);

extern uint8_t (*GetDSP)(uint16_t);
extern void    (*SetDSP)(uint8_t, uint16_t);
#endif
