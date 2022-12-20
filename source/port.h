#ifndef CHIMERASNES_PORT_H_
#define CHIMERASNES_PORT_H_

#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <libretro.h>
#include <retro_inline.h>

/* This is used to disable the 16-bit graphics mode checks sprinkled
   throughout the code, if the pixel format is always 16-bit. */
#ifdef PSP
	#define PIXEL_FORMAT BGR555
#else
	#define PIXEL_FORMAT RGB565
#endif

#include "pixform.h"

#ifndef _WIN32
	#ifndef PATH_MAX
		#define PATH_MAX 1024
	#endif

	#ifndef _MAX_DIR
		#define _MAX_DIR PATH_MAX
	#endif

	#ifndef _MAX_DRIVE
		#define _MAX_DRIVE 1
	#endif

	#ifndef _MAX_FNAME
		#define _MAX_FNAME PATH_MAX
	#endif

	#ifndef _MAX_EXT
		#define _MAX_EXT PATH_MAX
	#endif

	#ifndef _MAX_PATH
		#define _MAX_PATH PATH_MAX
	#endif
#else /* _WIN32 */
	#define strcasecmp  stricmp
	#define strncasecmp strnicmp
#endif

#define SLASH_STR  "/"
#define SLASH_CHAR '/'

#ifndef MSB_FIRST
	#define READ_WORD(s)       (*(uint16_t*) (s))
	#define READ_3WORD(s)      (*(uint32_t*) (s) & 0x00ffffff)
	#define READ_DWORD(s)      (*(uint32_t*) (s))
	#define READ_2BYTES(s)     (*(uint8_t*) (s) | (*((uint8_t*) (s) + 1) << 8))
	#define WRITE_WORD(s, d)   (*(uint16_t*) (s) = (d))
	#define WRITE_3WORD(s, d)  (*(uint16_t*) (s) = (uint16_t) (d), *((uint8_t*) (s) + 2) = (uint8_t) ((d) >> 16))
	#define WRITE_DWORD(s, d)  (*(uint32_t*) (s) = (d))
	#define WRITE_2BYTES(s, d) (*(uint8_t*) (s) = (uint8_t) (d), *((uint8_t*) (s) + 1) = (uint8_t) ((d) >> 8))
#else
	#define READ_WORD(s)       (*(uint8_t*) (s) | (*((uint8_t*) (s) + 1) << 8))
	#define READ_3WORD(s)      (*(uint8_t*) (s) | (*((uint8_t*) (s) + 1) << 8) | (*((uint8_t*) (s) + 2) << 16))
	#define READ_DWORD(s)      (*(uint8_t*) (s) | (*((uint8_t*) (s) + 1) << 8) | (*((uint8_t*) (s) + 2) << 16) | (*((uint8_t*) (s) + 3) << 24))
	#define READ_2BYTES(s)     (*(uint16_t*) (s))
	#define WRITE_WORD(s, d)   (*(uint8_t*) (s) = (uint8_t) (d), *((uint8_t*) (s) + 1) = (uint8_t) ((d) >> 8))
	#define WRITE_3WORD(s, d)  (*(uint8_t*) (s) = (uint8_t) (d), *((uint8_t*) (s) + 1) = (uint8_t) ((d) >> 8), *((uint8_t*) (s) + 2) = (uint8_t) ((d) >> 16))
	#define WRITE_DWORD(s, d)  (*(uint8_t*) (s) = (uint8_t) (d), *((uint8_t*) (s) + 1) = (uint8_t) ((d) >> 8), *((uint8_t*) (s) + 2) = (uint8_t) ((d) >> 16), *((uint8_t*) (s) + 3) = (uint8_t) ((d) >> 24))
	#define WRITE_2BYTES(s, d) (*(uint16_t*) (s) = (d))
#endif

const char* GetBIOSDir();

static INLINE uint32_t swap_dword(uint32_t val)
{
#ifdef __GNUC__
	return __builtin_bswap32(val);
#else
	return (((val & 0x000000ff) << 24) |
			((val & 0x0000ff00) << 8)  |
			((val & 0x00ff0000) >> 8)  |
			((val & 0xff000000) >> 24));
#endif
}
#endif
