#ifndef IO_H
#define IO_H

#include <cstdint>
#include <iostream>
#include <string.h>

#define IO_REGISTER_SIZE 0xa
#define JOY1 0x4016
#define JOY2 0x4017

class IO {
    public:
        IO();
        void clear(bool mute);
        uint8_t readIO(uint16_t addr) const;
        void writeIO(uint16_t addr, uint8_t val, bool mute);

    private:
        uint8_t registers[IO_REGISTER_SIZE];
};

#endif