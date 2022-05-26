#include "ppu-op.h"

// Public Member Functions

PPUOp::PPUOp() :
        nametableAddr(0x2000),
        nametableEntry(0),
        attributeAddr(0x23c0),
        attributeEntry(0),
        patternEntryLo(0),
        patternEntryHi(0),
        oamEntry(0),
        spriteNum(0),
        oamEntryNum(0),
        scanline(261),
        pixel(0),
        attributeQuadrant(0),
        oddFrame(false),
        cycle(0),
        status(0) { }

void PPUOp::clear() {
    nametableAddr = 0x2000;
    nametableEntry = 0;
    attributeAddr = 0x23c0;
    attributeEntry = 0;
    patternEntryLo = 0;
    patternEntryHi = 0;

    while (!tileRows.empty()) {
        tileRows.pop();
    }

    oamEntry = 0;
    spriteNum = 0;
    oamEntryNum = 0;
    currentSprites.clear();
    nextSprites.clear();
    scanline = 261;
    pixel = 0;
    attributeQuadrant = 0;
    oddFrame = false;
    cycle = 0;
    status = 0;
}

// Pushes the tile row that has all of its data fetched to the queue for future rendering

void PPUOp::addTileRow() {
    tileRows.push({nametableEntry, attributeEntry, patternEntryLo, patternEntryHi,
        attributeQuadrant});
}

// Gets the background palette bits for the current pixel

uint8_t PPUOp::getPalette() const {
    const struct TileRow& tileRow = tileRows.front();
    uint8_t upperPaletteBits = getUpperPalette();
    uint8_t bgPalette = 0;
    // The current pixel number mod 8 represents the column in the tile row, which isolates the 8x1
    // tile row to a single 1x1 pixel
    if (tileRow.patternEntryLo & (0x80 >> (pixel % 8))) {
        bgPalette |= 1;
    }
    if (tileRow.patternEntryHi & (0x80 >> (pixel % 8))) {
        bgPalette |= 2;
    }
    if (upperPaletteBits & 1) {
        bgPalette |= 4;
    }
    if (upperPaletteBits & 2) {
        bgPalette |= 8;
    }
    return bgPalette;
}

// Gets the upper 2 bits of the background palette for the current pixel

uint8_t PPUOp::getUpperPalette() const {
    const struct TileRow& tileRow = tileRows.front();
    unsigned int shiftNum = 0;
    // Which 2 bits to use out of the 8 bits in the attribute entry are determined by what the
    // current quadrant is: https://www.nesdev.org/wiki/PPU_attribute_tables
    switch (tileRow.attributeQuadrant) {
        case TopRight:
            shiftNum = 2;
            break;
        case BottomLeft:
            shiftNum = 4;
            break;
        case BottomRight:
            shiftNum = 6;
    }
    return (tileRow.attributeEntry >> shiftNum) & 3;
}

// Prepares anything else that needs to be updated for the next cycle

void PPUOp::prepNextCycle(uint8_t& ppuStatus) {
    if (canFetch()) {
        if (isFetchingBG()) {
            updateNametableAddr();
            updateAttributeAddr();
        }
        updateStatus();
    }

    if (isRendering()) {
        ++pixel;
        if (pixel % 8 == 0) {
            tileRows.pop();
        }
        if (pixel == TOTAL_PIXELS_PER_SCANLINE) {
            pixel = 0;
        }
    }

    if (cycle == 256) {
        spriteNum = 0;
        oamEntryNum = 0;
    } else if (cycle == 320) {
        spriteNum = 0;
        currentSprites = nextSprites;
        nextSprites.clear();
    }

    if (scanline == PRERENDER_LINE && cycle == LAST_CYCLE) {
        ppuStatus &= 0xbf;
        oddFrame = !oddFrame;
        scanline = 0;
        cycle = 0;
    } else if (cycle == LAST_CYCLE) {
        oddFrame = !oddFrame;
        ++scanline;
        cycle = 0;
    } else {
        ++cycle;
    }
}

void PPUOp::updateNametableAddr() {
    if (status == PPUOp::FetchPatternEntryHi) {
        ++nametableAddr;

        unsigned int renderLine = getRenderLine();
        if (cycle == 240 && (renderLine + 1) % 8 != 0) {
            nametableAddr -= 32;
        }
    }

    if (nametableAddr == ATTRIBUTE0_START) {
        nametableAddr = NAMETABLE0_START;
    }
}

void PPUOp::updateAttributeAddr() {
    if (status != PPUOp::FetchPatternEntryHi || nametableAddr % 2) {
        return;
    }

    unsigned int renderLine = getRenderLine();
    switch (attributeQuadrant) {
        case PPUOp::TopLeft:
            attributeQuadrant = PPUOp::TopRight;
            break;
        case PPUOp::TopRight:
            if (nametableAddr % 0x40 == 0 && (renderLine + 1) % 16 == 0) {
                attributeQuadrant = PPUOp::BottomLeft;
                attributeAddr &= 0xfff8;
            } else if (nametableAddr % 0x20 == 0) {
                attributeQuadrant = PPUOp::TopLeft;
                attributeAddr &= 0xfff8;
            } else {
                attributeQuadrant = PPUOp::TopLeft;
                ++attributeAddr;
            }
            break;
        case PPUOp::BottomLeft:
            attributeQuadrant = PPUOp::BottomRight;
            break;
        case PPUOp::BottomRight:
            if (nametableAddr == NAMETABLE0_START) {
                attributeQuadrant = PPUOp::TopLeft;
                attributeAddr = ATTRIBUTE0_START;
            } else if (nametableAddr % 0x80 == 0 && (renderLine + 1) % 32 == 0) {
                attributeQuadrant = PPUOp::TopLeft;
                ++attributeAddr;
            } else if (nametableAddr % 0x20 == 0) {
                attributeQuadrant = PPUOp::BottomLeft;
                attributeAddr &= 0xfff8;
            } else {
                attributeQuadrant = PPUOp::BottomLeft;
                ++attributeAddr;
            }
    }
}

void PPUOp::updateStatus() {
    if (status == PPUOp::FetchAttributeEntry) {
        if (isFetchingBG()) {
            status = PPUOp::FetchPatternEntryLo;
        } else {
            status = PPUOp::FetchSpriteEntryLo;
        }
    } else if (status == PPUOp::FetchPatternEntryHi || status == PPUOp::FetchSpriteEntryHi) {
        status = PPUOp::FetchNametableEntry;
    } else {
        ++status;
    }
}

unsigned int PPUOp::getRenderLine() const {
    if (cycle <= 320) {
        return scanline;
    }
    if (scanline == PRERENDER_LINE) {
        return 0;
    } else {
        return scanline + 1;
    }
}

bool PPUOp::isRendering() const {
    if (scanline <= LAST_RENDER_LINE && cycle >= FIRST_CYCLE_TO_OUTPUT_PIXEL &&
            cycle <= LAST_CYCLE_TO_OUTPUT_PIXEL) {
        return true;
    }
    return false;
}

bool PPUOp::canFetch() const {
    if (cycle % 2 || cycle == 0) {
        return false;
    }
    if (scanline < LAST_RENDER_LINE && (cycle < 241 || cycle > 256) && cycle < 337) {
        return true;
    }
    if (scanline == LAST_RENDER_LINE && cycle < 241) {
        return true;
    }
    if (scanline == PRERENDER_LINE && cycle > 320 && cycle < 337) {
        return true;
    }
    return false;
}

bool PPUOp::isFetchingBG() const {
    if (cycle < 257 || cycle > 320) {
        return true;
    }
    return false;
}