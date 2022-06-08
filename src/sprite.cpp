#include "sprite.h"

// Public Member Functions

Sprite::Sprite() :
        yPos(0),
        tileIndexNum(0),
        attributes(0),
        xPos(0),
        patternEntryLo(0),
        patternEntryHi(0),
        spriteNum(0) { }

Sprite::Sprite(uint8_t yPos, unsigned int spriteNum) :
        tileIndexNum(0),
        attributes(0),
        xPos(0),
        patternEntryLo(0),
        patternEntryHi(0) {
    this->yPos = yPos;
    this->spriteNum = spriteNum;
}

// Gets the tile row index for the given scanline, which can be treated as the current y-coordinate
// that is being rendered: https://www.nesdev.org/wiki/PPU_OAM#Byte_1

uint8_t Sprite::getTileRowIndex(unsigned int scanline, unsigned int spriteHeight) const {
    uint8_t index = 0;
    unsigned int yDiff = getYDifference(scanline);
    if (isFlippedVertically()) {
        // Add 7 to get the bottommost tile row with the intention of working from bottom to top,
        // because the sprite is flipped vertically
        index += 7;
        if (spriteHeight == 16) {
            // Add 0x10 to get the second, lower tile, because the sprite is 8x16 and is composed of
            // two tiles
            index += 0x10;
        }
        // If the y-difference (i.e., the difference between the scanline and the sprite's
        // y-coordinate) is greater than or equal to 8, then the scanline is past the lower tile and
        // in the higher tile (working from bottom to top)
        if (yDiff >= 8) {
            // Subtract 0x10 to go back to the first, higher tile
            index -= 0x10;
            // Subtract by the tile height (8 pixels) to get the remaining y-difference
            yDiff -= 8;
        }
        // Shift the index up however many times the y-difference is, which will then be the row
        // that the scanline lands on
        index -= yDiff;
    } else {
        // If the y-difference (i.e., the difference between the scanline and the sprite's
        // y-coordinate) is greater than or equal to 8, then the scanline is past the higher tile
        // and in the lower tile (working from top to bottom). This also means that the sprite is
        // 8x16
        if (yDiff >= 8) {
            // Add 0x10 to get the second, lower tile, because the sprite is 8x16 and is composed of
            // two tiles
            index += 0x10;
            // Subtract by the tile height (8 pixels) to get the remaining y-difference
            yDiff -= 8;
        }
        // Shift the index down however many times the y-difference is, which will then be the row
        // that the scanline lands on
        index += yDiff;
    }
    return index;
}

// Gets the palette for the given pixel, which can be treated as the current x-coordinate that is
// being rendered

uint8_t Sprite::getPalette(unsigned int pixel) const {
    unsigned int xDiff = getXDifference(pixel);
    // Gets the furthest left bit in the tile and shifts it right however many times the
    // x-difference is (i.e., the difference between the pixel and the sprite's x-coordinate), which
    // will then be the column/bit that the pixel lands on
    uint8_t pixelBit = 0x80 >> xDiff;
    uint8_t upperPaletteBits = getUpperPalette();
    uint8_t palette = 0;
    // Perform the above operation in reverse if the sprite is flipped horizontally
    if (isFlippedHorizontally()) {
        pixelBit = 1 << xDiff;
    }
    if (patternEntryLo & pixelBit) {
        palette |= 1;
    }
    if (patternEntryHi & pixelBit) {
        palette |= 2;
    }
    if (upperPaletteBits & 1) {
        palette |= 4;
    }
    if (upperPaletteBits & 2) {
        palette |= 8;
    }
    return palette;
}

// Determines whether to "choose" the sprite over the background. The conditions are based on this
// priority multiplexer decision table: https://www.nesdev.org/wiki/PPU_rendering#Preface. Only the
// lower 2 bits of color are relevant

bool Sprite::isChosen(uint8_t bgPalette, uint8_t spritePalette) const {
    bool priority = isPrioritized();
    if (bgPalette && spritePalette && priority) {
        return true;
    } else if (!bgPalette && spritePalette) {
        return true;
    }
    return false;
}

// Gets the difference between the given scanline and the y-position of the sprite's top edge

unsigned int Sprite::getYDifference(unsigned int scanline) const {
    return scanline - yPos;
}

// Determines whether the sprite is in range of the given scanline (i.e., if any row of the sprite's
// tile can be outputted on the scanline)

bool Sprite::isYInRange(unsigned int scanline, unsigned int spriteHeight) const {
    unsigned int yDiff = getYDifference(scanline);
    if (yDiff <= spriteHeight - 1) {
        return true;
    }
    return false;
}

// Gets the difference between the given pixel and the x-position of the sprite's left edge

unsigned int Sprite::getXDifference(unsigned int pixel) const {
    return pixel - xPos;
}

// Determines whether the sprite is in range of the given pixel (i.e., if any column of the sprite's
// tile can be outputted on the scanline)

bool Sprite::isXInRange(unsigned int pixel) const {
    unsigned int xDiff = getXDifference(pixel);
    if (xDiff <= SPRITE_WIDTH - 1) {
        return true;
    }
    return false;
}

uint8_t Sprite::getUpperPalette() const {
    return attributes & 3;
}

bool Sprite::isPrioritized() const {
    return !(attributes & 0x20);
}

bool Sprite::isFlippedHorizontally() const {
    return attributes & 0x40;
}

bool Sprite::isFlippedVertically() const {
    return attributes & 0x80;
}