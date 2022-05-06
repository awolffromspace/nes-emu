#ifndef PPU_H
#define PPU_H

#pragma once
class MMC;

#include <cstdint>
#include <cstring>
#include <iostream>
#include <SDL.h>
#include <string.h>

#include "mmc.h"
#include "ppu-op.h"

class PPU {
    public:
        PPU();
        void clear(bool mute);
        void step(MMC& mmc, SDL_Renderer* renderer, SDL_Texture* texture,
            bool mute);
        uint8_t readIO(uint16_t addr, MMC& mmc, bool mute);
        void writeIO(uint16_t addr, uint8_t val, MMC& mmc, bool mute);
        void writeOAM(uint8_t addr, uint8_t val);
        bool isNMIActive(MMC& mmc, bool mute);
        void setMirroring(unsigned int mirroring);
        void print(bool isCycleDone, bool mute) const;

        enum Mirroring {
            Horizontal = 0,
            Vertical = 1,
            SingleScreen = 2,
            FourScreen = 3
        };

    private:
        uint8_t registers[8];
        uint8_t oamDMA;
        uint8_t oam[0x100];
        // uint8_t secondaryOAM[0x20];
        uint8_t vram[0x820];
        uint8_t frame[256 * 240 * 4];
        PPUOp op;
        struct Controller {
            uint16_t nametableBaseAddr;
            unsigned int ppuAddrInc;
            uint16_t spritePatternAddr;
            uint16_t bgPatternAddr;
            unsigned int spriteHeight;
            bool outputColorOnEXT;
            bool nmiAtVblank;
        };
        struct Mask {
            bool grayscale;
            bool showBGLeftCol;
            bool showSpriteLeftCol;
            bool showBG;
            bool showSprites;
            bool redEmphasis;
            bool greenEmphasis;
            bool blueEmphasis;
        };
        struct Status {
            bool spriteOverflow;
            bool spriteZeroHit;
            bool inVblank;
        };
        struct Controller ctrl;
        struct Mask mask;
        struct Status status;
        uint16_t ppuAddr;
        uint8_t ppuDataBuffer;
        bool writeLoAddr;
        unsigned int mirroring;
        unsigned int totalCycles;

        void fetch(MMC& mmc);
        void getPixel(MMC& mmc);
        void updateFlags(MMC& mmc, bool mute);
        void prepNextCycle(MMC& mmc);
        void updateNametableAddr(MMC& mmc);
        void updateAttributeAddr();
        void updateOpStatus();
        uint8_t readRegister(uint16_t addr, MMC& mmc);
        void writeRegister(uint16_t addr, uint8_t val, MMC& mmc, bool mute);
        uint8_t readVRAM(uint16_t addr, MMC& mmc) const;
        void writeVRAM(uint16_t addr, uint8_t val, MMC& mmc, bool mute);
        void updateCtrl(uint8_t val);
        void updateMask(uint8_t val);
        void updateStatus(uint8_t val, bool updatePrevWritten);
        void updatePPUADDR(uint8_t val, MMC& mmc, bool mute);
        uint16_t getHorizontalMirrorAddr(uint16_t addr) const;
        uint16_t getVerticalMirrorAddr(uint16_t addr) const;
        uint16_t getSingleScreenMirrorAddr(uint16_t addr) const;
        uint16_t getFourScreenMirrorAddr(uint16_t addr, MMC& mmc) const;
        unsigned int getRenderLine() const;
        uint8_t getColorBits() const;
        void setRGB();
};

#endif