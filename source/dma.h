#ifndef CHIMERASNES_DMA_H_
#define CHIMERASNES_DMA_H_

void    ResetDMA();
uint8_t DoHDMA(uint8_t byte);
void    StartHDMA();
void    DoDMA(uint8_t Channel);
#endif
