#include "apu.h"

// Public Member Functions

APU::APU() {
    memset(registers, 0, 0x16);
}

void APU::clear() {
    memset(registers, 0, 0x16);
}

// Handles register reads from the CPU

uint8_t APU::readRegister(const uint16_t addr) const {
    const uint16_t localAddr = getLocalAddr(addr);
    return registers[localAddr];
}

// Handles register writes from the CPU

void APU::writeRegister(const uint16_t addr, const uint8_t val) {
    const uint16_t localAddr = getLocalAddr(addr);
    registers[localAddr] = val;
}

// Private Member Functions

// Maps the CPU address to the APU's local field, registers

uint16_t APU::getLocalAddr(const uint16_t addr) const {
    uint16_t localAddr = addr;
    // The APU's first set of registers are contiguous from $4000 - $4013 in the CPU memory map.
    // However, $4014 is one of the PPU's registers (OAMDMA), and $4016 is one of the I/O (joystick)
    // registers. The APU has two more registers in-between them ($4015 and $4017). $4015 and $4017
    // are decremented to keep the register array contiguous
    if (localAddr == 0x4015) {
        --localAddr;
    } else if (localAddr == 0x4017) {
        localAddr -= 2;
    }
    // Clear out the upper bits so that $4000 becomes 0, $4001 becomes 1, etc.
    return localAddr & 0x1f;
}