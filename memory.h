#ifndef MEMORY_H
#define MEMORY_H

#include <cstdint>

class Memory {
	public:
		Memory();
		Memory(uint8_t d[]);
		uint8_t read(uint16_t addr);
		void write(uint16_t addr, uint8_t val);

	private:
		uint8_t data[65536];
};

#endif