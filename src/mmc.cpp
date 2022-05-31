#include "mmc.h"

// Public Member Functions

MMC::MMC() :
        prgMemory(PRG_BANK_SIZE * 2, 0),
        chrMemory(CHR_BANK_SIZE * 2, 0),
        prgMemorySize(2),
        chrMemorySize(2),
        mapperID(0),
        shiftRegister(0x10),
        mirroring(0),
        prgBankMode(0),
        chrBankMode(0),
        prgBank(0),
        chrBank0(0),
        chrBank1(0),
        prgRAM(false),
        lastWriteCycle(0),
        testMode(false) { }

void MMC::clear() {
    memset(saveWorkRAM, 0, SAVE_WORK_RAM_SIZE);
    prgMemory.resize(PRG_BANK_SIZE * 2);
    for (uint8_t& entry : prgMemory) {
        entry = 0;
    }
    chrMemory.resize(CHR_BANK_SIZE * 2);
    for (uint8_t& entry : chrMemory) {
        entry = 0;
    }
    prgMemorySize = 2;
    chrMemorySize = 2;
    mapperID = 0;
    shiftRegister = 0x10;
    mirroring = 0;
    prgBankMode = 0;
    chrBankMode = 0;
    prgBank = 0;
    chrBank0 = 0;
    chrBank1 = 0;
    prgRAM = false;
    lastWriteCycle = 0;
    testMode = false;
}

// Handles I/O reads from the CPU

uint8_t MMC::readPRG(uint16_t addr) const {
    unsigned int localAddr = getLocalPRGAddr(addr);
    if (addr < PRG_MEMORY_START) {
        return saveWorkRAM[localAddr];
    }
    return prgMemory[localAddr];
}

// Handles I/O writes from the CPU

void MMC::writePRG(uint16_t addr, uint8_t val, unsigned int totalCycles) {
    unsigned int localAddr = getLocalPRGAddr(addr);
    if (addr < PRG_MEMORY_START) {
        if ((addr >= 0x6000 && prgRAM) || testMode) {
            saveWorkRAM[localAddr] = val;
        }
        return;
    }

    if (testMode) {
        prgMemory[localAddr] = val;
        return;
    }

    unsigned int lastWriteCycleWrapped = 0;
    unsigned int totalCyclesWrapped = 0;
    if (lastWriteCycle > totalCycles) {
        lastWriteCycleWrapped = UINT_MAX - lastWriteCycle;
        totalCyclesWrapped = totalCycles + lastWriteCycleWrapped + 1;
    }
    if (mapperID == 1) {
        if (val & 0x80) {
            shiftRegister = 0x10;
        } else if (lastWriteCycle + 1 < totalCycles ||
                lastWriteCycleWrapped + 1 < totalCyclesWrapped) {
            bool full = false;
            if (shiftRegister & 1) {
                full = true;
            }
            shiftRegister = shiftRegister >> 1;
            shiftRegister |= (val & 1) << 4;
            if (full) {
                updateSettings(addr);
            }
        }
        lastWriteCycle = totalCycles;
    }
}

// Handles I/O reads from the PPU

uint8_t MMC::readCHR(uint16_t addr) const {
    unsigned int localAddr = getLocalCHRAddr(addr);
    return chrMemory[localAddr];
}

// Handles I/O writes from the PPU

void MMC::writeCHR(uint16_t addr, uint8_t val) {
    unsigned int localAddr = getLocalCHRAddr(addr);
    if (chrRAM || testMode) {
        chrMemory[localAddr] = val;
    }
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

    testMode = true;
    // Set the reset vector to $8000, the beginning of the PRG-ROM lower bank. This is where test
    // programs will start at
    prgMemory[UPPER_RESET_ADDR - PRG_MEMORY_START] = 0x80;
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
            prgMemory.resize(prgMemorySize * PRG_BANK_SIZE);
        } else if (i == 5) {
            chrMemorySize = readInByte;
            chrMemory.resize(chrMemorySize * CHR_BANK_SIZE * 2);
        } else if (i == 6) {
            if (readInByte & 1) {
                mirroring = Vertical;
            } else {
                mirroring = Horizontal;
            }
            if (readInByte & 4) {
                hasTrainer = true;
            }
            mapperID = (readInByte & 0xf0) >> 4;
        } else if (i == 7) {
            mapperID |= readInByte & 0xf0;
        }
    }

    if (mapperID > 1) {
        std::cerr << "Only mapper 0 and 1 are supported\n";
        exit(1);
    }

    // Skip over trainer data if there is any
    if (hasTrainer) {
        for (unsigned int i = 0; i < TRAINER_SIZE; ++i) {
            file.read((char*) &readInByte, 1);
        }
    }

    // Read in PRG-ROM
    for (unsigned int i = 0; i < prgMemorySize * PRG_BANK_SIZE; ++i) {
        file.read((char*) &prgMemory[i], 1);
    }

    // Read in CHR-ROM
    for (unsigned int i = 0; i < chrMemorySize * CHR_BANK_SIZE * 2; ++i) {
        file.read((char*) &chrMemory[i], 1);
    }

    file.close();

    if (mapperID == 1) {
        prgBankMode = 3;
        prgBank = prgMemorySize - 1;
    }

    if (chrMemorySize == 0) {
        chrRAM = true;
        chrMemory.resize(CHR_BANK_SIZE * 32);
    }
}

unsigned int MMC::getMirroring() const {
    return mirroring;
}

// Private Member Functions

// Maps the CPU address to the MMC's local fields, saveWorkRAM and prgMemory

unsigned int MMC::getLocalPRGAddr(unsigned int addr) const {
    if (addr < PRG_MEMORY_START) {
        // The cartridge space is from $4020 - $ffff in the CPU memory map. The address is
        // subtracted by 0x4020 so that $4020 becomes 0, $4021 becomes 1, etc.
        return addr - SAVE_WORK_RAM_START;
    }
    if (prgMemorySize == 1) {
        // If there is only one PRG bank, then $c000 - $ffff is a mirror of $8000 - $bfff. This
        // operation clears out the upper bits that set the address to a value higher than $bfff, so
        // the result is a mirrored address
        addr &= 0xbfff;
    } else if (mapperID == 1) {
        if (prgBankMode < 2) {
            addr += (prgBank >> 1) * PRG_BANK_SIZE * 2;
        } else if (prgBankMode == 2) {
            if (addr >= 0xc000) {
                addr += (prgBank - 1) * PRG_BANK_SIZE;
            }
        } else {
            if (addr < 0xc000) {
                addr += prgBank * PRG_BANK_SIZE;
            } else {
                addr += (prgMemorySize - 2) * PRG_BANK_SIZE;
            }
        }
    }
    // PRG memory starts at $8000. The address is subtracted by 0x8000 so that $8000 becomes 0,
    // $8001 becomes 1, etc.
    return addr - PRG_MEMORY_START;
}

unsigned int MMC::getLocalCHRAddr(unsigned int addr) const {
    if (mapperID == 1) {
        if (chrBankMode) {
            unsigned int lowerBank = chrBank0;
            unsigned int upperBank = chrBank1;
            if (mirroring == SingleScreenLowerBank) {
                upperBank = lowerBank;
            } else if (mirroring == SingleScreenUpperBank) {
                lowerBank = upperBank;
            }
            if (addr < 0x1000) {
                addr += lowerBank * CHR_BANK_SIZE;
            } else {
                addr += (upperBank - 1) * CHR_BANK_SIZE;
            }
        } else {
            addr += (chrBank0 >> 1) * CHR_BANK_SIZE * 2;
        }
    }
    return addr;
}

void MMC::updateSettings(uint16_t addr) {
    if (addr < 0xa000) {
        unsigned int mirrorVal = shiftRegister & 3;
        if (mirrorVal == 0) {
            mirroring = SingleScreenLowerBank;
        } else if (mirrorVal == 1) {
            mirroring = SingleScreenUpperBank;
        } else if (mirrorVal == 2) {
            mirroring = Vertical;
        } else {
            mirroring = Horizontal;
        }
        prgBankMode = (shiftRegister >> 2) & 3;
        chrBankMode = (shiftRegister >> 4) & 1;
    } else if (addr < 0xc000) {
        chrBank0 = shiftRegister & 0x1f;
    } else if (addr < 0xe000) {
        chrBank1 = shiftRegister & 0x1f;
    } else {
        prgBank = shiftRegister & 0xf;
        prgRAM = !((shiftRegister >> 4) & 1);
    }
    shiftRegister = 0x10;
}