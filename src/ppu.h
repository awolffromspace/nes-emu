#ifndef PPU_H
#define PPU_H

#pragma once
class MMC;

#include <cstring>
#include <iostream>
#include <SDL.h>
#include <string.h>

#include "mmc.h"
#include "ppu-op.h"

#define ATTRIBUTE_OFFSET 0x3c0
#define BLACK 0xf
#define FRAME_HEIGHT 240
#define FRAME_SIZE 256 * 240 * 4
#define FRAME_WIDTH 256
#define IMAGE_PALETTE_START 0x3f00
#define NAMETABLE0_START 0x2000
#define NAMETABLE1_START 0x2400
#define NAMETABLE2_START 0x2800
#define NAMETABLE3_START 0x2c00
#define OAM_SIZE 0x100
#define OAMADDR 0x2003
#define OAMDATA 0x2004
#define OAMDMA 0x4014
#define PALETTE_SIZE 0x40
#define PPU_CYCLES_PER_FRAME 341 * 262
#define PPU_REGISTER_SIZE 8
#define PPUADDR 0x2006
#define PPUCTRL 0x2000
#define PPUDATA 0x2007
#define PPUMASK 0x2001
#define PPUSCROLL 0x2005
#define PPUSTATUS 0x2002
#define SECONDARY_OAM_SIZE 0x20
#define SPRITE_PALETTE_START 0x3f10
#define TILES_PER_ROW 0x20
#define UNIVERSAL_BG_INDEX 0x3f00 - 0xf00 - 0x400 - 0x400 - 0x2000
#define VRAM_SIZE 0x400 + 0x400 + 0x20

// Picture Processing Unit
// Handles anything related to graphics. Stores data for addresses $2000 - $2007 (registers), $2008
// - $3fff (mirrored addresses), and $4014 (OAMDMA) in the CPU memory map

class PPU {
    public:
        PPU();
        void clear();
        void step(MMC& mmc, SDL_Renderer* renderer, SDL_Texture* texture, bool mute);

        // Read/Write I/O Functions
        uint8_t readRegister(uint16_t addr, MMC& mmc);
        void writeRegister(uint16_t addr, uint8_t val, MMC& mmc, bool mute);
        void writeOAM(uint8_t addr, uint8_t val);

        // Miscellaneous Functions
        bool isNMIActive(MMC& mmc, bool mute);
        unsigned int getTotalCycles() const;
        void clearTotalCycles();
        void print(bool isCycleDone) const;

    private:
        struct RGBVal {
            uint8_t red;
            uint8_t green;
            uint8_t blue;
        };

        // Main registers that are exposed to the CPU. $2000 - $2007 in the CPU memory map
        // https://www.nesdev.org/wiki/PPU_registers
        uint8_t registers[PPU_REGISTER_SIZE];
        // Object Attribute Memory Direct Memory Access high-byte address. $4014 in the CPU memory
        // map. When the CPU writes to this register, the CPU starts an OAM DMA transfer that copies
        // data from $xx00 - $xxff to the PPU OAM where xx is the value of this register
        uint8_t oamDMA;
        uint16_t v;
        uint16_t t;
        uint8_t x;
        bool w;
        // Object Attribute Memory that contains data for up to 64 sprites. Each sprite is 4 bytes:
        // https://www.nesdev.org/wiki/PPU_OAM
        uint8_t oam[OAM_SIZE];
        // Secondary OAM that contains data for up to 8 sprites, which are the sprites that are
        // rendered for the current scanline
        uint8_t secondaryOAM[SECONDARY_OAM_SIZE];
        // Video RAM that includes nametables, attribute tables, and palettes. Makes up $2000 -
        // $ffff in the PPU memory map. Doesn't include the pattern tables in the MMC, which are
        // $0000 - $1fff
        uint8_t vram[VRAM_SIZE];
        // Frame that SDL displays to the screen. Each 4 bytes is a pixel. The first byte is the
        // blue value, the second is the green value, the third is the red value, and the fourth is
        // the opacity
        uint8_t frame[FRAME_SIZE];
        // Palette that contains the RGB values for displaying pixel colors
        struct RGBVal palette[PALETTE_SIZE];
        // Current operation that holds info about the current pixel and scanline being rendered
        PPUOp op;
        // PPUDATA read buffer:
        // https://www.nesdev.org/wiki/PPU_registers#The_PPUDATA_read_buffer_(post-fetch)
        uint8_t ppuDataBuffer;
        // Total number of cycles since initialization
        unsigned int totalCycles;

        // Cycle Skipping
        void skipCycle0();

        // Fetching
        void fetch(MMC& mmc);
        uint16_t getNametableAddr() const;
        uint16_t getNametableSelectAddr() const;
        void updateAttribute();
        void fetchSpriteEntry(MMC& mmc);

        // Sprite Computation
        void clearSecondaryOAM();
        void evaluateSprites();

        // Rendering
        void setPixel(MMC& mmc);
        uint8_t getPalette();
        uint8_t getUpperPalette(const struct TileRow& tileRow) const;
        void setSprite0Hit(const Sprite& sprite, uint8_t bgPalette);
        void setRGB(uint8_t paletteEntry);
        void renderFrame(SDL_Renderer* renderer, SDL_Texture* texture);

        // Scrolling
        void updateScroll();
        void incrementCoarseXScroll();
        void incrementYScroll();
        void switchHorizontalNametable();
        void switchVerticalNametable();
        void resetHorizontalNametable();

        // Update Flags
        void updatePPUStatus(MMC& mmc);

        // Read/Write Functions
        void writePPUScroll(uint8_t val);
        void writePPUAddr(uint8_t val);
        uint8_t readVRAM(uint16_t addr, MMC& mmc) const;
        void writeVRAM(uint16_t addr, uint8_t val, MMC& mmc, bool mute);

        // Mirrored Address Getters
        uint16_t getLocalRegisterAddr(uint16_t addr) const;
        uint16_t getLocalVRAMAddr(uint16_t addr, MMC& mmc, bool isRead) const;
        uint16_t getUpperMirrorAddr(uint16_t addr) const;
        uint16_t getHorizontalMirrorAddr(uint16_t addr) const;
        uint16_t getVerticalMirrorAddr(uint16_t addr) const;
        uint16_t getSingleScreenMirrorAddr(uint16_t addr) const;
        uint16_t getFourScreenMirrorAddr(uint16_t addr) const;

        // Register Flag Getters
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
        bool isRenderingEnabled() const;
        bool isRedEmphasized() const;
        bool isGreenEmphasized() const;
        bool isBlueEmphasized() const;
        bool isSpriteOverflow() const;
        bool isSpriteZeroHit() const;
        bool isVblank() const;
        unsigned int getCoarseXScroll() const;
        unsigned int getTempCoarseXScroll() const;
        unsigned int getCoarseYScroll() const;
        unsigned int getTempCoarseYScroll() const;
        unsigned int getNametableSelect() const;
        unsigned int getTempNametableSelect() const;
        unsigned int getFineYScroll() const;
        unsigned int getTempFineYScroll() const;

        // Register Flag Setters
        void setCoarseXScroll(unsigned int val);
        void setTempCoarseXScroll(unsigned int val);
        void setCoarseYScroll(unsigned int val);
        void setTempCoarseYScroll(unsigned int val);
        void setNametableSelect(unsigned int val);
        void setTempNametableSelect(unsigned int val);
        void setFineYScroll(unsigned int val);
        void setTempFineYScroll(unsigned int val);

        // Color Palette Initialization
        void initializePalette();
};

#endif