#ifndef MEMORY_H
#define MEMORY_H

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

class Memory {
    public:
        Memory();
        Memory(std::string filename);
        Memory(uint8_t d[]);
        uint8_t read(uint16_t addr);
        void write(uint16_t addr, uint8_t val);
        void push(uint8_t &pointer, uint8_t val);
        uint8_t pull(uint8_t &pointer);

    private:
        uint8_t data[65536];
};

#endif