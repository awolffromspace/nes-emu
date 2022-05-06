#include "apu.h"

APU::APU() {

}

void APU::clear(bool mute) {
    memset(registers, 0, 0x16);

    if (!mute) {
        std::cout << "APU was cleared\n";
    }
}

uint8_t APU::readIO(uint16_t addr) const {
    if (addr > 0x4014) {
        --addr;
    }
    addr &= 0x1f;
    return registers[addr];
}

void APU::writeIO(uint16_t addr, uint8_t val, bool mute) {
    uint16_t localAddr = addr;
    if (localAddr > 0x4014) {
        --localAddr;
    }
    localAddr &= 0x1f;
    registers[localAddr] = val;

    if (!mute) {
        std::cout << std::hex << "0x" << (unsigned int) val << " has " <<
            "been written to the address 0x" << (unsigned int) addr <<
            "\n--------------------------------------------------\n" <<
            std::dec;
    }
}