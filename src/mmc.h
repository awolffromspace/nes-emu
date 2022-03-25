#ifndef MMC_H
#define MMC_H

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

class MMC {
    public:
        MMC();
        void clear(bool mute);
        uint8_t read(uint16_t addr);
        void write(uint16_t addr, uint8_t val, bool mute);
        void readInInst(std::string& filename);
        void readInINES(std::string& filename);

    private:
        std::vector<uint8_t> data;
        // unsigned int ramSize;
        unsigned int romSize;
        // unsigned int ramBankSize;
        // unsigned int romBankSize;
};

#endif