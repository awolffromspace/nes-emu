#include "ram.h"

// Public Member Functions

RAM::RAM() { }

void RAM::clear() {
    memset(data, 0, RAM_SIZE);
}

// Handles reads from the CPU

uint8_t RAM::read(uint16_t addr) const {
    addr = getLocalAddr(addr);
    return data[addr];
}

// Handles writes from the CPU

void RAM::write(uint16_t addr, uint8_t val) {
    addr = getLocalAddr(addr);
    data[addr] = val;
}

// Handles pushes to the stack from the CPU

void RAM::push(uint8_t& pointer, uint8_t val, bool mute) {
    // The stack is located after the zero page, so the zero page size is effectively the bottom of
    // the stack
    uint16_t addr = ZERO_PAGE_SIZE + pointer;
    data[addr] = val;
    --pointer;

    if (!mute) {
        std::cout << std::hex << "0x" << (unsigned int) val << " has been pushed to the stack "
            "address 0x" << (unsigned int) addr <<
            "\n--------------------------------------------------\n" << std::dec;
    }
}

// Handles pulls from the stack from the CPU

uint8_t RAM::pull(uint8_t& pointer, bool mute) {
    ++pointer;
    // The stack is located after the zero page, so the zero page size is effectively the bottom of
    // the stack
    uint16_t addr = ZERO_PAGE_SIZE + pointer;
    uint8_t val = data[addr];

    if (!mute) {
        std::cout << std::hex << "0x" << (unsigned int) val << " has been pulled from the stack "
            "address 0x" << (unsigned int) addr <<
            "\n--------------------------------------------------\n" << std::dec;
    }

    return val;
}

// Private Member Functions

// Maps the CPU address to the RAM's local field, data

uint16_t RAM::getLocalAddr(uint16_t addr) const {
    // RAM is located from $0000 - $2000 in the CPU memory map. However, $0800 - $2000 is mirrored.
    // This operation clears out the upper bits that set the address to a value higher than $07ff,
    // so the result is a mirrored address
    return addr & 0x7ff;
}