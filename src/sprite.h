#ifndef SPRITE_H
#define SPRITE_H

#include <cstdint>

class Sprite {
    public:
        Sprite();
        Sprite(const uint8_t yPos, const unsigned int spriteNum);

    private:
        // Y-position of the sprite's top edge: https://www.nesdev.org/wiki/PPU_OAM#Byte_0. Since
        // sprite evaluation and fetching aren't performed in advance for scanline 0, sprites aren't
        // shown on scanline 0, and this y-position is shifted down by 1. E.g., no sprites on
        // scanline 0, y-position of 0 on scanline 1, y-position of 1 on scanline 2, etc.
        uint8_t yPos;
        // Tile index number of the sprite, including which CHR bank to get the pattern entries from
        // (not included if the sprite is 8x16): https://www.nesdev.org/wiki/PPU_OAM#Byte_1
        uint8_t tileIndexNum;
        // Sprite attributes, including the upper 2 bits of color, if the sprite should be
        // prioritized over the background, and if the sprite should be flipped horizontally or
        // vertically: https://www.nesdev.org/wiki/PPU_OAM#Byte_2
        uint8_t attributes;
        // X-position of the sprite's left edge: https://www.nesdev.org/wiki/PPU_OAM#Byte_2
        uint8_t xPos;
        // Row of a pattern table that has bit 0 of 4-bit color for 8x1 pixels of the sprite
        uint8_t patternEntryLo;
        // Row of a pattern table that has bit 1 of 4-bit color for 8x1 pixels of the sprite
        uint8_t patternEntryHi;
        // The index number of the sprite in the PPU OAM. E.g., the first 4 bytes in the OAM is
        // sprite 0, the next 4 bytes is sprite 1, etc. This is mainly used to identify sprite 0 for
        // sprite 0 hit
        unsigned int spriteNum;

        // Fetching
        uint8_t getTileRowIndex(const unsigned int scanline, const unsigned int spriteHeight) const;

        // Rendering
        uint8_t getPalette(const unsigned int pixel) const;
        bool isChosen(const uint8_t bgPalette, const uint8_t spritePalette) const;

        // Locating the Sprite in the Frame
        unsigned int getYDifference(const unsigned int scanline) const;
        bool isYInRange(const unsigned int scanline, const unsigned int spriteHeight) const;
        unsigned int getXDifference(const unsigned int pixel) const;
        bool isXInRange(const unsigned int pixel) const;

        // Attribute Getters
        uint8_t getUpperPalette() const;
        bool isPrioritized() const;
        bool isFlippedHorizontally() const;
        bool isFlippedVertically() const;

        friend class PPU;
        friend class PPUOp;
};

#endif