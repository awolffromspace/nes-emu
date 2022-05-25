#ifndef PPUOP_H
#define PPUOP_H

#include <queue>
#include <vector>

#include "sprite.h"

// PPU Operation
// Holds info specific to the current operation (i.e., rendering the current pixel of the current
// scanline)

class PPUOp {
    public:
        PPUOp();
        void clear();

    private:
        // Required data for rendering a background tile row
        struct TileRow {
            uint8_t nametableEntry;
            uint8_t attributeEntry;
            uint8_t patternEntryLo;
            uint8_t patternEntryHi;
            unsigned int attributeQuadrant;
        };

        // Address of the nametable byte
        uint16_t nametableAddr;
        // Nametable byte that points to a 8x8 pattern table
        uint8_t nametableEntry;
        // Address of the attribute table byte
        uint16_t attributeAddr;
        // Attribute table byte that has bit 3 and 4 of 4-bit color for four 8x8 pattern tables
        uint8_t attributeEntry;
        // Row of a pattern table that has bit 0 of 4-bit color for 8x1 pixels
        uint8_t patternEntryLo;
        // Row of a pattern table that has bit 1 of 4-bit color for 8x1 pixels
        uint8_t patternEntryHi;
        // Queue that stores the next background tile rows to render. The PPU pre-fetches 2 tile
        // rows in advance before rendering. Additionally, rendering doesn't start until cycle 4
        // while fetching a third tile row, so this queue will only ever contain a max of 3 tile
        // rows at a time
        std::queue<struct TileRow> tileRows;
        // OAM byte that has info about the current sprite
        uint8_t oamEntry;
        // Current sprite number out of the 64 sprites in the OAM (0 - 63)
        unsigned int spriteNum;
        // Current OAM byte out of the 4 bytes of sprite info (0 - 3)
        unsigned int oamEntryNum;
        // Sprites that are to be rendered on the current scanline
        std::vector<Sprite> currentSprites;
        // Sprites that are to be rendered on the next scanline
        std::vector<Sprite> nextSprites;
        // Current scanline out of 262 total scanlines (0 - 261)
        unsigned int scanline;
        // Current pixel out of 256 total pixels in the scanline (0 - 255)
        unsigned int pixel;
        // Current quadrant of the attribute table byte
        unsigned int attributeQuadrant;
        // How many cycles the PPU has taken to render the current scanline (max 340)
        unsigned int cycle;
        // Indicates any cycle-relevant info about the operation. Depends on the enum Status
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