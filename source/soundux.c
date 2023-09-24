#include "chisnes.h"
#include "apu.h"
#include "snesapu.h"
#include "soundux.h"

extern int8_t   FilterTaps[8];
extern int16_t  Loop[FIRBUF];
extern int32_t  Echo[ECHOBUF];
extern uint32_t Z;

void SetEchoEnable(uint8_t byte)
{
	if (!SoundData.echo_write_enabled)
		byte = 0;

	if (byte && !SoundData.echo_enable)
	{
		memset(Echo, 0, sizeof(Echo));
		memset(Loop, 0, sizeof(Loop));
	}

	SoundData.echo_enable = byte;
}

void SetEchoFeedback(int32_t feedback)
{
	feedback = INT8_CLAMP(feedback);
	SoundData.echo_feedback = feedback;
}

void SetEchoDelay(int32_t rate, int32_t delay)
{
	SoundData.echo_buffer_size = ((512 * delay * rate) / SNES_SAMPLE_RATE) << 1;

	if (SoundData.echo_buffer_size)
		SoundData.echo_ptr %= SoundData.echo_buffer_size;
	else
		SoundData.echo_ptr = 0;

	SetEchoEnable(APU.DSP[APU_EON]);
}

void FixSoundAfterSnapshotLoad()
{
	SoundData.echo_write_enabled = !(APU.DSP[APU_FLG] & 0x20);
	SetEchoFeedback((int8_t) APU.DSP[APU_EFB]);
	SetFilterCoefficient(0, (int8_t) APU.DSP[APU_C0]);
	SetFilterCoefficient(1, (int8_t) APU.DSP[APU_C1]);
	SetFilterCoefficient(2, (int8_t) APU.DSP[APU_C2]);
	SetFilterCoefficient(3, (int8_t) APU.DSP[APU_C3]);
	SetFilterCoefficient(4, (int8_t) APU.DSP[APU_C4]);
	SetFilterCoefficient(5, (int8_t) APU.DSP[APU_C5]);
	SetFilterCoefficient(6, (int8_t) APU.DSP[APU_C6]);
	SetFilterCoefficient(7, (int8_t) APU.DSP[APU_C7]);
	RestoreAPUDSP();
}

void SetFilterCoefficient(int32_t tap, int32_t value)
{
	FilterTaps[tap & 7] = value;
}

void ResetSound(bool full)
{
	int32_t i;

	for (i = 0; i < 8; i++)
	{
		SoundData.channels[i].state = SOUND_SILENT;
		SoundData.channels[i].mode = MODE_NONE;
		SoundData.channels[i].type = SOUND_SAMPLE;
		SoundData.channels[i].count = 0;
		SoundData.channels[i].envx = 0;
		SoundData.echo_ptr = 0;
		SoundData.echo_feedback = 0;
		SoundData.echo_buffer_size = 1;
	}

	if (full)
	{
		SoundData.echo_enable = 0;
		SoundData.echo_write_enabled = 0;
		SoundData.pitch_mod = 0;
		memset(Echo, 0, sizeof(Echo));
		memset(Loop, 0, sizeof(Loop));
	}
}
