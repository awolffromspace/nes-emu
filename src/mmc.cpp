#include "mmc.h"

MMC::MMC() : data(0xbfe0, 0), romSize(0x4000) {
    // Set the reset vector to 0x8000, the beginning of the PRG-ROM lower bank
    // This is where test programs will start at
    write(0xfffd, 0x80, true);
}

void MMC::clear(bool mute) {
    for (unsigned int i = 0; i < data.size(); ++i) {
        data[i] = 0x0;
    }

    write(0xfffd, 0x80, mute);

    if (!mute) {
        std::cout << "MMC was cleared\n";
    }
}

uint8_t MMC::read(uint16_t addr) {
    addr = (addr & 0xbfff) - 0x4020;
    return data[addr];
}

void MMC::write(uint16_t addr, uint8_t val, bool mute) {
    uint16_t localAddr = (addr & 0xbfff) - 0x4020;
    data[localAddr] = val;

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

    unsigned int addr = 0x8000 - 0x4020;
    while (file.good()) {
        getline(file, line);
        std::string substring = "";

        for (unsigned int i = 0; i < line.size(); ++i) {
            if (line.at(i) == '/' || line.at(i) == ' ') {
                break;
            }

            substring += line.at(i);

            if (i % 2 == 1) {
                data[addr] = std::stoul(substring, nullptr, 16);
                ++addr;
                substring = "";
            }
        }
    }

    file.close();
}

void MMC::readInINES(std::string& filename) {
    std::ifstream file(filename.c_str(), std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Error reading in file\n";
        exit(1);
    }

    unsigned int addr = 0x8000 - 0x4020;
    for (unsigned int i = 0; i < 0x10; ++i) {
        file.read((char*) &data[addr], 1);
    }

    for (unsigned int i = 0; i < romSize; ++i) {
        file.read((char*) &data[addr], 1);
        ++addr;
    }
}