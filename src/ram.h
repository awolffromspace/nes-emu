#ifndef RAM_H
#define RAM_H

#include <cstdint>
#include <iostream>
#include <string.h>

class RAM {
    public:
        RAM();
        void clear(bool mute);
        uint8_t read(uint16_t addr);
        void write(uint16_t addr, uint8_t val, bool mute);
        void push(uint8_t& pointer, uint8_t val, bool mute);
        uint8_t pull(uint8_t& pointer, bool mute);

    private:
        uint8_t data[0x800];
};

#endif