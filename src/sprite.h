#ifndef SPRITE_H
#define SPRITE_H

#include <cstdint>

class Sprite {
    public:
        Sprite();
        Sprite(uint8_t yPos, unsigned int spriteNum);

        // Fetching
        uint8_t getTileRowIndex(unsigned int renderLine, unsigned int spriteHeight) const;

        // Rendering
        uint8_t getPixel(unsigned int pixelNum) const;
        bool isChosen(uint8_t bgPixel, uint8_t spritePixel) const;

        // Locating the Sprite in the Frame
        unsigned int getYDifference(unsigned int renderLine) const;
        bool isYInRange(unsigned int renderLine, unsigned int spriteHeight) const;
        unsigned int getXDifference(unsigned int pixel) const;
        bool isXInRange(unsigned int pixel) const;

        // Attribute Getters
        uint8_t getPalette() const;
        bool isPrioritized() const;
        bool isFlippedHorizontally() const;
        bool isFlippedVertically() const;

    private:
        uint8_t yPos;
        uint8_t tileIndexNum;
        uint8_t attributes;
        uint8_t xPos;
        uint8_t patternEntryLo;
        uint8_t patternEntryHi;
        unsigned int spriteNum;

        friend class PPU;
        friend class PPUOp;
};

#endif