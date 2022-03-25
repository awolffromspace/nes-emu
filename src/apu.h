#ifndef APU_H
#define APU_H

#include <cstdint>
#include <iostream>
#include <string.h>

class APU {
    public:
        APU();
        void clear(bool mute);
        uint8_t readRegister(uint16_t addr);
        void writeRegister(uint16_t addr, uint8_t val, bool mute);

    private:
        uint8_t registers[0x15];
};

#endif