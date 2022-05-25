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

#define CHR_BANK_SIZE 0x2000
#define HEADER_SIZE 0x10
#define LOWER_RESET_ADDR 0xfffc
#define PRG_BANK_SIZE 0x4000
#define PRG_MEMORY_START 0x8000
#define SAVE_WORK_RAM_SIZE 0x8000 - 0x4020
#define SAVE_WORK_RAM_START 0x4020
#define TRAINER_SIZE 0x200
#define UPPER_RESET_ADDR 0xfffd

// Memory Management Controller (Mapper)
// Handles anything related to the cartridge. Stores data for addresses $4020 - $7fff (Save or Work
// RAM) and $8000 - $ffff (PRG memory) in the CPU memory map as well as addresses $0000 - $1fff (CHR
// memory or pattern tables) in the PPU memory map

class MMC {
    public:
        MMC();
        void clear();
        uint8_t readPRG(uint16_t addr) const;
        void writePRG(uint16_t addr, uint8_t val);
        uint8_t readCHR(uint16_t addr) const;
        void writeCHR(uint16_t addr, uint8_t val);
        void readInInst(const std::string& filename);
        void readInINES(const std::string& filename, PPU& ppu);

    private:
        // Battery Backed Save (aka SRAM) or Work RAM (aka Expansion ROM)
        uint8_t saveWorkRAM[SAVE_WORK_RAM_SIZE];
        // PRG-ROM (i.e., the program) and PRG-RAM (i.e., additional workspace for the program).
        // Made up of banks, the number of which is determined by the mapper ID
        std::vector<uint8_t> prgMemory;
        // CHR-ROM (i.e., character data, which are pattern tables) and CHR-RAM (i.e., additional
        // work space or modifiable pattern tables). Made up of banks, the number of which is
        // determined by the mapper ID
        std::vector<uint8_t> chrMemory;
        // Number of PRG banks
        unsigned int prgMemorySize;
        // Number of CHR banks
        unsigned int chrMemorySize;
        // Mapper ID: https://www.nesdev.org/wiki/Mapper
        uint8_t mapperID;

        uint16_t getLocalPRGAddr(uint16_t addr) const;
};

#endif