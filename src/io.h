#ifndef IO_H
#define IO_H

#include <cstdint>
#include <cstring>
#include <iostream>
#include <SDL.h>

// Input/Output (Joysticks)
// Handles anything related to I/O from the user. Stores data for addresses $4016 (joystick 1) and
// $4017 (joystick 2) in the CPU memory map

class IO {
    public:
        IO();
        void clear();
        uint8_t readRegister(const uint16_t addr);
        void writeRegister(const uint16_t addr, const uint8_t val);
        void updateButton(const SDL_Event& event);

    private:
        // I/O registers in the CPU memory map, including joystick 1 and joystick 2
        uint8_t registers[2];
        // Strobe mode. If strobe mode is set, only the status of the A button is set to the
        // joystick register. Otherwise, it cycles through each button on each CPU read
        bool strobe;
        // The current button to return the status of when strobe mode is off. Depends on enum
        // ButtonPress
        unsigned int currentButton;

        // Joystick buttons
        // Only the first bit of each field is used. These fields are ORed with the joystick
        // register to signify a button press

        uint8_t a;
        uint8_t b;
        uint8_t select;
        uint8_t start;
        uint8_t up;
        uint8_t down;
        uint8_t left;
        uint8_t right;

        uint16_t getLocalAddr(const uint16_t addr) const;

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