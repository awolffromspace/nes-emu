#ifndef APU_H
#define APU_H

#include <cstdint>
#include <iostream>
#include <string.h>

class APU {
    public:
        APU();
        void clear(bool mute);
        uint8_t readIO(uint16_t addr) const;
        void writeIO(uint16_t addr, uint8_t val, bool mute);

    private:
        uint8_t registers[0x16];
};

#endif