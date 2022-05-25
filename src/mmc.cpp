#include "mmc.h"

// Public Member Functions

MMC::MMC() :
        prgMemory(PRG_BANK_SIZE * 2, 0),
        chrMemory(CHR_BANK_SIZE, 0),
        prgMemorySize(1),
        chrMemorySize(1),
        mapperID(0) {
    // Set the reset vector to $8000, the beginning of the PRG-ROM lower bank. This is where test
    // programs will start at
    writePRG(UPPER_RESET_ADDR, 0x80);
}

void MMC::clear() {
    memset(saveWorkRAM, 0, SAVE_WORK_RAM_SIZE);
    prgMemory.resize(PRG_BANK_SIZE * 2);
    for (uint8_t& entry : prgMemory) {
        entry = 0;
    }
    chrMemory.resize(CHR_BANK_SIZE);
    for (uint8_t& entry : chrMemory) {
        entry = 0;
    }
    prgMemorySize = 1;
    chrMemorySize = 1;
    // Set the reset vector to $8000, the beginning of the PRG-ROM lower bank. This is where test
    // programs will start at
    writePRG(UPPER_RESET_ADDR, 0x80);
    mapperID = 0;
}

// Handles I/O reads from the CPU

uint8_t MMC::readPRG(uint16_t addr) const {
    uint16_t localAddr = getLocalPRGAddr(addr);
    if (addr < PRG_MEMORY_START) {
        return saveWorkRAM[localAddr];
    }
    return prgMemory[localAddr];
}

// Handles I/O writes from the CPU

void MMC::writePRG(uint16_t addr, uint8_t val) {
    uint16_t localAddr = getLocalPRGAddr(addr);
    if (addr < PRG_MEMORY_START) {
        saveWorkRAM[localAddr] = val;
    } else {
        prgMemory[localAddr] = val;
    }
}

// Handles I/O reads from the PPU

uint8_t MMC::readCHR(uint16_t addr) const {
    return chrMemory[addr];
}

// Handles I/O writes from the PPU

void MMC::writeCHR(uint16_t addr, uint8_t val) {
    chrMemory[addr] = val;
}

// Reads in an "instruction" file, which is just a text file of hexadecimal instructions separated
// by new lines

void MMC::readInInst(const std::string& filename) {
    std::string line;
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        std::cerr << "Error reading in file\n";
        exit(1);
    }

    unsigned int addr = 0;
    while (file.good()) {
        getline(file, line);
        std::string substring = "";

        for (unsigned int i = 0; i < line.size(); ++i) {
            // Ignore comments and spaces
            if (line.at(i) == '/' || line.at(i) == ' ') {
                break;
            }

            substring += line.at(i);

            // Every two characters in an instruction file represents a byte
            if (i % 2) {
                prgMemory[addr] = std::stoul(substring, nullptr, 16);
                ++addr;
                substring = "";
            }
        }
    }
    file.close();
}

// Reads in an .NES file: https://www.nesdev.org/wiki/INES

void MMC::readInINES(const std::string& filename, PPU& ppu) {
    std::ifstream file(filename.c_str(), std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error reading in file\n";
        exit(1);
    }

    uint8_t readInByte;
    bool hasTrainer = false;
    // Use any relevant info from the header
    for (unsigned int i = 0; i < HEADER_SIZE; ++i) {
        file.read((char*) &readInByte, 1);

        if (i == 4) {
            prgMemorySize = readInByte;
        } else if (i == 5) {
            chrMemorySize = readInByte;
        } else if (i == 6) {
            if (readInByte & 1) {
                ppu.setMirroring(PPU::Vertical);
            } else {
                ppu.setMirroring(PPU::Horizontal);
            }
            if (readInByte & 4) {
                hasTrainer = true;
            }
            mapperID = (readInByte & 0xf0) >> 4;
        } else if (i == 7) {
            mapperID |= readInByte & 0xf0;
        }
    }

    if (mapperID != 0 || prgMemorySize > 2 || chrMemorySize > 1) {
        std::cerr << "Only mapper 0 is supported\n";
        exit(1);
    }

    // Skip over trainer data if there is any
    if (hasTrainer) {
        for (unsigned int i = 0; i < TRAINER_SIZE; ++i) {
            file.read((char*) &readInByte, 1);
        }
    }

    // Read in PRG-ROM
    for (unsigned int i = 0; i < PRG_BANK_SIZE * prgMemorySize; ++i) {
        file.read((char*) &prgMemory[i], 1);
    }

    // Read in CHR-ROM
    for (unsigned int i = 0; i < chrMemorySize * CHR_BANK_SIZE; ++i) {
        file.read((char*) &chrMemory[i], 1);
    }

    file.close();
}

// Private Member Functions

// Maps the CPU address to the MMC's local fields, saveWorkRAM and prgMemory

uint16_t MMC::getLocalPRGAddr(uint16_t addr) const {
    if (addr < PRG_MEMORY_START) {
        // The cartridge space is from $4020 - $ffff in the CPU memory map. The address is
        // subtracted by 0x4020 so that $4020 becomes 0, $4021 becomes 1, etc.
        return addr - SAVE_WORK_RAM_START;
    }
    // If there is only one PRG bank, then $c000 - $ffff is a mirror of $8000 - $bfff. This
    // operation clears out the upper bits that set the address to a value higher than $bfff, so the
    // result is a mirrored address
    if (prgMemorySize == 1) {
        addr &= 0xbfff;
    }
    // PRG memory starts at $8000. The address is subtracted by 0x8000 so that $8000 becomes 0,
    // $8001 becomes 1, etc.
    return addr - PRG_MEMORY_START;
}