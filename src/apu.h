#ifndef APU_H
#define APU_H

#include <cstdint>
#include <cstring>
#include <iostream>

// Audio Processing Unit
// Handles anything related to audio. Stores data for addresses $4000 - $4013, $4015, and $4017 in
// the CPU memory map. Note: the APU is not implemented yet

class APU {
    public:
        APU();
        void clear();
        uint8_t readRegister(const uint16_t addr) const;
        void writeRegister(const uint16_t addr, const uint8_t val);

    private:
        uint8_t registers[0x16]; // APU registers in the CPU memory map

        uint16_t getLocalAddr(const uint16_t addr) const;
};

#endif