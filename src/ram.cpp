#include "ram.h"

// Public Member Functions

RAM::RAM() { }

void RAM::clear() {
    memset(data, 0, 0x800);
}

// Handles reads from the CPU

uint8_t RAM::read(const uint16_t addr) const {
    const uint16_t localAddr = getLocalAddr(addr);
    return data[localAddr];
}

// Handles writes from the CPU

void RAM::write(const uint16_t addr, const uint8_t val) {
    const uint16_t localAddr = getLocalAddr(addr);
    data[localAddr] = val;
}

// Handles pushes to the stack from the CPU

void RAM::push(uint8_t& pointer, const uint8_t val, bool mute) {
    const uint16_t zeroPageSize = 0x100;
    // The stack is located after the zero page, so the zero page size is effectively the bottom of
    // the stack
    const uint16_t addr = zeroPageSize + pointer;
    data[addr] = val;
    --pointer;

    if (!mute) {
        std::cout << std::hex << "0x" << (unsigned int) val << " has been pushed to the stack "
            "address 0x" << (unsigned int) addr <<
            "\n--------------------------------------------------\n" << std::dec;
    }
}

// Handles pulls from the stack from the CPU

uint8_t RAM::pull(uint8_t& pointer, const bool mute) {
    ++pointer;
    const uint16_t zeroPageSize = 0x100;
    // The stack is located after the zero page, so the zero page size is effectively the bottom of
    // the stack
    const uint16_t addr = zeroPageSize + pointer;
    const uint8_t val = data[addr];

    if (!mute) {
        std::cout << std::hex << "0x" << (unsigned int) val << " has been pulled from the stack "
            "address 0x" << (unsigned int) addr <<
            "\n--------------------------------------------------\n" << std::dec;
    }

    return val;
}

// Private Member Functions

// Maps the CPU address to the RAM's local field, data

uint16_t RAM::getLocalAddr(const uint16_t addr) const {
    // RAM is located from $0000 - $1fff in the CPU memory map. However, $0800 - $1fff is mirrored.
    // This operation clears out the upper bits that set the address to a value higher than $07ff,
    // so the result is a mirrored address
    return addr & 0x7ff;
}