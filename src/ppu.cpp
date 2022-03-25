#include "ppu.h"

PPU::PPU() {

}

void PPU::clear(bool mute) {
    memset(registers, 0, 0x8);

    if (!mute) {
        std::cout << "PPU was cleared\n";
    }
}

uint8_t PPU::readRegister(uint16_t addr) {
    if (addr == 0x4014) {
        return oamdma;
    }

    addr &= 0x7;
    return registers[addr];
}

void PPU::writeRegister(uint16_t addr, uint8_t val, bool mute) {
    if (addr == 0x4014) {
        oamdma = val;
    } else {
        addr &= 0x7;
        registers[addr] = val;
    }

    if (!mute) {
        std::cout << std::hex << "0x" << (unsigned int) val << " has " <<
            "been written to the address 0x" << (unsigned int) addr <<
            "\n--------------------------------------------------\n" <<
            std::dec;
    }
}