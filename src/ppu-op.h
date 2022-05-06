#ifndef PPUOP_H
#define PPUOP_H

#include <cstdint>

class PPUOp {
    public:
        PPUOp();
        void clear();

        enum OpStatus {
            FetchNametableEntry = 0,
            FetchAttributeEntry = 1,
            FetchPatternEntryLo = 2,
            FetchPatternEntryHi = 3,
            FetchSpriteEntryLo = 4,
            FetchSpriteEntryHi = 5
        };

        enum QuadrantLocation {
            TopLeft = 0,
            TopRight = 1,
            BottomLeft = 2,
            BottomRight = 3
        };

    private:
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
        uint8_t spriteEntryLo;
        uint8_t spriteEntryHi;
        uint8_t paletteEntry;
        // Current scanline out of 262 total scanlines (0 - 261)
        unsigned int scanline;
        // Current pixel out of 256 total pixels in the scanline (0 - 255)
        unsigned int pixel;
        // Current quadrant of the attribute table byte
        unsigned int attributeQuadrant;
        bool oddFrame;
        bool changedFrame;
        // How many cycles the PPU has taken to render the current scanline
        // (max 340)
        unsigned int cycle;
        bool delayedFirstCycle;
        // Indicates any cycle-relevant info about the operation
        // Depends on the enum Status
        unsigned int status;
        friend class PPU;
};

#endif