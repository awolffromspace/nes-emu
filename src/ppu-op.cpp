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
    cycle = 0;
    status = 0;
}