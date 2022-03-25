#include "apu.h"

APU::APU() {

}

void APU::clear(bool mute) {
    memset(registers, 0, 0x15);

    if (!mute) {
        std::cout << "APU was cleared\n";
    }
}

uint8_t APU::readRegister(uint16_t addr) {
    if (addr == 0x4015) {
        --addr;
    }
    addr &= 0x1f;
    return registers[addr];
}

void APU::writeRegister(uint16_t addr, uint8_t val, bool mute) {
    if (addr == 0x4015) {
        --addr;
    }
    addr &= 0x1f;
    registers[addr] = val;

    if (!mute) {
        std::cout << std::hex << "0x" << (unsigned int) val << " has " <<
            "been written to the address 0x" << (unsigned int) addr <<
            "\n--------------------------------------------------\n" <<
            std::dec;
    }
}