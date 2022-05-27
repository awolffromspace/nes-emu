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
    tileRows.clear();
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
    tileRows.push_back({nametableEntry, attributeEntry, patternEntryLo, patternEntryHi,
        attributeQuadrant});
}

// Gets the background palette bits for the current pixel

uint8_t PPUOp::getPalette(uint8_t x) {
    std::deque<TileRow>::iterator tileRowIterator = tileRows.begin();
    if (pixel % 8 + x > 7) {
        ++tileRowIterator;
    }
    uint8_t upperPaletteBits = getUpperPalette(*tileRowIterator);
    uint8_t bgPalette = 0;
    if (tileRowIterator->patternEntryLo & (0x80 >> ((pixel + x) % 8))) {
        bgPalette |= 1;
    }
    if (tileRowIterator->patternEntryHi & (0x80 >> ((pixel + x) % 8))) {
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

uint8_t PPUOp::getUpperPalette(const struct TileRow& tileRow) const {
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
    if (scanline < LAST_RENDER_LINE && (cycle < 249 || cycle > 256) && cycle < 337) {
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