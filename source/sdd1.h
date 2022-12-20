#ifndef CHIMERASNES_SDD1_H_
#define CHIMERASNES_SDD1_H_

void SetSDD1MemoryMap(uint32_t bank, uint32_t value);
void ResetSDD1();
void SDD1_decompress(uint8_t* out, uint8_t* in, int32_t output_length);
#endif
