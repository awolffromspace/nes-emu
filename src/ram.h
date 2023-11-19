#ifndef RAM_H
#define RAM_H

#include <cstdint>
#include <cstring>
#include <iostream>

#define RAM_SIZE 0x800
#define ZERO_PAGE_SIZE 0x100

// Random Access Memory
// The main workspace for programs. Stores data for addresses $0000 - $00ff (zero page), $0100 -
// $01ff (stack), $0200 - $07ff (additional RAM), and $0800 - $1fff (mirrored addresses) in the CPU
// memory map

class RAM {
    public:
        RAM();
        void clear();
        uint8_t read(uint16_t addr) const;
        void write(uint16_t addr, uint8_t val);
        void push(uint8_t& pointer, uint8_t val, bool mute);
        uint8_t pull(uint8_t& pointer, bool mute);

    private:
        uint8_t data[RAM_SIZE]; // RAM in the CPU memory map

        uint16_t getLocalAddr(uint16_t addr) const;
};

#endif