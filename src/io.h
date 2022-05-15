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
        uint8_t readIO(uint16_t addr);
        void writeIO(uint16_t addr, uint8_t val, bool mute);

    private:
        uint8_t registers[IO_REGISTER_SIZE];
        bool strobe;
        unsigned int currentButton;
        uint8_t a;
        uint8_t b;
        uint8_t select;
        uint8_t start;
        uint8_t up;
        uint8_t down;
        uint8_t left;
        uint8_t right;

        friend class CPU;

        enum ButtonPress {
            A = 0,
            B = 1,
            Select = 2,
            Start = 3,
            Up = 4,
            Down = 5,
            Left = 6,
            Right = 7
        };
};

#endif