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
    memset(registers, 0, 2);
}

void IO::clear() {
    memset(registers, 0, 2);
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

uint8_t IO::readRegister(const uint16_t addr) {
    const uint16_t localAddr = getLocalAddr(addr);
    const uint8_t primaryControllerStatus = 1;
    // If strobe mode is on, only return the status of the A button
    if (localAddr == 0 && strobe) {
        // Clear the primary controller status bit
        registers[localAddr] &= ~primaryControllerStatus;
        registers[localAddr] |= a;
    // If strobe mode is off, cycle through each button on each CPU read
    } else if (localAddr == 0 && !strobe) {
        // Clear the primary controller status bit
        registers[localAddr] &= ~primaryControllerStatus;
        switch (currentButton) {
            case A:
                registers[localAddr] |= a;
                break;
            case B:
                registers[localAddr] |= b;
                break;
            case Select:
                registers[localAddr] |= select;
                break;
            case Start:
                registers[localAddr] |= start;
                break;
            case Up:
                registers[localAddr] |= up;
                break;
            case Down:
                registers[localAddr] |= down;
                break;
            case Left:
                registers[localAddr] |= left;
                break;
            case Right:
                registers[localAddr] |= right;
        }
        ++currentButton;
        // Wraparound back to the A button
        if (currentButton > Right) {
            currentButton = A;
        }
    } else if (localAddr == 1) {
        registers[localAddr] = 0;
    }
    // This bit is always set on the bus
    registers[localAddr] |= 0x40;
    return registers[localAddr];
}

// Handles register writes from the CPU

void IO::writeRegister(const uint16_t addr, const uint8_t val) {
    const uint16_t localAddr = getLocalAddr(addr);
    registers[localAddr] = val;

    if (localAddr == 0) {
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

uint16_t IO::getLocalAddr(const uint16_t addr) const {
    // Subtract by 0x4016 so that $4016 becomes 0 and $4017 becomes 1
    return addr - 0x4016;
}