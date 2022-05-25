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

uint8_t Sprite::getTileRowIndex(unsigned int renderLine, unsigned int spriteHeight) const {
    uint8_t index = 0;
    unsigned int yDiff = getYDifference(renderLine);
    if (isFlippedVertically()) {
        index += 0x7;
        if (spriteHeight == 16) {
            index += 0x10;
        }
        if (yDiff >= 8) {
            index -= 0x10;
            yDiff -= 8;
        }
        index -= yDiff;
    } else {
        if (yDiff >= 8) {
            index += 0x10;
            yDiff -= 8;
        }
        index += yDiff;
    }
    return index;
}

uint8_t Sprite::getPixel(unsigned int pixelNum) const {
    unsigned int xDiff = getXDifference(pixelNum);
    uint8_t pixelBit = 0x80 >> xDiff;
    uint8_t upperPaletteBits = getPalette();
    uint8_t pixel = 0;
    if (isFlippedHorizontally()) {
        pixelBit = 1 << xDiff;
    }
    if (patternEntryLo & pixelBit) {
        pixel |= 1;
    }
    if (patternEntryHi & pixelBit) {
        pixel |= 2;
    }
    if (upperPaletteBits & 1) {
        pixel |= 4;
    }
    if (upperPaletteBits & 2) {
        pixel |= 8;
    }
    return pixel;
}

bool Sprite::isChosen(uint8_t bgPixel, uint8_t spritePixel) const {
    bool priority = isPrioritized();
    if (bgPixel && spritePixel && priority) {
        return true;
    } else if (!bgPixel && spritePixel) {
        return true;
    }
    return false;
}

unsigned int Sprite::getYDifference(unsigned int renderLine) const {
    return renderLine - yPos;
}

bool Sprite::isYInRange(unsigned int renderLine, unsigned int spriteHeight) const {
    unsigned int yDiff = getYDifference(renderLine);
    if (yDiff <= spriteHeight - 1) {
        return true;
    }
    return false;
}

unsigned int Sprite::getXDifference(unsigned int pixel) const {
    return pixel - xPos;
}

bool Sprite::isXInRange(unsigned int pixel) const {
    unsigned int xDiff = getXDifference(pixel);
    if (xDiff <= 7) {
        return true;
    }
    return false;
}

uint8_t Sprite::getPalette() const {
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