#ifdef PSP
	#include <pspkernel.h>
	#include <pspgu.h>
#endif

#include <libretro.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>

#include "chisnes.h"
#include "soundux.h"
#include "snesapu.h"
#include "memmap.h"
#include "apu.h"
#include "cheats.h"
#include "display.h"
#include "gfx.h"
#include "cpuexec.h"
#include "spc7110.h"
#include "srtc.h"
#include "sa1.h"
#include "libretro_core_options.h"

static retro_log_printf_t         log_cb         = NULL;
static retro_video_refresh_t      video_cb       = NULL;
static retro_input_poll_t         poll_cb        = NULL;
static retro_input_state_t        input_cb       = NULL;
static retro_audio_sample_batch_t audio_batch_cb = NULL;
static retro_environment_t        environ_cb     = NULL;

static bool libretro_supports_option_categories = false;
static bool libretro_supports_bitmasks = false;

#define FRAME_TIME         (Settings.PAL ? 20000 : 16667)
#define FRAMES_PER_SECOND  (Settings.PAL ? 50    : 60)
#define VIDEO_REFRESH_RATE (SNES_CLOCK_SPEED * 6.0 / (SNES_CYCLES_PER_SCANLINE * SNES_MAX_VCOUNTER))

static int16_t* audio_out_buffer           = NULL;
static float    audio_samples_per_frame    = 0.0f;
static float    audio_samples_accumulator  = 0.0f;

static uint8_t  frameskip_type             = 0;
static uint8_t  frameskip_threshold        = 0;

static bool     retro_audio_buff_active    = false;
static uint8_t  retro_audio_buff_occupancy = 0;
static bool     retro_audio_buff_underrun  = false;

static uint16_t retro_audio_latency        = 0;
static bool     update_audio_latency       = false;
static bool     mute_audio                 = false;

void retro_set_environment(retro_environment_t cb)
{
	struct retro_log_callback log;
	environ_cb = cb;

	if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
		log_cb = log.log;
	else
		log_cb = NULL;

	libretro_supports_option_categories = false;
	libretro_set_core_options(environ_cb, &libretro_supports_option_categories);
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
	video_cb = cb;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
	(void) cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
	audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
	poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
	input_cb = cb;
}

void retro_set_controller_port_device(unsigned in_port, unsigned device)
{
	(void) in_port;
	(void) device;
}

unsigned retro_api_version()
{
	return RETRO_API_VERSION;
}

static void retro_audio_buff_status_cb(bool active, unsigned occupancy, bool underrun_likely)
{
	retro_audio_buff_active    = active;
	retro_audio_buff_occupancy = (uint8_t) occupancy;
	retro_audio_buff_underrun  = underrun_likely;
}

static void retro_set_audio_buff_status_cb()
{
	if (frameskip_type > 0)
	{
		struct retro_audio_buffer_status_callback buf_status_cb;
		buf_status_cb.callback = retro_audio_buff_status_cb;

		if (!environ_cb(RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK, &buf_status_cb))
		{
			if (log_cb)
				log_cb(RETRO_LOG_WARN, "Frameskip disabled - frontend does not support audio buffer status monitoring.\n");

			retro_audio_buff_active    = false;
			retro_audio_buff_occupancy = 0;
			retro_audio_buff_underrun  = false;
			retro_audio_latency        = 0;
		}
		else
		{
			/* Frameskip is enabled - increase frontend
			 * audio latency to minimise potential
			 * buffer underruns */
			uint32_t frame_time_usec = FRAME_TIME;
			/* Set latency to 6x current frame time... */
			retro_audio_latency = (uint16_t) (6 * frame_time_usec / 1000);
			/* ...then round up to nearest multiple of 32 */
			retro_audio_latency = (retro_audio_latency + 0x1F) & ~0x1F;
		}
	}
	else
	{
		environ_cb(RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK, NULL);
		retro_audio_latency = 0;
	}

	update_audio_latency = true;
}

const char* GetBIOSDir()
{
	const char *directory = NULL;
	environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &directory);
	return directory;
}

void DeinitDisplay()
{
	if (GFX.Screen_buffer)
		free(GFX.Screen_buffer);

	if (GFX.SubScreen_buffer)
		free(GFX.SubScreen_buffer);

	if (GFX.ZBuffer_buffer)
		free(GFX.ZBuffer_buffer);

	if (GFX.SubZBuffer_buffer)
		free(GFX.SubZBuffer_buffer);

	GFX.Screen = NULL;
	GFX.Screen_buffer = NULL;
	GFX.SubScreen = NULL;
	GFX.SubScreen_buffer = NULL;
	GFX.ZBuffer = NULL;
	GFX.ZBuffer_buffer = NULL;
	GFX.SubZBuffer = NULL;
	GFX.SubZBuffer_buffer = NULL;
}

void InitDisplay()
{
	int32_t h = IMAGE_HEIGHT;
	int32_t safety = 32;
	GFX.Pitch = IMAGE_WIDTH * 2;
	GFX.Screen_buffer = (uint8_t*) malloc(GFX.Pitch * h + safety);
	GFX.SubScreen_buffer = (uint8_t*) malloc(GFX.Pitch * h + safety);
	GFX.ZBuffer_buffer = (uint8_t*) malloc((GFX.Pitch >> 1) * h + safety);
	GFX.SubZBuffer_buffer = (uint8_t*) malloc((GFX.Pitch >> 1) * h + safety);
	GFX.Screen = GFX.Screen_buffer + safety;
	GFX.SubScreen = GFX.SubScreen_buffer + safety;
	GFX.ZBuffer = GFX.ZBuffer_buffer + safety;
	GFX.SubZBuffer = GFX.SubZBuffer_buffer + safety;
	GFX.Delta = (GFX.SubScreen - GFX.Screen) >> 1;
}

static void init_sfc_setting()
{
	memset(&Settings, 0, sizeof(Settings));
	Settings.ControllerOption = SNES_JOYPAD;
	Settings.H_Max       = SNES_CYCLES_PER_SCANLINE;
	Settings.HBlankStart = (256 * Settings.H_Max) / SNES_MAX_HCOUNTER;
}

static void audio_out_buffer_init()
{
	float refresh_rate        = (float) VIDEO_REFRESH_RATE;
	float samples_per_frame   = (float) SNES_SAMPLE_RATE / refresh_rate;
	size_t buffer_size        = ((size_t) samples_per_frame + 1) << 1;
	audio_out_buffer          = (int16_t*) malloc(buffer_size * sizeof(int16_t));
	audio_samples_per_frame   = samples_per_frame;
	audio_samples_accumulator = 0.0f;
}

static void audio_out_buffer_deinit()
{
	if (audio_out_buffer)
		free(audio_out_buffer);

	audio_out_buffer = NULL;
	audio_samples_per_frame   = 0.0f;
	audio_samples_accumulator = 0.0f;
}

static void audio_upload_samples()
{
	if (!Settings.APUEnabled)
		return;

	size_t available_frames    = (size_t) audio_samples_per_frame;
	audio_samples_accumulator += audio_samples_per_frame - (float) available_frames;

	if (audio_samples_accumulator > 1.0f)
	{
		available_frames          += 1;
		audio_samples_accumulator -= 1.0f;
	}

	MixSamples(audio_out_buffer, available_frames);

	if (!mute_audio)
		audio_batch_cb(audio_out_buffer, available_frames);
}

void retro_init()
{
	struct retro_log_callback log;
	enum retro_pixel_format rgb565;
	bool achievements = true;

	if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
		log_cb = log.log;
	else
		log_cb = NULL;

	/* State that the core supports achievements. */
	environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS, &achievements);
	rgb565 = RETRO_PIXEL_FORMAT_RGB565;

	if (environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &rgb565) && log_cb)
		log_cb(RETRO_LOG_INFO, "Frontend supports RGB565 - will use that instead of XRGB1555.\n");

	init_sfc_setting();
	InitMemory();
	InitAPU();
	InitDisplay();
	InitGFX();
	ResetAPU();

	if (environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL))
		libretro_supports_bitmasks = true;
}

void retro_deinit()
{
	if ((Settings.Chip & SPC7110) == SPC7110)
		DeinitSPC7110();

	DeinitGFX();
	DeinitDisplay();
	DeinitAPU();
	DeinitMemory();
	audio_out_buffer_deinit();

	/* Reset globals (required for static builds) */
	libretro_supports_option_categories = false;
	libretro_supports_bitmasks          = false;
	frameskip_type                      = 0;
	frameskip_threshold                 = 0;
	retro_audio_buff_active             = false;
	retro_audio_buff_occupancy          = 0;
	retro_audio_buff_underrun           = false;
	retro_audio_latency                 = 0;
	update_audio_latency                = false;
	mute_audio                          = false;
}

uint32_t ReadJoypad(int32_t port)
{
	static const uint32_t snes_lut[] =
	{
		SNES_B_MASK,
		SNES_Y_MASK,
		SNES_SELECT_MASK,
		SNES_START_MASK,
		SNES_UP_MASK,
		SNES_DOWN_MASK,
		SNES_LEFT_MASK,
		SNES_RIGHT_MASK,
		SNES_A_MASK,
		SNES_X_MASK,
		SNES_TL_MASK,
		SNES_TR_MASK
	};

	int32_t i;
	uint32_t joypad = 0;
	uint32_t joy_bits = 0;

	if (libretro_supports_bitmasks)
		joy_bits = input_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
	else
	{
		for (i = 0; i < (RETRO_DEVICE_ID_JOYPAD_R3+1); i++)
			joy_bits |= input_cb(port, RETRO_DEVICE_JOYPAD, 0, i) ? (1 << i) : 0;
	}

	for (i = RETRO_DEVICE_ID_JOYPAD_B; i <= RETRO_DEVICE_ID_JOYPAD_R; i++)
		if (joy_bits & (1 << i))
			joypad |= snes_lut[i];

	return joypad;
}

static void check_variables(bool first_run)
{
	struct retro_variable var;
	bool prev_frameskip_type;
	char* endptr;
	double freq = 10.0;

	var.key = "chimerasnes_frameskip";
	var.value = NULL;
	prev_frameskip_type = frameskip_type;
	frameskip_type      = 0;

	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
	{
		if (strcmp(var.value, "auto") == 0)
			frameskip_type = 1;
		else if (strcmp(var.value, "manual") == 0)
			frameskip_type = 2;
	}

	var.key = "chimerasnes_frameskip_threshold";
	var.value = NULL;
	frameskip_threshold = 33;

	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		frameskip_threshold = strtol(var.value, NULL, 10);

	var.key = "chimerasnes_overclock_cycles";
	var.value = NULL;
	Settings.OverclockCycles = false;

	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
	{
		if (strcmp(var.value, "compatible") == 0)
		{
			Settings.OverclockCycles = true;
			Settings.OneCycle        = DEFAULT_ONE_CYCLE - 2;
			Settings.SlowOneCycle    = DEFAULT_ONE_CYCLE - 1;
			Settings.TwoCycles       = DEFAULT_ONE_CYCLE;
		}
		else if (strcmp(var.value, "max") == 0)
		{
			Settings.OverclockCycles = true;
			Settings.OneCycle        = DEFAULT_ONE_CYCLE / 2;
			Settings.SlowOneCycle    = DEFAULT_ONE_CYCLE / 2;
			Settings.TwoCycles       = DEFAULT_ONE_CYCLE / 2;
		}
		else if (strcmp(var.value, "light") == 0)
		{
			Settings.OverclockCycles = true;
			Settings.OneCycle        = DEFAULT_ONE_CYCLE;
			Settings.SlowOneCycle    = DEFAULT_ONE_CYCLE;
			Settings.TwoCycles       = DEFAULT_TWO_CYCLES;
		}
	}

	if (!Settings.OverclockCycles)
	{
		Settings.OneCycle     = DEFAULT_ONE_CYCLE;
		Settings.SlowOneCycle = DEFAULT_SLOW_ONE_CYCLE;
		Settings.TwoCycles    = DEFAULT_TWO_CYCLES;
	}

	var.key = "chimerasnes_overclock_superfx";
	var.value = NULL;

	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
	{
		freq = strtod(var.value, &endptr);

		/* There must be a space between the value and the unit. Therefore, we
		 * check that the character after the converter integer is a space. */
		if (*endptr != ' ' || freq == 0.0)
			freq = 10.0;
	}

	/* Convert MHz value to Hz and multiply by required factors. */
	Settings.SuperFXSpeedPerLine = (uint32_t) ((582340.5 * freq) * ((1.0f / FRAMES_PER_SECOND) / ((float) SNES_MAX_VCOUNTER)));

	var.key = "chimerasnes_reduce_sprite_flicker";
	var.value = NULL;
	Settings.ReduceSpriteFlicker = false;

	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		if (strcmp(var.value, "enabled") == 0)
			Settings.ReduceSpriteFlicker = true;

	/* Reinitialise frameskipping, if required */
	if (!first_run && (frameskip_type != prev_frameskip_type))
		retro_set_audio_buff_status_cb();
}

void retro_run()
{
	bool updated = false;
	int result;
	bool okay;

	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
		check_variables(false);

#ifdef NO_VIDEO_OUTPUT
	video_cb(NULL, IPPU.RenderedScreenWidth, IPPU.RenderedScreenHeight, GFX.Pitch);
	IPPU.RenderThisFrame = false;
#endif

	result = -1;
	okay = environ_cb(RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE, &result);

	if (okay)
	{
		bool videoEnabled     = (bool) (result & 1);
		bool audioEnabled     = (bool) (result & 2);
		bool hardDisableAudio = (bool) (result & 8);
		IPPU.RenderThisFrame  = videoEnabled;
		mute_audio            = !audioEnabled;
		Settings.APUEnabled   = !hardDisableAudio;
	}
	else
	{
		IPPU.RenderThisFrame = true;
		mute_audio           = false;
		Settings.APUEnabled  = true;
	}

	/* Check whether current frame should be skipped */
	if ((frameskip_type > 0) && retro_audio_buff_active && IPPU.RenderThisFrame)
	{
		bool skip_frame;

		switch (frameskip_type)
		{
			case 1: /* auto */
				skip_frame = retro_audio_buff_underrun;
				break;
			case 2: /* manual */
				skip_frame = (retro_audio_buff_occupancy < frameskip_threshold);
				break;
			default:
				skip_frame = false;
				break;
		}

		if (skip_frame)
			IPPU.RenderThisFrame = false;
	}

	/* If frameskip/timing settings have changed,
	 * update frontend audio latency
	 * > Can do this before or after the frameskip
	 *   check, but doing it after means we at least
	 *   retain the current frame's audio output */
	if (update_audio_latency)
	{
		environ_cb(RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY, &retro_audio_latency);
		update_audio_latency = false;
	}

	poll_cb();
	MainLoop();

#ifdef NO_VIDEO_OUTPUT
	audio_upload_samples();
	return;
#endif

	if (IPPU.RenderThisFrame)
	{
	#ifdef PSP
		static uint32_t __attribute__((aligned(16))) d_list[32];
		void* const texture_vram_p = (void*) (0x44200000 - (512 * 512)); /* max VRAM address - frame size */
		sceKernelDcacheWritebackRange(GFX.Screen, GFX.Pitch * IPPU.RenderedScreenHeight);
		sceGuStart(GU_DIRECT, d_list);
		sceGuCopyImage(GU_PSM_4444, 0, 0, IPPU.RenderedScreenWidth, IPPU.RenderedScreenHeight, GFX.Pitch >> 1, GFX.Screen, 0, 0, 512, texture_vram_p);
		sceGuTexSync();
		sceGuTexImage(0, 512, 512, 512, texture_vram_p);
		sceGuTexMode(GU_PSM_5551, 0, 0, GU_FALSE);
		sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
		sceGuDisable(GU_BLEND);
		sceGuFinish();
		video_cb(texture_vram_p, IPPU.RenderedScreenWidth, IPPU.RenderedScreenHeight, GFX.Pitch);
	#else
		video_cb(GFX.Screen, IPPU.RenderedScreenWidth, IPPU.RenderedScreenHeight, GFX.Pitch);
	#endif
	}
	else
		video_cb(NULL, IPPU.RenderedScreenWidth, IPPU.RenderedScreenHeight, GFX.Pitch);

	audio_upload_samples();
}

bool ReadMousePosition(int32_t which1, int32_t* x, int32_t* y, uint32_t* buttons)
{
	(void) which1;
	(void) x;
	(void) y;
	(void) buttons;
	return false;
}

bool ReadSuperScopePosition(int32_t* x, int32_t* y, uint32_t* buttons)
{
	(void) x;
	(void) y;
	(void) buttons;
	return true;
}

bool JustifierOffscreen()
{
	return false;
}

void JustifierButtons(uint32_t* justifiers)
{
	(void) justifiers;
}

unsigned retro_get_region()
{
	return Settings.PAL ? RETRO_REGION_PAL : RETRO_REGION_NTSC;
}

void retro_get_system_info(struct retro_system_info* info)
{
	info->need_fullpath = false;
	info->valid_extensions = "smc|fig|sfc|gd3|gd7|dx2|bsx|bs|swc|st";
#ifndef GIT_VERSION
	#define GIT_VERSION ""
#endif
	info->library_version = GIT_VERSION;
	info->library_name = "ChimeraSNES";
	info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info* info)
{
	info->geometry.base_width   = SNES_WIDTH;
	info->geometry.base_height  = SNES_HEIGHT;
	info->geometry.max_width    = MAX_SNES_WIDTH;
	info->geometry.max_height   = MAX_SNES_HEIGHT;
	info->geometry.aspect_ratio = 4.0 / 3.0;
	info->timing.sample_rate    = SNES_SAMPLE_RATE;
	info->timing.fps            = VIDEO_REFRESH_RATE;
}

void retro_reset()
{
	SoftReset();
}

size_t retro_serialize_size()
{
	return sizeof(CPU) + sizeof(ICPU) + sizeof(PPU) + sizeof(DMA) +
		0x10000 + 0x20000 + 0x20000 + 0x8000 +
		sizeof(APU) + sizeof(IAPU) + 0x10000 +
		sizeof(SA1) + sizeof(s7r) + sizeof(rtc_f9);
}

bool retro_serialize(void* data, size_t size)
{
	uint8_t* buffer = data;
	(void) size;
	PackStatus();
	StoreAPUDSP();
	APUPackStatus();
	UpdateRTC();
	memcpy(buffer, &CPU, sizeof(CPU));
	buffer += sizeof(CPU);
	memcpy(buffer, &ICPU, sizeof(ICPU));
	buffer += sizeof(ICPU);
	memcpy(buffer, &PPU, sizeof(PPU));
	buffer += sizeof(PPU);
	memcpy(buffer, &DMA, sizeof(DMA));
	buffer += sizeof(DMA);
	memcpy(buffer, Memory.VRAM, 0x10000);
	buffer += 0x10000;
	memcpy(buffer, Memory.RAM, 0x20000);
	buffer += 0x20000;
	memcpy(buffer, Memory.SRAM, 0x20000);
	buffer += 0x20000;
	memcpy(buffer, Memory.FillRAM, 0x8000);
	buffer += 0x8000;
	memcpy(buffer, &APU, sizeof(APU));
	buffer += sizeof(APU);
	memcpy(buffer, &IAPU, sizeof(IAPU));
	buffer += sizeof(IAPU);
	memcpy(buffer, IAPU.RAM, 0x10000);
	buffer += 0x10000;
	SA1PackStatus();
	memcpy(buffer, &SA1, sizeof(SA1));
	buffer += sizeof(SA1);
	memcpy(buffer, &s7r, sizeof(s7r));
	buffer += sizeof(s7r);
	memcpy(buffer, &rtc_f9, sizeof(rtc_f9));
	return true;
}

bool retro_unserialize(const void* data, size_t size)
{
	const uint8_t* buffer = data;
	uint8_t* IAPU_RAM_current = IAPU.RAM;
	uintptr_t IAPU_RAM_offset;

	if (size != retro_serialize_size())
		return false;

	Reset();
	memcpy(&CPU, buffer, sizeof(CPU));
	buffer += sizeof(CPU);
	memcpy(&ICPU, buffer, sizeof(ICPU));
	buffer += sizeof(ICPU);
	memcpy(&PPU, buffer, sizeof(PPU));
	buffer += sizeof(PPU);
	memcpy(&DMA, buffer, sizeof(DMA));
	buffer += sizeof(DMA);
	memcpy(Memory.VRAM, buffer, 0x10000);
	buffer += 0x10000;
	memcpy(Memory.RAM, buffer, 0x20000);
	buffer += 0x20000;
	memcpy(Memory.SRAM, buffer, 0x20000);
	buffer += 0x20000;
	memcpy(Memory.FillRAM, buffer, 0x8000);
	buffer += 0x8000;
	memcpy(&APU, buffer, sizeof(APU));
	buffer += sizeof(APU);
	memcpy(&IAPU, buffer, sizeof(IAPU));
	buffer += sizeof(IAPU);
	IAPU_RAM_offset = IAPU_RAM_current - IAPU.RAM;
	IAPU.PC += IAPU_RAM_offset;
	IAPU.DirectPage += IAPU_RAM_offset;
	IAPU.WaitAddress1 += IAPU_RAM_offset;
	IAPU.WaitAddress2 += IAPU_RAM_offset;
	IAPU.RAM = IAPU_RAM_current;
	memcpy(IAPU.RAM, buffer, 0x10000);
	buffer += 0x10000;
	memcpy(&SA1, buffer, sizeof(SA1));
	buffer += sizeof(SA1);
	memcpy(&s7r, buffer, sizeof(s7r));
	buffer += sizeof(s7r);
	memcpy(&rtc_f9, buffer, sizeof(rtc_f9));
	FixROMSpeed(Settings.SlowOneCycle);
	IPPU.ColorsChanged = true;
	IPPU.OBJChanged = true;
	CPU.InDMA = false;
	FixColourBrightness();
	SRTCPostLoadState();
	SA1UnpackStatus();
	SetPlaybackRate(SNES_SAMPLE_RATE);
	APUUnpackStatus();
	RestoreAPUDSP();
	ICPU.ShiftedPB = ICPU.Registers.PB << 16;
	ICPU.ShiftedDB = ICPU.Registers.DB << 16;
	SetPCBase(ICPU.Registers.PBPC);
	UnpackStatus();
	FixCycles();
	Reschedule();
	return true;
}

void retro_cheat_reset()
{
	RemoveCheats();
}

void retro_cheat_set(unsigned index, bool enabled, const char* code)
{
	(void) index;

	if (!enabled)
		return;

	AddCheat(code);
}

static void init_descriptors()
{
	#define describe_buttons(INDEX)                                                    \
		{INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,   "D-Pad Left"},  \
		{INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,     "D-Pad Up"},    \
		{INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,   "D-Pad Down"},  \
		{INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT,  "D-Pad Right"}, \
		{INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,      "B"},           \
		{INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,      "A"},           \
		{INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,      "X"},           \
		{INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,      "Y"},           \
		{INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,      "L"},           \
		{INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,      "R"},           \
		{INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select"},      \
		{INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Start"}

	struct retro_input_descriptor desc[] =
	{
		describe_buttons(0),
		describe_buttons(1),
		describe_buttons(2),
		describe_buttons(3),
		describe_buttons(4),
		{0, 0, 0, 0, NULL}
	};

	environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);
}

bool retro_load_game(const struct retro_game_info* game)
{
	char info_buf[256];

	if (!game)
		return false;

	init_descriptors();
	check_variables(true);

	if (!LoadROM(game, info_buf))
		return false;

	if (environ_cb)
	{
		struct retro_message msg;
		msg.frames = FRAMES_PER_SECOND * 2;
		msg.msg = info_buf;
		environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, (void*) &msg);
	}
	else if (log_cb)
		log_cb(RETRO_LOG_INFO, info_buf);

	retro_set_audio_buff_status_cb();
	audio_out_buffer_init();
	SetPlaybackRate(SNES_SAMPLE_RATE);
	return true;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info* info, size_t num_info)
{
	(void) game_type;
	(void) info;
	(void) num_info;
	return false;
}

void retro_unload_game()
{
}

void* retro_get_memory_data(unsigned type)
{
	uint8_t* data;

	switch (type)
	{
		case RETRO_MEMORY_SAVE_RAM:
			data = Memory.SRAM;
			break;
		case RETRO_MEMORY_RTC:
			data = RTCData.reg;
			break;
		case RETRO_MEMORY_SYSTEM_RAM:
			data = Memory.RAM;
			break;
		case RETRO_MEMORY_VIDEO_RAM:
			data = Memory.VRAM;
			break;
		default:
			data = NULL;
			break;
	}

	return data;
}

size_t retro_get_memory_size(unsigned type)
{
	uint32_t size;

	switch (type)
	{
		case RETRO_MEMORY_SAVE_RAM:
			size = (uint32_t)(Memory.SRAMSize ? (1 << (Memory.SRAMSize + 3)) * 128 : 0);

			if (size > 0x20000)
				size = 0x20000;

			break;
		case RETRO_MEMORY_RTC:
			size = ((Settings.Chip & S_RTC) == S_RTC) ? 20 : 0;
			break;
		case RETRO_MEMORY_SYSTEM_RAM:
			size = 128 * 1024;
			break;
		case RETRO_MEMORY_VIDEO_RAM:
			size = 64 * 1024;
			break;
		default:
			size = 0;
			break;
	}

	return size;
}
