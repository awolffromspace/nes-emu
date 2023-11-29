#include "mmc.h"

// Public Member Functions

MMC::MMC() :
        prgROM(0x4000 * 2, 0),
        chrMemory(0x1000 * 2, 0),
        prgROMSize(2),
        chrMemorySize(2),
        mirroring(0),
        mapperID(0),
        shiftRegister(0x10),
        prgBankMode(0),
        chrBankMode(0),
        prgBank(0),
        chrBank0(0),
        chrBank1(0),
        chrRAM(false),
        lastWriteCycle(0),
        testMode(false) { }

void MMC::clear() {
    const uint16_t prgROMStart = 0x8000;
    const uint16_t ioRegisterEnd = 0x4020;
    memset(prgRAM, 0, prgROMStart - ioRegisterEnd);
    const uint16_t defaultPRGBankSize = 0x4000;
    prgROM.resize(defaultPRGBankSize * 2);
    for (uint8_t& entry : prgROM) {
        entry = 0;
    }
    const uint16_t defaultCHRBankSize = 0x1000;
    chrMemory.resize(defaultCHRBankSize * 2);
    for (uint8_t& entry : chrMemory) {
        entry = 0;
    }
    prgROMSize = 2;
    chrMemorySize = 2;
    mirroring = 0;
    mapperID = 0;
    shiftRegister = 0x10;
    prgBankMode = 0;
    chrBankMode = 0;
    prgBank = 0;
    chrBank0 = 0;
    chrBank1 = 0;
    chrRAM = false;
    lastWriteCycle = 0;
    testMode = false;
}

// Handles reads from the CPU

uint8_t MMC::readPRG(const uint16_t addr) const {
    const unsigned int localAddr = getLocalPRGAddr(addr);
    const uint16_t prgROMStart = 0x8000;
    // PRG-RAM is in $4020 - $7fff, while PRG-ROM is in $8000 - $ffff
    if (addr < prgROMStart) {
        return prgRAM[localAddr];
    }
    return prgROM[localAddr];
}

// Handles writes from the CPU

void MMC::writePRG(const uint16_t addr, const uint8_t val, const unsigned int totalCycles) {
    const unsigned int localAddr = getLocalPRGAddr(addr);
    const uint16_t prgROMStart = 0x8000;
    // PRG-RAM is in $4020 - $7fff, while PRG-ROM is in $8000 - $ffff
    if (addr < prgROMStart) {
        prgRAM[localAddr] = val;
        return;
    }
    // If test mode is enabled, allow writes to PRG-ROM
    if (testMode) {
        prgROM[localAddr] = val;
        // Return early because test mode assumes 2 fixed PRG banks and no mapper
        return;
    }

    // Writes to the PRG-ROM are used by the CPU to control the MMC, and the mapper ID (i.e., the
    // type of MMC) determines how to intrepret the writes
    switch (mapperID) {
        case 1:
            // Mapper 1: https://www.nesdev.org/wiki/MMC1#Registers
            writeShiftRegister(addr, val, totalCycles);
            break;
        case 2:
            // Mapper 2: https://www.nesdev.org/wiki/UxROM#Registers
            prgBank = val & 0xf;
            break;
        case 3:
            // Mapper 3: https://www.nesdev.org/wiki/INES_Mapper_003#Registers
            chrBank0 = val & 3;
            expandCHRMemory(chrBank0);
            break;
        case 7:
            // Mapper 7: https://www.nesdev.org/wiki/AxROM#Registers
            prgBank = val & 7;
            if (val & 0x10) {
                mirroring = SingleScreen1;
            } else {
                mirroring = SingleScreen0;
            }
    }
}

// Handles reads from the PPU

uint8_t MMC::readCHR(const uint16_t addr) const {
    const unsigned int localAddr = getLocalCHRAddr(addr);
    return chrMemory[localAddr];
}

// Handles writes from the PPU

void MMC::writeCHR(const uint16_t addr, const uint8_t val) {
    const unsigned int localAddr = getLocalCHRAddr(addr);
    // If CHR-RAM or test mode is enabled, allow writes to CHR-ROM
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
            if (line[i] == '/' || line[i] == ' ' || line[i] == '\n') {
                break;
            }

            substring += line[i];

            // Every two characters in an instruction file represents a byte
            if (i % 2) {
                prgROM[addr] = std::stoul(substring, nullptr, 16);
                ++addr;
                substring = "";
            }
        }
    }
    file.close();

    testMode = true;
    const uint16_t upperResetAddr = 0xfffd;
    const uint16_t prgROMStart = 0x8000;
    // Set the reset vector to $8000, the beginning of the PRG-ROM lower bank. This is where test
    // programs will start at
    prgROM[upperResetAddr - prgROMStart] = 0x80;
}

// Reads in an .NES file: https://www.nesdev.org/wiki/INES

void MMC::readInINES(const std::string& filename) {
    std::ifstream file(filename.c_str(), std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error reading in file\n";
        exit(1);
    }

    const uint8_t headerSize = 0x10;
    uint8_t readInByte;
    bool hasTrainer = false;
    // Use any relevant info from the header
    for (unsigned int i = 0; i < headerSize; ++i) {
        file.read((char*) &readInByte, 1);
        if (i == 4) {
            const uint16_t defaultPRGBankSize = 0x4000;
            prgROMSize = readInByte;
            prgROM.resize(prgROMSize * defaultPRGBankSize);
        } else if (i == 5) {
            const uint16_t defaultCHRBankSize = 0x1000;
            chrMemorySize = readInByte;
            chrMemory.resize(chrMemorySize * defaultCHRBankSize * 2);
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

    if (mapperID > 3 && mapperID != 7) {
        std::cerr << "Only mappers 0, 1, 2, 3, and 7 are supported\n";
        exit(1);
    }

    const uint16_t trainerSize = 0x200;
    // Skip over trainer data if there is any
    if (hasTrainer) {
        for (unsigned int i = 0; i < trainerSize; ++i) {
            file.read((char*) &readInByte, 1);
        }
    }

    const uint16_t defaultPRGBankSize = 0x4000;
    // Read in PRG-ROM
    for (unsigned int i = 0; i < prgROMSize * defaultPRGBankSize; ++i) {
        file.read((char*) &prgROM[i], 1);
    }

    const uint16_t defaultCHRBankSize = 0x1000;
    // Read in CHR-ROM
    for (unsigned int i = 0; i < chrMemorySize * defaultCHRBankSize * 2; ++i) {
        file.read((char*) &chrMemory[i], 1);
    }

    file.close();

    // CHR memory size is unknown, so enable CHR-RAM and automatically resize to accomodate what the
    // game uses
    if (chrMemorySize == 0) {
        chrRAM = true;
    }

    // Pretty much all mapper 1 games power on the last bank by default:
    // https://www.nesdev.org/wiki/MMC1#Control_(internal,_$8000-$9FFF). This has to be assumed
    // because some games assume that the last bank's reset vector is being used on power up (e.g.,
    // Metroid).
    if (mapperID == 1) {
        prgBankMode = 3;
    }
}

unsigned int MMC::getMirroring() const {
    return mirroring;
}

// Private Member Functions

// Maps the CPU address to the MMC's local fields, prgRAM and prgROM

unsigned int MMC::getLocalPRGAddr(const unsigned int addr) const {
    unsigned int localAddr = addr;
    const uint16_t prgROMStart = 0x8000;
    if (localAddr < prgROMStart) {
        const uint16_t prgRAMStart = 0x4020;
        // The cartridge space is from $4020 - $ffff in the CPU memory map. The address is
        // subtracted by 0x4020 so that $4020 becomes 0, $4021 becomes 1, etc.
        return localAddr - prgRAMStart;
    }

    const uint16_t defaultPRGBankSize = 0x4000;
    switch (mapperID) {
        case 0:
            // If there is only one PRG bank, then $c000 - $ffff is a mirror of $8000 - $bfff. This
            // operation clears out the upper bits that set the address to a value higher than $bfff, so
            // the result is a mirrored address
            if (prgROMSize == 1) {
                localAddr &= 0xbfff;
            }
            break;
        case 1:
            localAddr = getMapper1PRGAddr(localAddr);
            break;
        case 2:
            localAddr = getMapper2PRGAddr(localAddr);
            break;
        case 7:
            // PRG bank size is always 32 KB: https://www.nesdev.org/wiki/AxROM
            localAddr += prgBank * defaultPRGBankSize * 2;
    }

    // PRG-ROM starts at $8000. The address is subtracted by 0x8000 so that $8000 becomes 0, $8001
    // becomes 1, etc.
    return localAddr - prgROMStart;
}

// Maps the CPU address to the MMC's local field, prgROM, based on mapper 1 settings:
// https://www.nesdev.org/wiki/MMC1#Registers

unsigned int MMC::getMapper1PRGAddr(const unsigned int addr) const {
    unsigned int localAddr = addr;
    const uint16_t defaultPRGBankSize = 0x4000;
    if (prgBankMode <= 1) {
        // Ignore low bit in 32 KB mode
        localAddr += (prgBank >> 1) * defaultPRGBankSize * 2;
    } else if (prgBankMode == 2) {
        if (localAddr >= 0xc000) {
            // Subtract by 1 because $c000 - $ffff is already shifted forward by 0x4000 (the default
            // PRG bank size). Bank 0 should be $8000 - $bfff, bank 1 should be $c000 - $ffff, bank
            // 2 should be $10000 - $13fff, etc. These address ranges later get subtracted by 0x8000
            // in getLocalPRGAddr so that bank 0 starts at 0 in the prgROM field
            localAddr += (prgBank - 1) * defaultPRGBankSize;
        }
    } else {
        if (localAddr < 0xc000) {
            localAddr += prgBank * defaultPRGBankSize;
        } else {
            // Fix the last bank to this range. Subtract by 1 because the bank numbers start at 0
            // instead of 1. Subtract by 1 again because $c000 - $ffff is already shifted forward by
            // 0x4000 (the default PRG bank size). Bank 0 should be $8000 - $bfff, bank 1 should be
            // $c000 - $ffff, bank 2 should be $10000 - $13fff, etc. These address ranges later get
            // subtracted by 0x8000 in getLocalPRGAddr so that bank 0 starts at 0 in the prgROM
            // field
            localAddr += (prgROMSize - 2) * defaultPRGBankSize;
        }
    }
    return localAddr;
}

// Maps the CPU address to the MMC's local field, prgROM, based on mapper 2 settings:
// https://www.nesdev.org/wiki/UxROM#Registers

unsigned int MMC::getMapper2PRGAddr(const unsigned int addr) const {
    unsigned int localAddr = addr;
    const uint16_t defaultPRGBankSize = 0x4000;
    if (localAddr < 0xc000) {
        localAddr += prgBank * defaultPRGBankSize;
    } else {
        // Fix the last bank to this range. Subtract by 1 because the bank numbers start at 0
        // instead of 1. Subtract by 1 again because $c000 - $ffff is already shifted forward by
        // 0x4000 (the default PRG bank size). Bank 0 should be $8000 - $bfff, bank 1 should be
        // $c000 - $ffff, bank 2 should be $10000 - $13fff, etc. These address ranges later get
        // subtracted by 0x8000 in getLocalPRGAddr so that bank 0 starts at 0 in the prgROM field
        localAddr += (prgROMSize - 2) * defaultPRGBankSize;
    }
    return localAddr;
}

// Maps the PPU address to the MMC's local field, chrMemory

unsigned int MMC::getLocalCHRAddr(const unsigned int addr) const {
    unsigned int localAddr = addr;
    const uint16_t defaultCHRBankSize = 0x1000;
    switch (mapperID) {
        case 1:
            localAddr = getMapper1CHRAddr(addr);
            break;
        case 3:
            // CHR bank size is always 8 KB: https://www.nesdev.org/wiki/INES_Mapper_003
            localAddr += chrBank0 * defaultCHRBankSize * 2;
    }
    return localAddr;
}

// Maps the PPU address to the MMC's local field, chrMemory, based on mapper 1 settings:
// https://www.nesdev.org/wiki/MMC1#Registers

unsigned int MMC::getMapper1CHRAddr(const unsigned int addr) const {
    unsigned int localAddr = addr;
    const uint16_t defaultCHRBankSize = 0x1000;
    if (chrBankMode) {
        if (localAddr < 0x1000) {
            localAddr += chrBank0 * defaultCHRBankSize;
        } else {
            // Subtract by 1 because $1000 - $1fff is already shifted forward by 0x1000 (the default
            // CHR bank size). Bank 0 should be $0000 - $0fff, bank 1 should be $1000 - $1fff, bank
            // 2 should be $2000 - $2fff, etc.
            localAddr += (chrBank1 - 1) * defaultCHRBankSize;
        }
    } else {
        // Ignore low bit in 8 KB mode
        localAddr += (chrBank0 >> 1) * defaultCHRBankSize * 2;
    }
    return localAddr;
}

// Shifts the new bit into mapper 1's shift register and updates the settings if the register is
// full: https://www.nesdev.org/wiki/MMC1#Registers

void MMC::writeShiftRegister(const uint16_t addr, const uint8_t val,
        const unsigned int totalCycles) {
    unsigned int lastWriteCycleWrapped = 0;
    unsigned int totalCyclesWrapped = 0;
    // The total cycles should always be greater than the last write cycle, so if this condition is
    // true, the total cycles must have wrapped around since the last write cycle. This if statement
    // uses local variables to ensure that the total cycles are greater than the last write cycle,
    // while maintaining the difference between the two
    if (lastWriteCycle > totalCycles) {
        lastWriteCycleWrapped = UINT_MAX - lastWriteCycle;
        totalCyclesWrapped = totalCycles + lastWriteCycleWrapped + 1;
    }
    // If bit 7 is set, reset the shift register
    if (val & 0x80) {
        shiftRegister = 0x10;
    // Only update the shift register if the last write was greater than 1 cycle ago
    } else if (lastWriteCycle + 1 < totalCycles ||
            lastWriteCycleWrapped + 1 < totalCyclesWrapped) {
        bool fullRegister = false;
        // If the 1 that was originally in bit 4 has been shifted to bit 0 (i.e., there have been 4
        // writes and this is the fifth write), then the shift register is now full and the settings
        // can be updated
        if (shiftRegister & 1) {
            fullRegister = true;
        }
        // Make room for the new bit on the far left
        shiftRegister = shiftRegister >> 1;
        // Insert the new bit at bit 4
        shiftRegister |= (val & 1) << 4;
        if (fullRegister) {
            updateSettings(addr);
        }
    }
    lastWriteCycle = totalCycles;
}

// Extracts the settings from the full shift register and assigns them to the MMC's fields:
// https://www.nesdev.org/wiki/MMC1#Registers

void MMC::updateSettings(const uint16_t addr) {
    const uint16_t chrBank0Start = 0xa000;
    const uint16_t chrBank1Start = 0xc000;
    const uint16_t prgBankStart = 0xe000;
    // The address of the fifth write determines which settings are updated
    if (addr < chrBank0Start) {
        unsigned int mirrorVal = shiftRegister & 3;
        switch (mirrorVal) {
            case 0:
                mirroring = SingleScreen0;
                break;
            case 1:
                mirroring = SingleScreen1;
                break;
            case 2:
                mirroring = Vertical;
                break;
            case 3:
                mirroring = Horizontal;
        }
        prgBankMode = (shiftRegister >> 2) & 3;
        chrBankMode = (shiftRegister >> 4) & 1;
    } else if (addr < chrBank1Start) {
        chrBank0 = shiftRegister & 0x1f;
        expandCHRMemory(chrBank0);
    } else if (addr < prgBankStart) {
        chrBank1 = shiftRegister & 0x1f;
        expandCHRMemory(chrBank1);
    } else {
        prgBank = shiftRegister & 0xf;
    }
    // Reset shift register to its default value
    shiftRegister = 0x10;
}

// Increases the CHR memory size if CHR-RAM is enabled and the game selects a CHR bank that has not
// yet been allocated

void MMC::expandCHRMemory(const unsigned int selectedCHRBank) {
    if (chrRAM && selectedCHRBank >= chrMemorySize) {
        chrMemory.resize((selectedCHRBank + 1) * 2);
        chrMemorySize = selectedCHRBank + 1;
    }
}