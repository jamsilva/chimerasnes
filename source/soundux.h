#ifndef CHIMERASNES_SOUNDUX_H_
#define CHIMERASNES_SOUNDUX_H_

enum
{
	SOUND_SAMPLE,
	SOUND_NOISE
};

enum
{
	MODE_NONE,
	MODE_ADSR,
	MODE_RELEASE,
	MODE_GAIN,
	MODE_INCREASE_LINEAR,
	MODE_INCREASE_BENT_LINE,
	MODE_DECREASE_LINEAR,
	MODE_DECREASE_EXPONENTIAL
};

#define SNES_SAMPLE_RATE    32000
#define MAX_SAMPLE_RATE     48000
#define MAX_ENVELOPE_HEIGHT 127
#define ENVELOPE_SHIFT      7
#define MAX_VOLUME          127
#define VOLUME_SHIFT        7
#define SOUND_DECODE_LENGTH 16

#define NUM_CHANNELS 8
#define FIRBUF       16
#define SOUND_BUFS   4

#define CLAMP(x, low, high) \
	(((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

#define INT8_CLAMP(n) \
	((int8_t)  CLAMP(n, -128,   127))

#define INT16_CLAMP(n) \
	((int16_t) CLAMP(n, -32768, 32767))

typedef struct
{
	int16_t  decoded[16];
	int32_t  envx;
	int32_t  mode;
	int32_t  type;
	int32_t  _Channel_PAD1 : 32;
	uint32_t block_pointer;
	uint32_t sample_pointer;
	int16_t* block;
} Channel;

void SetFilterCoefficient(int32_t tap, int32_t value);
#endif
