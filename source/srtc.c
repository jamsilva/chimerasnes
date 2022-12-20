#include <time.h>

#include "chisnes.h"
#include "cpuexec.h"
#include "display.h"
#include "math.h"
#include "memmap.h"
#include "srtc.h"
#include "spc7110dec.h"

enum
{
	RTCM_READY   = 0,
	RTCM_COMMAND = 1,
	RTCM_READ    = 2,
	RTCM_WRITE   = 3
};

SSRTCSnap srtcsnap;
static const uint32_t months[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static void srtc_update_time()
{
	time_t rtc_time = RTCData.reg[16] | (RTCData.reg[17] << 8) | (RTCData.reg[18] << 16) | (RTCData.reg[19] << 24);
	time_t current_time = time(0);

	/* sizeof(time_t) is platform-dependent; though memory::cartrtc needs to be platform-agnostic.
	 * yet platforms with 32-bit signed time_t will overflow every ~68 years. handle this by
	 * accounting for overflow at the cost of 1-bit precision (to catch underflow). this will allow
	 * memory::cartrtc timestamp to remain valid for up to ~34 years from the last update, even if
	 * time_t overflows. calculation should be valid regardless of number representation, time_t size,
	 * or whether time_t is signed or unsigned. */
	time_t max_time = (time_t) -1;
	time_t diff = (current_time >= rtc_time) ? (current_time - rtc_time) : (max_time - rtc_time + current_time + 1); /* compensate for overflow */

	if (diff > max_time / 2)
		diff = 0; /* compensate for underflow */

	if (diff > 0)
	{
		uint32_t days;
		uint32_t second  = RTCData.reg[0] + RTCData.reg[1] * 10 + diff;
		uint32_t minute  = RTCData.reg[2] + RTCData.reg[3] * 10;
		uint32_t hour    = RTCData.reg[4] + RTCData.reg[5] * 10;
		uint32_t day     = RTCData.reg[6] + RTCData.reg[7] * 10 - 1;
		uint32_t month   = RTCData.reg[8] - 1;
		uint32_t year    = RTCData.reg[9] + RTCData.reg[10] * 10 + RTCData.reg[11] * 100 + 1000;
		uint32_t weekday = RTCData.reg[12];

		while (second >= 60)
		{
			second -= 60;
			minute++;

			if (minute < 60)
				continue;

			minute = 0;
			hour++;

			if (hour < 24)
				continue;

			hour = 0;
			day++;
			weekday = (weekday + 1) % 7;
			days = months[month % 12];

			if (days == 28)
				days += !(year % 400) - !(year % 100) + !(year % 4);

			if (day < days)
				continue;

			day = 0;
			month++;

			if (month < 12)
				continue;

			month = 0;
			year++;
		}

		day++;
		month++;
		year -= 1000;
		RTCData.reg[0] = second % 10;
		RTCData.reg[1] = second / 10;
		RTCData.reg[2] = minute % 10;
		RTCData.reg[3] = minute / 10;
		RTCData.reg[4] = hour % 10;
		RTCData.reg[5] = hour / 10;
		RTCData.reg[6] = day % 10;
		RTCData.reg[7] = day / 10;
		RTCData.reg[8] = month;
		RTCData.reg[9] = year % 10;
		RTCData.reg[10] = (year / 10) % 10;
		RTCData.reg[11] = year / 100;
		RTCData.reg[12] = weekday % 7;
	}

	RTCData.reg[16] = current_time;
	RTCData.reg[17] = current_time >> 8;
	RTCData.reg[18] = current_time >> 16;
	RTCData.reg[19] = current_time >> 24;
}

/* returns day of week for specified date - eg 0 = Sunday, 1 = Monday, ... 6 = Saturday */
/* usage: weekday(2008, 1, 1) returns weekday of January 1st, 2008 */
static uint32_t srtc_weekday(uint32_t year, uint32_t month, uint32_t day)
{
	uint32_t y   = 1900;
	uint32_t m   = 1;    /* epoch is 1900-01-01 */
	uint32_t sum = 0; /* number of days passed since epoch */
	year  = MATH_MAX(1900, year);
	month = MATH_MAX(1,    MATH_MIN(12, month));
	day   = MATH_MAX(1,    MATH_MIN(31, day));

	while (y < year)
	{
		sum += 365 + !(y % 400) - !(y % 100) + !(y % 4);
		y++;
	}

	while (m < month)
	{
		uint32_t days = months[m - 1];

		if (days == 28)
			days += !(y % 400) - !(y % 100) + !(y % 4);

		sum += days;
		m++;
	}

	sum += day;
	return sum % 7; /* 1900-01-01 was a Monday */
}

void InitSRTC()
{
	spc7110dec_init();
	ResetSRTC();
	memset(RTCData.reg, 0, 20);
}

void ResetSRTC()
{
	srtcsnap.rtc_mode  = RTCM_READ;
	srtcsnap.rtc_index = -1;
	srtc_update_time();
}

void SetSRTC(uint8_t data, uint16_t address)
{
	address &= 0xffff;

	if (address != 0x2801)
		return;

	data &= 0x0f; /* only the low four bits are used */

	if (data == 0x0d)
	{
		srtcsnap.rtc_mode = RTCM_READ;
		srtcsnap.rtc_index = -1;
		return;
	}

	if (data == 0x0e)
	{
		srtcsnap.rtc_mode = RTCM_COMMAND;
		return;
	}

	if (data == 0x0f)
		return; /* unknown behavior */

	if (srtcsnap.rtc_mode == RTCM_WRITE)
	{
		if (srtcsnap.rtc_index >= 0 && srtcsnap.rtc_index < 12)
		{
			RTCData.reg[srtcsnap.rtc_index++] = data;

			if (srtcsnap.rtc_index == 12) /* day of week is automatically calculated and written */
			{
				uint32_t day = RTCData.reg[6] + RTCData.reg[7] * 10;
				uint32_t month = RTCData.reg[8];
				uint32_t year = RTCData.reg[9] + RTCData.reg[10] * 10 + RTCData.reg[11] * 100 + 1000;
				RTCData.reg[srtcsnap.rtc_index++] = srtc_weekday(year, month, day);
			}
		}
	}
	else if (srtcsnap.rtc_mode == RTCM_COMMAND)
	{
		if (data == 0)
		{
			srtcsnap.rtc_mode = RTCM_WRITE;
			srtcsnap.rtc_index = 0;
		}
		else if (data == 4)
		{
			uint32_t i;
			srtcsnap.rtc_mode = RTCM_READY;
			srtcsnap.rtc_index = -1;

			for (i = 0; i < 13; i++)
				RTCData.reg[i] = 0;
		}
		else /* unknown behavior */
			srtcsnap.rtc_mode = RTCM_READY;
	}
}

uint8_t GetSRTC(uint16_t address)
{
	address &= 0xffff;

	if (address != 0x2800)
		return ICPU.OpenBus;

	if (srtcsnap.rtc_mode != RTCM_READ)
		return 0x00;

	if (srtcsnap.rtc_index < 0)
	{
		srtc_update_time();
		srtcsnap.rtc_index++;
		return 0x0f;
	}
	else if (srtcsnap.rtc_index > 12)
	{
		srtcsnap.rtc_index = -1;
		return 0x0f;
	}

	return RTCData.reg[srtcsnap.rtc_index++];
}

void SRTCPostLoadState()
{
	srtc_update_time();
}
