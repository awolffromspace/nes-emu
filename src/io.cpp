#include "io.h"

// Public Member Functions

IO::IO() :
        strobe(false),
        currentButton(A),
        a(0),
        b(0),
        select(0),
        start(0),
        up(0),
        down(0),
        left(0),
        right(0) {
    memset(registers, 0, IO_REGISTER_SIZE);
}

void IO::clear() {
    memset(registers, 0, IO_REGISTER_SIZE);
    strobe = false;
    currentButton = A;
    a = 0;
    b = 0;
    select = 0;
    start = 0;
    up = 0;
    down = 0;
    left = 0;
    right = 0;
}

// Handles register reads from the CPU

uint8_t IO::readRegister(uint16_t addr) {
    addr = getLocalAddr(addr);
    // If strobe mode is on, only return the status of the A button
    if (addr == 0 && strobe) {
        // Clear the primary controller status bit
        registers[addr] &= 0xfe;
        registers[addr] |= a;
    // If strobe mode is off, cycle through each button on each CPU read
    } else if (addr == 0 && !strobe) {
        // Clear the primary controller status bit
        registers[addr] &= 0xfe;
        switch (currentButton) {
            case A:
                registers[addr] |= a;
                break;
            case B:
                registers[addr] |= b;
                break;
            case Select:
                registers[addr] |= select;
                break;
            case Start:
                registers[addr] |= start;
                break;
            case Up:
                registers[addr] |= up;
                break;
            case Down:
                registers[addr] |= down;
                break;
            case Left:
                registers[addr] |= left;
                break;
            case Right:
                registers[addr] |= right;
        }
        ++currentButton;
        // Wraparound back to the A button
        if (currentButton > Right) {
            currentButton = A;
        }
    } else if (addr == 1) {
        registers[addr] = 0;
    }
    // This bit is always set on the bus
    registers[addr] |= 0x40;
    return registers[addr];
}

// Handles register writes from the CPU

void IO::writeRegister(uint16_t addr, uint8_t val) {
    addr = getLocalAddr(addr);
    registers[addr] = val;

    if (addr == 0) {
        if (val & 1) {
            strobe = true;
            // Reset to the A button whenever strobe mode is set
            currentButton = A;
        } else {
            strobe = false;
        }
    }
}

// Whenever the runNESGame function in emulator.cpp receives a key press or release, this function
// gets called. This function sets the first bit of the specified button, which later gets ORed with
// joystick register to signify a button press

void IO::updateButton(const SDL_Event& event) {
    uint8_t pressed = 0;
    if (event.type == SDL_KEYDOWN) {
        pressed = 1;
    }
    switch (event.key.keysym.sym) {
        case SDLK_x:
            a = pressed;
            break;
        case SDLK_z:
            b = pressed;
            break;
        case SDLK_UP:
            up = pressed;
            break;
        case SDLK_DOWN:
            down = pressed;
            break;
        case SDLK_LEFT:
            left = pressed;
            break;
        case SDLK_RIGHT:
            right = pressed;
            break;
        case SDLK_RETURN:
            start = pressed;
            break;
        case SDLK_RSHIFT:
            select = pressed;
    }
}

// Private Member Functions

// Maps the CPU address to the I/O local field, registers

uint16_t IO::getLocalAddr(uint16_t addr) const {
    // Subtract by 0x4016 so that $4016 becomes 0 and $4017 becomes 1
    return addr - 0x4016;
}