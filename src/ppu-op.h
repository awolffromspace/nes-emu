#ifndef PPUOP_H
#define PPUOP_H

#include <cstdint>
#include <queue>
#include <vector>

class PPUOp {
    public:
        PPUOp();
        void clear();

    private:
        struct TileRow {
            uint8_t nametableEntry;
            uint8_t attributeEntry;
            uint8_t patternEntryLo;
            uint8_t patternEntryHi;
            unsigned int attributeQuadrant;
        };
        struct Sprite {
            uint8_t yPos;
            uint8_t tileIndexNum;
            uint8_t attributes;
            uint8_t xPos;
            uint8_t patternEntryLo;
            uint8_t patternEntryHi;
        };

        // Address of the nametable byte
        uint16_t nametableAddr;
        // Nametable byte that points to a 8x8 pattern table
        uint8_t nametableEntry;
        // Address of the attribute table byte
        uint16_t attributeAddr;
        // Attribute table byte that has bit 3 and 4 of 4-bit color for four 8x8
        // pattern tables
        uint8_t attributeEntry;
        // Row of a pattern table that has bit 0 of 4-bit color for 8x1 pixels
        uint8_t patternEntryLo;
        // Row of a pattern table that has bit 1 of 4-bit color for 8x1 pixels
        uint8_t patternEntryHi;
        std::queue<struct TileRow> tileRows;
        uint8_t oamEntry;
        unsigned int spriteNum;
        unsigned int oamEntryNum;
        std::vector<struct Sprite> currentSprites;
        std::vector<struct Sprite> nextSprites;
        // Current scanline out of 262 total scanlines (0 - 261)
        unsigned int scanline;
        // Current pixel out of 256 total pixels in the scanline (0 - 255)
        unsigned int pixel;
        // Current quadrant of the attribute table byte
        unsigned int attributeQuadrant;
        // If the current frame is odd (flips between true/false each frame)
        bool oddFrame;
        // How many frames the PPU has outputted
        unsigned int frameNum;
        // How many cycles the PPU has taken to render the current scanline
        // (max 340)
        unsigned int cycle;
        // If the first cycle was not skipped (in other words, delayed)
        bool delayedFirstCycle;
        // Indicates any cycle-relevant info about the operation
        // Depends on the enum Status
        unsigned int status;
        friend class PPU;

        enum QuadrantLocation {
            TopLeft = 0,
            TopRight = 1,
            BottomLeft = 2,
            BottomRight = 3
        };
        enum OpStatus {
            FetchNametableEntry = 0,
            FetchAttributeEntry = 1,
            FetchPatternEntryLo = 2,
            FetchPatternEntryHi = 3,
            FetchSpriteEntryLo = 4,
            FetchSpriteEntryHi = 5
        };
};

#endif