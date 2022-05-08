#include "io.h"

IO::IO() {

}

void IO::clear(bool mute) {
    memset(registers, 0, IO_REGISTER_SIZE);

    if (!mute) {
        std::cout << "IO was cleared\n";
    }
}

uint8_t IO::readIO(uint16_t addr) const {
    addr -= 0x4016;
    return registers[addr];
}

void IO::writeIO(uint16_t addr, uint8_t val, bool mute) {
    uint16_t localAddr = addr - 0x4016;
    registers[localAddr] = val;

    if (!mute) {
        std::cout << std::hex << "0x" << (unsigned int) val << " has " <<
            "been written to the address 0x" << (unsigned int) addr <<
            "\n--------------------------------------------------\n" <<
            std::dec;
    }
}