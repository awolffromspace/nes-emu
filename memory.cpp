#include "memory.h"

Memory::Memory() {

}

Memory::Memory(uint8_t d[]) {
	for (int i = 0; i < 65536; ++i) {
		data[i] = d[i];
	}
}

uint8_t Memory::read(uint16_t addr) {
	return data[addr];
}

void Memory::write(uint16_t addr, uint8_t val) {
	data[addr] = val;
	if (addr < 0x2000) { // Zero Page mirroring
		for (int i = 1; i <= 3; ++i) {
			uint16_t index = addr + 0x800 * i;
			if (index >= 0x2000) {
				index -= 0x2000;
			}
			data[index] = val;
		}
	} else if (addr < 0x4000) { // I/O register mirroring
		for (int i = 1; i <= 400; ++i) {
			uint16_t index = addr + 0x8 * i;
			if (index >= 0x4000) {
				index -= 0x2000;
			}
			data[index] = val;
		}
	}
}