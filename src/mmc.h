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

#define SAVE_WORK_RAM_SIZE 0x8000 - 0x4020
#define SAVE_WORK_RAM_START 0x4020
#define PRG_BANK_SIZE 0x4000
#define PRG_MEMORY_START 0x8000
#define CHR_BANK_SIZE 0x2000
#define HEADER_SIZE 0x10
#define TRAINER_SIZE 0x200
#define UPPER_RESET_ADDR 0xfffd
#define LOWER_RESET_ADDR 0xfffc

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
        uint8_t saveWorkRAM[SAVE_WORK_RAM_SIZE];
        std::vector<uint8_t> prgMemory;
        std::vector<uint8_t> chrMemory;
        unsigned int prgMemorySize;
        unsigned int chrMemorySize;
        uint8_t mapperID;
};

#endif