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

#define PPU_REGISTER_SIZE 8
#define PPUCTRL 0x2000
#define PPUMASK 0x2001
#define PPUSTATUS 0x2002
#define OAMADDR 0x2003
#define OAMDATA 0x2004
#define PPUSCROLL 0x2005
#define PPUADDR 0x2006
#define PPUDATA 0x2007
#define OAMDMA 0x4014
#define OAM_SIZE 0x100
#define SECONDARY_OAM_SIZE 0x20
#define VRAM_SIZE 0x400 + 0x400 + 0x20
#define FRAME_SIZE 256 * 240 * 4
#define FRAME_WIDTH 256
#define FRAME_HEIGHT 240
#define PALETTE_SIZE 0x40
#define PALETTE_START 0x3f00
#define NAMETABLE0_START 0x2000
#define NAMETABLE1_START 0x2400
#define NAMETABLE2_START 0x2800
#define NAMETABLE3_START 0x2c00
#define ATTRIBUTE0_START 0x23c0
#define UNIVERSAL_BG_INDEX 0x3f00 - 0xf00 - 0x400 - 0x400 - 0x2000
#define BLACK 0xf
#define LAST_RENDER_LINE 239
#define PRERENDER_LINE 261

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
        uint8_t registers[PPU_REGISTER_SIZE];
        uint8_t oamDMA;
        uint8_t oam[OAM_SIZE];
        uint8_t secondaryOAM[SECONDARY_OAM_SIZE];
        uint8_t vram[VRAM_SIZE];
        uint8_t frame[FRAME_SIZE];
        struct RGBVal {
            uint8_t red;
            uint8_t green;
            uint8_t blue;
        };
        struct RGBVal palette[PALETTE_SIZE];
        PPUOp op;
        uint16_t ppuAddr;
        uint8_t ppuDataBuffer;
        bool writeLoAddr;
        unsigned int mirroring;
        unsigned int totalCycles;

        void fetch(MMC& mmc);
        void addTileRow();
        void setPixel(MMC& mmc);
        void setRGB(uint8_t paletteEntry);
        void updateFlags(MMC& mmc, bool mute);
        void prepNextCycle(MMC& mmc);
        void updateNametableAddr(MMC& mmc);
        void updateAttributeAddr();
        void updateOpStatus();
        uint8_t readRegister(uint16_t addr, MMC& mmc);
        void writeRegister(uint16_t addr, uint8_t val, MMC& mmc, bool mute);
        uint8_t readVRAM(uint16_t addr, MMC& mmc) const;
        void writeVRAM(uint16_t addr, uint8_t val, MMC& mmc, bool mute);
        void updatePPUAddr(uint8_t val, MMC& mmc, bool mute);
        uint16_t getHorizontalMirrorAddr(uint16_t addr) const;
        uint16_t getVerticalMirrorAddr(uint16_t addr) const;
        uint16_t getSingleScreenMirrorAddr(uint16_t addr) const;
        uint16_t getFourScreenMirrorAddr(uint16_t addr, MMC& mmc) const;
        unsigned int getRenderLine() const;
        bool isRendering() const;
        bool isValidFetch() const;
        uint8_t getPaletteFromAttribute() const;
        uint16_t getNametableBaseAddr() const;
        unsigned int getPPUAddrInc() const;
        uint16_t getSpritePatternAddr() const;
        uint16_t getBGPatternAddr() const;
        unsigned int getSpriteHeight() const;
        bool isColorOutputOnEXT() const;
        bool isNMIEnabled() const;
        bool isGrayscale() const;
        bool isBGLeftColShown() const;
        bool areSpritesLeftColShown() const;
        bool isBGShown() const;
        bool areSpritesShown() const;
        bool isRedEmphasized() const;
        bool isGreenEmphasized() const;
        bool isBlueEmphasized() const;
        bool isSpriteOverflow() const;
        bool isSpriteZeroHit() const;
        bool isVblank() const;
        void initializePalette();
};

#endif