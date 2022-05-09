#include "ppu-op.h"

PPUOp::PPUOp() :
        nametableAddr(0x2000),
        nametableEntry(0),
        attributeAddr(0x23c0),
        attributeEntry(0),
        patternEntryLo(0),
        patternEntryHi(0),
        spriteEntryLo(0),
        spriteEntryHi(0),
        paletteEntry(0),
        scanline(261),
        pixel(0),
        attributeQuadrant(0),
        oddFrame(false),
        frameNum(0),
        cycle(1),
        delayedFirstCycle(false),
        status(0) {
}

void PPUOp::clear() {
    nametableAddr = 0x2000;
    nametableEntry = 0;
    attributeAddr = 0x23c0;
    attributeEntry = 0;
    patternEntryLo = 0;
    patternEntryHi = 0;
    spriteEntryLo = 0;
    spriteEntryHi = 0;
    paletteEntry = 0;
    scanline = 261;
    pixel = 0;
    attributeQuadrant = 0;
    oddFrame = false;
    frameNum = 0;
    cycle = 1;
    delayedFirstCycle = false;
    status = 0;
}