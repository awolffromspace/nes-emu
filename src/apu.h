#ifndef APU_H
#define APU_H

#include <cstdint>
#include <iostream>
#include <string.h>

#define APU_REGISTER_SIZE 0x16
#define SQ1_VOL 0x4000

class APU {
    public:
        APU();
        void clear(bool mute);
        uint8_t readIO(uint16_t addr) const;
        void writeIO(uint16_t addr, uint8_t val, bool mute);

    private:
        uint8_t registers[APU_REGISTER_SIZE];
};

#endif