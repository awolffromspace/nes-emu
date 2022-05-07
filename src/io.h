#ifndef IO_H
#define IO_H

#include <cstdint>
#include <iostream>
#include <string.h>

class IO {
    public:
        IO();
        void clear(bool mute);
        uint8_t readIO(uint16_t addr) const;
        void writeIO(uint16_t addr, uint8_t val, bool mute);

    private:
        uint8_t registers[0xa];
};

#endif