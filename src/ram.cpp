#include "ram.h"

RAM::RAM() {

}

void RAM::clear(bool mute) {
    memset(data, 0, 0x800);

    if (!mute) {
        std::cout << "RAM was cleared\n";
    }
}

uint8_t RAM::read(uint16_t addr) {
    addr &= 0x7ff;
    return data[addr];
}

void RAM::write(uint16_t addr, uint8_t val, bool mute) {
    addr &= 0x7ff;
    data[addr] = val;

    if (!mute) {
        std::cout << std::hex << "0x" << (unsigned int) val << " has " <<
            "been written to the address 0x" << (unsigned int) addr <<
            "\n--------------------------------------------------\n" <<
            std::dec;
    }
}

void RAM::push(uint8_t& pointer, uint8_t val, bool mute) {
    uint16_t addr = 0x100 + pointer;
    data[addr] = val;

    if (!mute) {
        std::cout << std::hex << "0x" << (unsigned int) val << " has " <<
            "been pushed to the stack address 0x" << (unsigned int) addr <<
            "\n--------------------------------------------------\n" <<
            std::dec;
    }

    --pointer;
}

uint8_t RAM::pull(uint8_t& pointer, bool mute) {
    ++pointer;
    uint16_t addr = 0x100 + pointer;
    uint8_t val = data[addr];

    if (!mute) {
        std::cout << std::hex << "0x" << (unsigned int) val << " has " <<
            "been pulled from the stack address 0x" << (unsigned int) addr <<
            "\n--------------------------------------------------\n" <<
            std::dec;
    }

    return val;
}