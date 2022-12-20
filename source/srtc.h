#ifndef CHIMERASNES_SRTC_H_
#define CHIMERASNES_SRTC_H_

typedef struct
{
	uint8_t reg[20];
} SRTCData;

typedef struct
{
	int32_t  rtc_index;
	uint32_t rtc_mode;
} SSRTCSnap;

extern SRTCData  RTCData;
extern SSRTCSnap srtcsnap;

void    InitSRTC();
void    ResetSRTC();
void    SetSRTC(uint8_t data, uint16_t address);
uint8_t GetSRTC(uint16_t address);
void    SRTCPostLoadState();
#endif
