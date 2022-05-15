#include "io.h"

IO::IO() :
        strobe(false),
        currentButton(A),
        a(0),
        b(0),
        select(0),
        start(0),
        up(0),
        down(0),
        left(0),
        right(0) {

}

void IO::clear(bool mute) {
    memset(registers, 0, IO_REGISTER_SIZE);
    strobe = false;
    currentButton = A;
    a = 0;
    b = 0;
    select = 0;
    start = 0;
    up = 0;
    down = 0;
    left = 0;
    right = 0;

    if (!mute) {
        std::cout << "IO was cleared\n";
    }
}

uint8_t IO::readIO(uint16_t addr) {
    if (addr == 0x4016 && strobe) {
        registers[addr - 0x4016] &= 0xfe;
        registers[addr - 0x4016] |= a;
    } else if (addr == 0x4016 && !strobe) {
        registers[addr - 0x4016] &= 0xfe;
        switch (currentButton) {
            case A:
                registers[addr - 0x4016] |= a;
                break;
            case B:
                registers[addr - 0x4016] |= b;
                break;
            case Select:
                registers[addr - 0x4016] |= select;
                break;
            case Start:
                registers[addr - 0x4016] |= start;
                break;
            case Up:
                registers[addr - 0x4016] |= up;
                break;
            case Down:
                registers[addr - 0x4016] |= down;
                break;
            case Left:
                registers[addr - 0x4016] |= left;
                break;
            case Right:
                registers[addr - 0x4016] |= right;
        }
        ++currentButton;
        if (currentButton > Right) {
            currentButton = A;
        }
    }

    addr -= 0x4016;
    return registers[addr];
}

void IO::writeIO(uint16_t addr, uint8_t val, bool mute) {
    uint16_t localAddr = addr - 0x4016;
    registers[localAddr] = val;

    if (addr == 0x4016) {
        if (val & 1) {
            strobe = true;
            currentButton = A;
        } else {
            strobe = false;
        }
    }

    if (!mute) {
        std::cout << std::hex << "0x" << (unsigned int) val << " has " <<
            "been written to the address 0x" << (unsigned int) addr <<
            "\n--------------------------------------------------\n" <<
            std::dec;
    }
}