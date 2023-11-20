#ifndef RAM_H
#define RAM_H

#include <cstdint>
#include <cstring>
#include <iostream>

// Random Access Memory
// The main workspace for programs. Stores data for addresses $0000 - $00ff (zero page), $0100 -
// $01ff (stack), $0200 - $07ff (additional RAM), and $0800 - $1fff (mirrored addresses) in the CPU
// memory map

class RAM {
    public:
        RAM();
        void clear();
        uint8_t read(const uint16_t addr) const;
        void write(const uint16_t addr, const uint8_t val);
        void push(uint8_t& pointer, const uint8_t val, const bool mute);
        uint8_t pull(uint8_t& pointer, const bool mute);

    private:
        uint8_t data[0x800]; // RAM in the CPU memory map

        uint16_t getLocalAddr(const uint16_t addr) const;
};

#endif