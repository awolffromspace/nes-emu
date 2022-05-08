#ifndef RAM_H
#define RAM_H

#include <cstdint>
#include <iostream>
#include <string.h>

#define RAM_SIZE 0x800

class RAM {
    public:
        RAM();
        void clear(bool mute);
        uint8_t read(uint16_t addr) const;
        void write(uint16_t addr, uint8_t val, bool mute);
        void push(uint8_t& pointer, uint8_t val, bool mute);
        uint8_t pull(uint8_t& pointer, bool mute);

    private:
        uint8_t data[RAM_SIZE];
};

#endif