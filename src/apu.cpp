#include "apu.h"

// Public Member Functions

APU::APU() { }

void APU::clear() {
    memset(registers, 0, APU_REGISTER_SIZE);
}

// Handles I/O reads from the CPU

uint8_t APU::readIO(uint16_t addr) const {
    addr = getLocalAddr(addr);
    return registers[addr];
}

// Handles I/O writes from the CPU

void APU::writeIO(uint16_t addr, uint8_t val) {
    addr = getLocalAddr(addr);
    registers[addr] = val;
}

// Private Member Functions

// Maps the CPU address to the APU's local field, registers

uint16_t APU::getLocalAddr(uint16_t addr) const {
    // The APU's first set of registers are contiguous from $4000 - $4013 in the CPU memory map.
    // However, $4014 is one of the PPU's registers (OAMDMA), and $4016 is one of the I/O (joystick)
    // registers. The APU has two more registers in-between them ($4015 and $4017). $4015 and $4017
    // are decremented to keep the register array contiguous
    if (addr == 0x4015) {
        --addr;
    } else if (addr == 0x4017) {
        addr -= 2;
    }
    // Clear out the upper bits so that $4000 becomes 0, $4001 becomes 1, etc.
    return addr & 0x1f;
}