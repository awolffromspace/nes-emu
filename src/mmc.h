#ifndef MMC_H
#define MMC_H

#pragma once
class PPU;

#include <climits>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "ppu.h"

#define DEFAULT_CHR_BANK_SIZE 0x1000
#define DEFAULT_PRG_BANK_SIZE 0x4000
#define HEADER_SIZE 0x10
#define LOWER_RESET_ADDR 0xfffc
#define PRG_RAM_SIZE 0x8000 - 0x4020
#define PRG_RAM_START 0x4020
#define PRG_ROM_START 0x8000
#define TRAINER_SIZE 0x200
#define UPPER_RESET_ADDR 0xfffd

// Memory Management Controller (Mapper)
// Handles anything related to the cartridge. Stores data for addresses $4020 - $7fff (PRG-RAM) and
// $8000 - $ffff (PRG-ROM) in the CPU memory map as well as addresses $0000 - $1fff (CHR memory or
// pattern tables) in the PPU memory map

class MMC {
    public:
        MMC();
        void clear();
        uint8_t readPRG(uint16_t addr) const;
        void writePRG(uint16_t addr, uint8_t val, unsigned int totalCycles);
        uint8_t readCHR(uint16_t addr) const;
        void writeCHR(uint16_t addr, uint8_t val);
        void readInInst(const std::string& filename);
        void readInINES(const std::string& filename, PPU& ppu);
        unsigned int getMirroring() const;

        enum Mirroring {
            Horizontal = 0,
            Vertical = 1,
            SingleScreen0 = 2,
            SingleScreen1 = 3,
            SingleScreen2 = 4,
            SingleScreen3 = 5,
            FourScreen = 6
        };

    private:
        // PRG-RAM (i.e., additional workspace for the program)
        uint8_t prgRAM[PRG_RAM_SIZE];
        // PRG-ROM (i.e., the program)
        std::vector<uint8_t> prgROM;
        // CHR-ROM (i.e., character data, which are pattern tables) and CHR-RAM (i.e., additional
        // work space or modifiable pattern tables)
        std::vector<uint8_t> chrMemory;
        // Number of PRG banks
        unsigned int prgROMSize;
        // Number of CHR banks
        unsigned int chrMemorySize;
        unsigned int mirroring;
        // Mapper ID: https://www.nesdev.org/wiki/Mapper
        unsigned int mapperID;
        uint8_t shiftRegister;
        unsigned int prgBankMode;
        unsigned int chrBankMode;
        unsigned int prgBank;
        unsigned int chrBank0;
        unsigned int chrBank1;
        bool chrRAM;
        unsigned int lastWriteCycle;
        bool testMode;

        unsigned int getLocalPRGAddr(unsigned int addr) const;
        void writeShiftRegister(uint16_t addr, uint8_t val, unsigned int totalCycles);
        unsigned int getMapper1PRGAddr(unsigned int addr) const;
        unsigned int getMapper2PRGAddr(unsigned int addr) const;
        unsigned int getLocalCHRAddr(unsigned int addr) const;
        unsigned int getMapper1CHRAddr(unsigned int addr) const;
        void updateSettings(uint16_t addr);
};

#endif