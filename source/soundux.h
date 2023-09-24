#ifndef CHIMERASNES_SOUNDUX_H_
#define CHIMERASNES_SOUNDUX_H_

enum
{
	SOUND_SAMPLE = 0,
	SOUND_NOISE
};

enum
{
	SOUND_SILENT,
	SOUND_ATTACK,
	SOUND_DECAY,
	SOUND_SUSTAIN,
	SOUND_RELEASE,
	SOUND_GAIN,
	SOUND_INCREASE_LINEAR,
	SOUND_INCREASE_BENT_LINE,
	SOUND_DECREASE_LINEAR,
	SOUND_DECREASE_EXPONENTIAL
};

enum
{
	MODE_NONE = SOUND_SILENT,
	MODE_ADSR,
	MODE_RELEASE,
	MODE_GAIN,
	MODE_INCREASE_LINEAR,
	MODE_INCREASE_BENT_LINE,
	MODE_DECREASE_LINEAR,
	MODE_DECREASE_EXPONENTIAL
};

enum
{
	SNES_SAMPLE_RATE    = 32000,
	MAX_SAMPLE_RATE     = 48000,
	MAX_ENVELOPE_HEIGHT = 127,
	ENVELOPE_SHIFT      = 7,
	MAX_VOLUME          = 127,
	VOLUME_SHIFT        = 7,
	SOUND_DECODE_LENGTH = 16,
	NUM_CHANNELS        = 8,
	FIRBUF              = 16,
	SOUND_BUFS          = 4,
	ECHOBUF             = (((255 * 64 * MAX_SAMPLE_RATE / SNES_SAMPLE_RATE) * 2) + ((240 * MAX_SAMPLE_RATE / 1000) * 2))
};

#define CLAMP(x, low, high) \
	(((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

#define INT8_CLAMP(n) \
	((int8_t)  CLAMP(n, -128,   127))

#define INT16_CLAMP(n) \
	((int16_t) CLAMP(n, -32768, 32767))

typedef struct
{
	int16_t  next_sample;
	int16_t  decoded[16];
	int32_t  envx;
	int32_t  mode;
	int32_t  state;
	int32_t  type;
	uint32_t count;
	uint32_t block_pointer;
	uint32_t sample_pointer;
	int16_t* block;
} Channel;

typedef struct
{
	int32_t echo_buffer_size;
	int32_t echo_enable;
	int32_t echo_feedback;
	int32_t echo_ptr;
	int32_t echo_write_enabled;
	int32_t pitch_mod;
	Channel channels[NUM_CHANNELS];
} SSoundData;

extern SSoundData SoundData;

void SetEchoFeedback(int32_t echo_feedback);
void SetEchoEnable(uint8_t byte);
void SetEchoDelay(int32_t rate, int32_t delay);
void SetFilterCoefficient(int32_t tap, int32_t value);
void ResetSound(bool full);
void FixSoundAfterSnapshotLoad();
#endif
