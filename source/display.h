#ifndef CHIMERASNES_DISPLAY_H_
#define CHIMERASNES_DISPLAY_H_

/* Routines the port specific code has to implement */
uint32_t ReadJoypad(int32_t port);
bool     ReadMousePosition(int32_t which1_0_to_1, int32_t* x, int32_t* y, uint32_t* buttons);
bool     ReadSuperScopePosition(int32_t* x, int32_t* y, uint32_t* buttons);
void     InitDisplay();
void     DeinitDisplay();
void     ToggleSoundChannel(int32_t channel);
void     NextController();
#endif
