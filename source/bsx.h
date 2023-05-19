#ifndef CHIMERASNES_BSX_H_
#define CHIMERASNES_BSX_H_

#include <streams/file_stream.h>

typedef struct
{
	bool     first           : 1;
	bool     pf_latch_enable : 1;
	bool     dt_latch_enable : 1;
	uint8_t  _STREAM_T_PAD1  : 5;
	uint8_t  count;
	uint16_t queue;
	RFILE*   file;
} stream_t;

typedef struct
{
	bool     dirty          : 1; /* Changed register values */
	bool     dirty2         : 1; /* Changed register values */
	bool     write_enable   : 1; /* ROM write protection */
	bool     read_enable    : 1; /* Allow card vendor reading */
	bool     flash_csr      : 1;
	bool     flash_gsr      : 1;
	bool     flash_bsr      : 1;
	bool     flash_mode     : 1;
	uint8_t  out_index;
	uint8_t  MMC[16];
	uint8_t  prevMMC[16];
	uint8_t  PPU[32];
	uint32_t flash_command; /* Flash command */
	uint32_t old_write;     /* Previous flash write address */
	uint32_t new_write;     /* Current flash write address */
	stream_t sat_stream1;
	stream_t sat_stream2;
} SBSX;

extern SBSX BSX;

uint8_t  GetBSX(uint32_t address);
void     SetBSX(uint8_t byte, uint32_t address);
uint8_t  GetBSXPPU(uint16_t address);
void     SetBSXPPU(uint8_t byte, uint16_t address);
uint8_t* GetBasePointerBSX(uint32_t address);
void     InitBSX();
void     ResetBSX();
void     BSXPostLoadState();

#endif
