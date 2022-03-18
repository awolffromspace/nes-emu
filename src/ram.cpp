#include "ram.h"

RAM::RAM() {

}

void RAM::reset(bool mute) {
    memset(data, 0, 65536);
    write(0xfffd, 0x80, mute);

    if (!mute) {
        std::cout << "RAM was reset\n";
    }
}

uint8_t RAM::read(uint16_t addr) {
    return data[addr];
}

void RAM::write(uint16_t addr, uint8_t val, bool mute) {
    data[addr] = val;

    if (!mute) {
        std::cout << std::hex << "0x" << (unsigned int) val <<
            " has been written to RAM address(es)\n0x" << (unsigned int) addr;
    }

    if (addr < 0x2000) { // Zero Page mirroring
        for (int i = 1; i <= 3; ++i) {
            uint16_t index = addr + 0x800 * i;
            if (index >= 0x2000) {
                index -= 0x2000;
            }
            data[index] = val;

            if (!mute) {
                std::cout << ", 0x" << (unsigned int) index;
            }
        }
    } else if (addr < 0x4000) { // I/O register mirroring
        for (int i = 1; i <= 400; ++i) {
            uint16_t index = addr + 0x8 * i;
            if (index >= 0x4000) {
                index -= 0x2000;
            }
            data[index] = val;
            // Not printing I/O register mirroring due to the large number of
            // addresses
        }
    }

    if (!mute) {
        std::cout << "\n--------------------------------------------------\n" <<
            std::dec;
    }
}

void RAM::push(uint8_t& pointer, uint8_t val, bool mute) {
    uint16_t index = 0x100 + pointer;
    data[index] = val;

    if (!mute) {
        std::cout << std::hex << "0x" << (unsigned int) val << " has " <<
            "been pushed to the stack address 0x" << (unsigned int) index <<
            "\n--------------------------------------------------\n" <<
            std::dec;
    }

    --pointer;
}

uint8_t RAM::pull(uint8_t& pointer, bool mute) {
    ++pointer;
    uint16_t index = 0x100 + pointer;
    uint8_t val = data[index];

    if (!mute) {
        std::cout << std::hex << "0x" << (unsigned int) val << " has " <<
            "been pulled from the stack address 0x" << (unsigned int) index <<
            "\n--------------------------------------------------\n" <<
            std::dec;
    }

    return val;
}

void RAM::readInInst(std::string& filename) {
    std::string line;
    std::ifstream file(filename.c_str());

    if (!file.is_open()) {
        std::cerr << "Error reading in file" << std::endl;
        exit(1);
    }

    int dataIndex = 0x8000;
    while (file.good()) {
        getline(file, line);
        std::string substring = "";

        for (int i = 0; i < line.size(); ++i) {
            if (line.at(i) == '/' || line.at(i) == ' ') {
                break;
            }

            substring += line.at(i);

            if (i % 2 == 1) {
                data[dataIndex] = std::stoul(substring, nullptr, 16);
                ++dataIndex;
                substring = "";
            }
        }
    }

    file.close();
}