#ifndef CHIMERASNES_CPUOPS_H_
#define CHIMERASNES_CPUOPS_H_

void OpcodeNMI();
void OpcodeIRQ();

#define CHECK_FOR_IRQ()               \
	if (CPU.IRQActive && !CheckIRQ()) \
		OpcodeIRQ()
#endif
