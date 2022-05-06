#include "mmc.h"

MMC::MMC() :
        prgMemory(0x10000 - 0x8000, 0),
        chrMemory(0x2000, 0),
        prgMemorySize(1),
        chrMemorySize(1),
        mapperID(0) {
    // Set the reset vector to 0x8000, the beginning of the PRG-ROM lower bank
    // This is where test programs will start at
    writePRG(0xfffd, 0x80, true);
}

void MMC::clear(bool mute) {
    memset(saveWorkRAM, 0, 0x8000 - 0x4020);
    prgMemory.resize(0x10000 - 0x8000);
    for (uint8_t& entry : prgMemory) {
        entry = 0;
    }
    chrMemory.resize(0x2000);
    for (uint8_t& entry : chrMemory) {
        entry = 0;
    }
    prgMemorySize = 1;
    chrMemorySize = 1;
    writePRG(0xfffd, 0x80, mute);
    mapperID = 0;

    if (!mute) {
        std::cout << "MMC was cleared\n";
    }
}

uint8_t MMC::readPRG(uint16_t addr) const {
    if (addr < 0x8000) {
        addr -= 0x4020;
        return saveWorkRAM[addr];
    }
    if (prgMemorySize == 1) {
        addr &= 0xbfff;
    }
    addr -= 0x8000;
    return prgMemory[addr];
}

void MMC::writePRG(uint16_t addr, uint8_t val, bool mute) {
    uint16_t localAddr = addr;
    if (localAddr < 0x8000) {
        localAddr -= 0x4020;
        saveWorkRAM[localAddr] = val;
    } else {
        if (prgMemorySize == 1) {
            localAddr &= 0xbfff;
        }
        localAddr -= 0x8000;
        prgMemory[localAddr] = val;
    }

    if (!mute) {
        std::cout << std::hex << "0x" << (unsigned int) val << " has " <<
            "been written to the address 0x" << (unsigned int) addr <<
            "\n--------------------------------------------------\n" <<
            std::dec;
    }
}

uint8_t MMC::readCHR(uint16_t addr) const {
    return chrMemory[addr];
}

void MMC::writeCHR(uint16_t addr, uint8_t val, bool mute) {
    chrMemory[addr] = val;

    if (!mute) {
        std::cout << std::hex << "0x" << (unsigned int) val << " has " <<
            "been written to the address 0x" << (unsigned int) addr <<
            "\n--------------------------------------------------\n" <<
            std::dec;
    }
}

void MMC::readInInst(std::string& filename) {
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
            if (line.at(i) == '/' || line.at(i) == ' ') {
                break;
            }

            substring += line.at(i);

            if (i % 2 == 1) {
                prgMemory[addr] = std::stoul(substring, nullptr, 16);
                ++addr;
                substring = "";
            }
        }
    }

    file.close();
}

void MMC::readInINES(std::string& filename, PPU& ppu) {
    std::ifstream file(filename.c_str(), std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Error reading in file\n";
        exit(1);
    }

    uint8_t readInByte;
    bool hasTrainer = false;
    for (unsigned int i = 0; i < 0x10; ++i) {
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

    if (hasTrainer) {
        for (unsigned int i = 0; i < 0x200; ++i) {
            file.read((char*) &readInByte, 1);
        }
    }

    for (unsigned int i = 0; i < prgMemorySize * 0x4000; ++i) {
        file.read((char*) &prgMemory[i], 1);
    }

    for (unsigned int i = 0; i < chrMemorySize * 0x2000; ++i) {
        file.read((char*) &chrMemory[i], 1);
    }
}