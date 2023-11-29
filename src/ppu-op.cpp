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
        nmiOccurred(false),
        suppressNMI(false),
        forceNMI(false),
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
    nmiOccurred = false;
    suppressNMI = false;
    forceNMI = false;
    cycle = 0;
    status = 0;
}

// Once a tile row has all of its data fetched, this function is called to push it to the queue for
// future rendering

void PPUOp::addTileRow() {
    tileRows.push_back({nametableEntry, attributeEntry, patternEntryLo, patternEntryHi,
        attributeQuadrant});
}

// Gets the background palette bits for the current pixel

uint8_t PPUOp::getPalette(const uint8_t x) {
    std::deque<TileRow>::iterator tileRowIterator = tileRows.begin();
    const unsigned int tileRowSize = 8;
    // If this condition is true, then the current pixel and fine X scroll is past the current tile,
    // so the next tile row should be used instead
    if (pixel % tileRowSize + x > tileRowSize - 1) {
        ++tileRowIterator;
    }
    const uint8_t upperPaletteBits = getUpperPalette(*tileRowIterator);
    uint8_t bgPalette = 0;
    // Get the bit in the byte that represents the current pixel
    const uint8_t pixelInTileRow = 0x80 >> ((pixel + x) % tileRowSize);
    if (tileRowIterator->patternEntryLo & pixelInTileRow) {
        bgPalette |= 1;
    }
    if (tileRowIterator->patternEntryHi & pixelInTileRow) {
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

// Prepares anything else that needs to be updated for the next cycle

void PPUOp::prepNextCycle() {
    const unsigned int prerenderLine = 261;
    // Only update the operation status if the current cycle is associated with a valid fetch and
    // not an unused fetch
    if (canFetch()) {
        updateStatus();
    }

    if (isRendering()) {
        ++pixel;
        const unsigned int tileRowSize = 8;
        // Every tile is 8x8, so pop the queue every 8 pixels
        if (pixel % tileRowSize == 0) {
            tileRows.pop_front();
        }
        const unsigned int pixelsPerScanline = 256;
        // Ensure that the pixel number wraparounds back to 0 after outputting the last pixel
        if (pixel == pixelsPerScanline) {
            pixel = 0;
        }
    }

    // Clear the force NMI flag before an NMI gets triggered too late
    if (cycle == 3 && scanline == prerenderLine) {
        forceNMI = false;
    // If the cycle is 256, then sprite evaluation is done and spriteNum can be reset and used for
    // sprite fetching next. oamEntryNum is reset for the next scanline's sprite evaluation
    } else if (cycle == 256) {
        spriteNum = 0;
        oamEntryNum = 0;
    // If the cycle is 320, then sprite fetching for the next scanline and rendering the current
    // scanline are both done. These sprites for the next scanline can now be treated as the
    // currentSprites to render
    } else if (cycle == 320) {
        spriteNum = 0;
        currentSprites = nextSprites;
        nextSprites.clear();
        const unsigned int lastRenderLine = 239;
        // After rendering the entire scanline, there is one tile row remaining in the queue that is
        // past the right border of the frame, which is accessed by prior rendering functions when
        // outputting one of the last 8 pixels and the fine X scroll is large enough. This ensures
        // that the queue is empty for the next scanline
        if (scanline <= lastRenderLine && !tileRows.empty()) {
            tileRows.clear();
        }
    }

    const unsigned int lastCycle = 340;
    if (cycle == lastCycle && scanline == prerenderLine) {
        // Update any relevant fields for the next frame
        scanline = 0;
        oddFrame = !oddFrame;
        nmiOccurred = false;
        suppressNMI = false;
        cycle = 0;
    } else if (cycle == lastCycle) {
        // Update fields for the next scanline
        ++scanline;
        cycle = 0;
    } else {
        ++cycle;
    }
}

// Updates the operation status. Since memory access is the bottleneck, the status is just the
// current fetch. Fetches follow the pattern of: nametable -> attribute -> pattern lo -> pattern hi
// where the two pattern fetches are different depending on if it's a background or sprite fetch:
// https://www.nesdev.org/wiki/PPU_rendering#Line-by-line_timing

void PPUOp::updateStatus() {
    if (status == FetchAttributeEntry) {
        if (isFetchingBG()) {
            status = FetchPatternEntryLo;
        } else {
            status = FetchSpriteEntryLo;
        }
    } else if (status == FetchPatternEntryHi || status == FetchSpriteEntryHi) {
        status = FetchNametableEntry;
    } else {
        ++status;
    }
}

// Tells the PPU to output a pixel on the current cycle

bool PPUOp::isRendering() const {
    const unsigned int lastRenderLine = 239;
    const unsigned int firstCycle = 4;
    const unsigned int lastCycle = firstCycle + 255;
    if (scanline <= lastRenderLine && cycle >= firstCycle && cycle <= lastCycle) {
        return true;
    }
    return false;
}

// Determines whether the cycle is associated with a valid fetch and not an unused fetch

bool PPUOp::canFetch() const {
    const unsigned int lastRenderLine = 239;
    // Each fetch takes 2 cycles, so only the second cycle of each fetch is where the fetch is
    // actually performed
    if (cycle % 2 || cycle == 0) {
        return false;
    }
    // Cycles 249 - 256 are an unused tile fetch
    if (scanline < lastRenderLine && (cycle <= 248 || cycle >= 257) && cycle <= 336) {
        return true;
    }
    // Cycles 257 - 336 are for the next scanline, which are irrelevant for the last render line
    if (scanline == lastRenderLine && cycle <= 248) {
        return true;
    }
    const unsigned int prerenderLine = 261;
    // While cycles 257 - 320 are for the next scanline (that would be scanline 0 here), these are
    // sprite fetches, which are not shown on the first scanline. Only cycles 321 - 336 are
    // relevant, which are where the first two background tile rows are fetched for the next
    // scanline
    if (scanline == prerenderLine && cycle >= 321 && cycle <= 336) {
        return true;
    }
    return false;
}

// Determines whether the cycle is associated with a background or sprite fetch

bool PPUOp::isFetchingBG() const {
    if (cycle <= 256 || cycle >= 321) {
        return true;
    }
    return false;
}