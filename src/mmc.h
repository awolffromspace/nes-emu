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
        uint8_t prgRAM[0x8000 - 0x4020];
        // PRG-ROM (i.e., the program)
        std::vector<uint8_t> prgROM;
        // CHR-ROM (i.e., character data, which are pattern tables) and CHR-RAM (i.e., additional
        // work space or modifiable pattern tables)
        std::vector<uint8_t> chrMemory;
        // Number of PRG banks
        unsigned int prgROMSize;
        // Number of CHR banks
        unsigned int chrMemorySize;
        // Which nametable mirroring to use: https://www.nesdev.org/wiki/Mirroring. Depends on enum
        // Mirroring
        unsigned int mirroring;
        // Mapper ID which is the type of MMC: https://www.nesdev.org/wiki/Mapper
        unsigned int mapperID;
        // 5-bit shift register for MMC1: https://www.nesdev.org/wiki/MMC1#Registers
        uint8_t shiftRegister;
        // PRG bank mode that specifies settings for the PRG banks, such as the size of a bank and
        // which banks to switch. Has a different meaning depending what the mapper ID is
        unsigned int prgBankMode;
        // CHR bank mode that specifies settings for the CHR banks, such as the size of a bank and
        // which banks to switch. Has a different meaning depending what the mapper ID is
        unsigned int chrBankMode;
        // The PRG bank that is currently swapped in
        unsigned int prgBank;
        // The first CHR bank that is currently swapped in
        unsigned int chrBank0;
        // The second CHR bank that is currently swapped in
        unsigned int chrBank1;
        // Set to true if CHR-RAM is enabled. This happens when the CHR-ROM has no size specified in
        // the .NES file
        bool chrRAM;
        // The last cycle that the CPU wrote to the MMC on
        unsigned int lastWriteCycle;
        // Set to true for instruction tests, which allows them to write to the PRG-ROM
        bool testMode;

        unsigned int getLocalPRGAddr(unsigned int addr) const;
        unsigned int getMapper1PRGAddr(unsigned int addr) const;
        unsigned int getMapper2PRGAddr(unsigned int addr) const;
        unsigned int getLocalCHRAddr(unsigned int addr) const;
        unsigned int getMapper1CHRAddr(unsigned int addr) const;
        void writeShiftRegister(uint16_t addr, uint8_t val, unsigned int totalCycles);
        void updateSettings(uint16_t addr);
        void expandCHRMemory(unsigned int selectedCHRBank);
};

#endif