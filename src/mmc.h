#ifndef MMC_H
#define MMC_H

#pragma once
class PPU;

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "ppu.h"

class MMC {
    public:
        MMC();
        void clear(bool mute);
        uint8_t readPRG(uint16_t addr) const;
        void writePRG(uint16_t addr, uint8_t val, bool mute);
        uint8_t readCHR(uint16_t addr) const;
        void writeCHR(uint16_t addr, uint8_t val, bool mute);
        void readInInst(std::string& filename);
        void readInINES(std::string& filename, PPU& ppu);

    private:
        uint8_t saveWorkRAM[0x8000 - 0x4020];
        std::vector<uint8_t> prgMemory;
        std::vector<uint8_t> chrMemory;
        unsigned int prgMemorySize;
        unsigned int chrMemorySize;
        uint8_t mapperID;
};

#endif