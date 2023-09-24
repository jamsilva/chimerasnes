#ifndef CHIMERASNES_SNESAPU_H_
#define CHIMERASNES_SNESAPU_H_

/* DSP options */
#define DSP_NOECHO 0x10 /* Disable echo (must not be bit 0 or 5, see disEcho) */

/* Mixing flags */
#define MFLG_MUTE 0x01 /* Mute voice */
#define MFLG_KOFF 0x02 /* Voice is in the process of keying off */
#define MFLG_OFF 0x04 /* Voice is currently inactive */
#define MFLG_END 0x08 /* End block was just played */
#define MFLG_SSRC 0x10 /* StartSrc */

/* Envelope precision */
#define E_SHIFT 8 /* Amount to shift envelope to get 8-bit signed value */

#define FIXED_POINT 0x10000
#define FIXED_POINT_REMAINDER 0xffff
#define FIXED_POINT_SHIFT 16

void InitAPUDSP();
void ResetAPUDSP();
void SetPlaybackRate(int32_t rate);
void StoreAPUDSP();
void RestoreAPUDSP();
void SetAPUDSPAmp(int32_t amp);
void MixSamples(int16_t* pBuf, int32_t num);
void APUDSPIn(uint8_t address, uint8_t data);
#endif
