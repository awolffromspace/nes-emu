#include "memory.h"

Memory::Memory() {

}

void Memory::reset(bool mute) {
    memset(data, 0, 0x10000);
    write(0xfffd, 0x80, mute);

    if (!mute) {
        std::cout << "Memory was reset\n";
    }
}

uint8_t Memory::read(uint16_t addr) {
    if (addr < 0x2000) {
        addr &= 0x7ff;
    }
    return data[addr];
}

void Memory::write(uint16_t addr, uint8_t val, bool mute) {
    if (addr < 0x2000) {
        addr &= 0x7ff;
    }
    data[addr] = val;

    if (!mute) {
        std::cout << std::hex << "0x" << (unsigned int) val << " has " <<
            "been written to the address 0x" << (unsigned int) addr <<
            "\n--------------------------------------------------\n" <<
            std::dec;
    }
}

void Memory::push(uint8_t& pointer, uint8_t val, bool mute) {
    uint16_t addr = 0x100 + pointer;
    data[addr] = val;

    if (!mute) {
        std::cout << std::hex << "0x" << (unsigned int) val << " has " <<
            "been pushed to the stack address 0x" << (unsigned int) addr <<
            "\n--------------------------------------------------\n" <<
            std::dec;
    }

    --pointer;
}

uint8_t Memory::pull(uint8_t& pointer, bool mute) {
    ++pointer;
    uint16_t addr = 0x100 + pointer;
    uint8_t val = data[addr];

    if (!mute) {
        std::cout << std::hex << "0x" << (unsigned int) val << " has " <<
            "been pulled from the stack address 0x" << (unsigned int) addr <<
            "\n--------------------------------------------------\n" <<
            std::dec;
    }

    return val;
}

void Memory::readInInst(std::string& filename) {
    std::string line;
    std::ifstream file(filename.c_str());

    if (!file.is_open()) {
        std::cerr << "Error reading in file" << std::endl;
        exit(1);
    }

    int dataAddr = 0x8000;
    while (file.good()) {
        getline(file, line);
        std::string substring = "";

        for (int i = 0; i < line.size(); ++i) {
            if (line.at(i) == '/' || line.at(i) == ' ') {
                break;
            }

            substring += line.at(i);

            if (i % 2 == 1) {
                data[dataAddr] = std::stoul(substring, nullptr, 16);
                ++dataAddr;
                substring = "";
            }
        }
    }

    file.close();
}

void Memory::readInINES(std::string& filename) {
    std::ifstream file(filename.c_str(), std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Error reading in file" << std::endl;
        exit(1);
    }

    int dataIndex = 0x8000;
    for (int i = 0; i < 0x10; ++i) {
        file.read((char*) &data[dataIndex], 1);
    }

    for (int i = 0; i < 0x4000; ++i) {
        file.read((char*) &data[dataIndex], 1);
        data[dataIndex + 0x4000] = data[dataIndex];
        ++dataIndex;
    }
}