#ifndef MEMORY_H
#define MEMORY_H

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <string.h>

class Memory {
    public:
        Memory();
        void reset(bool mute);
        uint8_t read(uint16_t addr);
        void write(uint16_t addr, uint8_t val);
        void push(uint8_t& pointer, uint8_t val);
        uint8_t pull(uint8_t& pointer);
        void readInInst(std::string& filename);

    private:
        uint8_t data[65536];
};

#endif