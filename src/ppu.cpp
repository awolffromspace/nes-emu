#include "ppu.h"

// Public Member Functions

PPU::PPU() :
        oamDMA(0),
        v(0),
        t(0),
        x(0),
        w(false),
        ppuDataBuffer(0),
        totalCycles(0) {
    memset(registers, 0, 8);
    const uint16_t universalBGColorAddr = 0x3f00;
    const uint16_t nametableMirrorSize = 0xf00;
    const uint16_t nametableSize = 0x3c0;
    const uint16_t attributeTableSize = 0x40;
    const uint16_t patternTableSize = 0x1000;
    const uint16_t universalBGColorLocalAddr = universalBGColorAddr - nametableMirrorSize -
        (nametableSize + attributeTableSize) * 2 - patternTableSize * 2;
    const uint8_t black = 0xf;
    vram[universalBGColorLocalAddr] = black;
    initializePalette();
}

void PPU::clear() {
    memset(registers, 0, 8);
    oamDMA = 0;
    v = 0;
    t = 0;
    x = 0;
    w = false;
    memset(oam, 0, 0x100);
    memset(secondaryOAM, 0xff, 0x20);
    const uint16_t nametableSize = 0x3c0;
    const uint16_t attributeTableSize = 0x40;
    const uint16_t paletteSize = 0x20;
    memset(vram, 0, (nametableSize + attributeTableSize) * 2 + paletteSize);
    const uint16_t universalBGColorAddr = 0x3f00;
    const uint16_t nametableMirrorSize = 0xf00;
    const uint16_t patternTableSize = 0x1000;
    const uint16_t universalBGColorLocalAddr = universalBGColorAddr - nametableMirrorSize -
        (nametableSize + attributeTableSize) * 2 - patternTableSize * 2;
    const uint8_t black = 0xf;
    vram[universalBGColorLocalAddr] = black;
    const unsigned int frameWidth = 256;
    const unsigned int frameHeight = 240;
    const unsigned int bytesPerPixel = 4;
    memset(frame, 0, frameWidth * frameHeight * bytesPerPixel);
    ppuDataBuffer = 0;
    op.clear();
    totalCycles = 0;
}

// Executes exactly one PPU cycle

void PPU::step(MMC& mmc, SDL_Renderer* renderer, SDL_Texture* texture, const bool mute) {
    skipCycle0();
    const unsigned int lastRenderLine = 239;
    const unsigned int prerenderLine = 261;
    if (op.scanline <= lastRenderLine || op.scanline == prerenderLine) {
        if (op.canFetch()) {
            fetch(mmc);
        }
        if (op.scanline != prerenderLine) {
            // Clearing the secondary OAM technically occurs over cycles 1 - 64, but it's redundant
            // to clear repeatedly
            if (op.cycle == 64) {
                clearSecondaryOAM();
            }
            if (op.cycle >= 65 && op.cycle <= 256) {
                evaluateSprites();
            }
        }
        if (op.isRendering()) {
            setPixel(mmc);
        }
        const unsigned int firstPixelOutputCycle = 4;
        const unsigned int lastPixelOutputCycle = firstPixelOutputCycle + 255;
        // If the current scanline is the last render line, and the current cycle is the last cycle
        // to set a pixel in the frame, then the frame is ready to be rendered
        if (op.scanline == lastRenderLine && op.cycle == lastPixelOutputCycle) {
            renderFrame(renderer, texture);
        }
    }
    if (isRenderingEnabled()) {
        updateScroll();
    }
    if (op.cycle == 1) {
        updatePPUStatus(mmc);
    }
    op.prepNextCycle();
    ++totalCycles;
}

// Handles register reads from the CPU

uint8_t PPU::readRegister(const uint16_t addr, MMC& mmc) {
    const uint16_t localAddr = getLocalRegisterAddr(addr);
    const uint16_t oamDMAAddr = 0x4014;
    if (localAddr == PPUStatus) {
        const uint8_t val = registers[PPUStatus];
        // Reset the vblank flag and write toggle whenever PPUSTATUS is read from
        registers[PPUStatus] &= 0x7f;
        w = false;
        // This if statement is for the PPUSTATUS race condition:
        // https://www.nesdev.org/wiki/PPU_frame_timing#VBL_Flag_Timing
        if (op.cycle >= 1 && op.cycle <= 3 && op.scanline == 241) {
            op.suppressNMI = true;
            if (op.cycle >= 2) {
                return val | 0x80;
            }
        }
        return val;
    // PPUDATA read buffer logic:
    // https://www.nesdev.org/wiki/PPU_registers#The_PPUDATA_read_buffer_(post-fetch)
    } else if (localAddr == PPUData) {
        const uint16_t paletteStart = 0x3f00;
        // If the v register is in not in the palette, then the read is buffered
        if ((v & 0x3fff) < paletteStart) {
            registers[PPUData] = ppuDataBuffer;
            ppuDataBuffer = readVRAM(v, mmc);
        // If the v register is in the palette, then the read is immediate
        } else {
            registers[PPUData] = readVRAM(v, mmc);
            // Subtract by 0x1000 to stay in the nametable:
            // https://forums.nesdev.org/viewtopic.php?f=3&t=18627
            ppuDataBuffer = readVRAM(v - 0x1000, mmc);
        }
        v += getPPUAddrInc();
    } else if (addr == oamDMAAddr) {
        return oamDMA;
    }
    return registers[localAddr];
}

// Handles register writes from the CPU

void PPU::writeRegister(const uint16_t addr, const uint8_t val, MMC& mmc, const bool mute) {
    const uint16_t localAddr = getLocalRegisterAddr(addr);
    const unsigned int lastCycle = 340;
    const unsigned int prerenderLine = 261;
    switch (localAddr) {
        case PPUCtrl:
            setTempNametableSelect(val & 3);
            // If the NMI flag is toggled from 0 to 1 in PPUCTRL, then another NMI can occur
            if (!isNMIEnabled() && (val & 0x80)) {
                op.nmiOccurred = false;
                // If the NMI flag is toggled at the very end of vblank, ensure that an NMI occurs
                if ((op.cycle == lastCycle && op.scanline == prerenderLine - 1) ||
                        (op.cycle == 0 && op.scanline == prerenderLine)) {
                    op.forceNMI = true;
                }
            }
            break;
        case PPUMask:
            // These if statements handle cases where rendering is enabled or disabled shortly
            // before cycle 0, which affect whether to skip cycle 0. Normally, the skipCycle0
            // function would handle this, but the timing is too early or late without these if
            // statements
            if (op.cycle == lastCycle && op.scanline == prerenderLine && (val & 0x18) &&
                    op.oddFrame && !isRenderingEnabled()) {
                --op.cycle;
            } else if (op.cycle == lastCycle - 1 && op.scanline == prerenderLine && !(val & 0x18) &&
                    !op.oddFrame && isRenderingEnabled()) {
                op.prepNextCycle();
            }
            break;
        case PPUScroll:
            writePPUScroll(val);
            break;
        case PPUAddr:
            writePPUAddr(val);
            break;
        case PPUData:
            writeVRAM(v, val, mmc, mute);
            v += getPPUAddrInc();
    }

    const uint16_t oamDMAAddr = 0x4014;
    if (addr == oamDMAAddr) {
        oamDMA = val;
    // PPUSTATUS is read-only
    } else if (localAddr != PPUStatus) {
        registers[localAddr] = val;
    }
}

// Allows the CPU to write to the PPU's OAM

void PPU::writeOAM(const uint8_t addr, const uint8_t val) {
    oam[addr] = val;
}

// Tells the CPU when it should enter an NMI

bool PPU::isNMIActive(MMC& mmc, const bool mute) {
    // Since the PPU runs after the CPU on each CPU cycle, the vblank flag isn't set yet when the
    // CPU polls for interrupts on the exact cycle that it's set. This cycle is explicitly referred
    // to here to handle that case
    if (isNMIEnabled() && (isVblank() || (op.cycle == 1 && op.scanline == 241) || op.forceNMI) &&
            !op.nmiOccurred && !op.suppressNMI) {
        op.nmiOccurred = true;
        op.forceNMI = false;
        return true;
    }
    return false;
}

unsigned int PPU::getTotalCycles() const {
    return totalCycles;
}

void PPU::clearTotalCycles() {
    totalCycles = 0;
}

void PPU::print(const bool isCycleDone) const {
    unsigned int inc = 0;
    std::string time;
    if (isCycleDone) {
        time = "After";
    } else {
        time = "Before";
        ++inc;
        std::cout << "--------------------------------------------------\n";
    }
    std::cout << "PPU Cycle " << totalCycles + inc << ": " << time
        << std::hex << "\n----------------------------\n"
        "PPU Fields\n----------------------------\n"
        "registers[0]      = 0x" << (unsigned int) registers[0] << "\n"
        "registers[1]      = 0x" << (unsigned int) registers[1] << "\n"
        "registers[2]      = 0x" << (unsigned int) registers[2] << "\n"
        "registers[3]      = 0x" << (unsigned int) registers[3] << "\n"
        "registers[4]      = 0x" << (unsigned int) registers[4] << "\n"
        "registers[5]      = 0x" << (unsigned int) registers[5] << "\n"
        "registers[6]      = 0x" << (unsigned int) registers[6] << "\n"
        "registers[7]      = 0x" << (unsigned int) registers[7] << "\n"
        "oamDMA            = 0x" << (unsigned int) oamDMA << "\n"
        "v                 = 0x" << (unsigned int) v << "\n"
        "t                 = 0x" << (unsigned int) t << "\n"
        "x                 = 0x" << (unsigned int) x << "\n"
        "w                 = " << std::dec << w << "\n"
        "ppuDataBuffer     = 0x" << std::hex << (unsigned int) ppuDataBuffer << "\n"
        "totalCycles       = " << std::dec << totalCycles <<
        "\n----------------------------\n" << std::hex <<
        "PPU Operation Fields\n----------------------------\n"
        "nametableAddr     = 0x" << (unsigned int) op.nametableAddr << "\n"
        "nametableEntry    = 0x" << (unsigned int) op.nametableEntry << "\n"
        "attributeAddr     = 0x" << (unsigned int) op.attributeAddr << "\n"
        "attributeEntry    = 0x" << (unsigned int) op.attributeEntry << "\n"
        "patternEntryLo    = 0x" << (unsigned int) op.patternEntryLo << "\n"
        "patternEntryHi    = 0x" << (unsigned int) op.patternEntryHi << "\n"
        "scanline          = " << std::dec << op.scanline << "\n"
        "pixel             = " << op.pixel << "\n"
        "attributeQuadrant = " << op.attributeQuadrant << "\n"
        "oddFrame          = " << op.oddFrame << "\n"
        "nmiOccurred       = " << op.nmiOccurred << "\n"
        "suppressNMI       = " << op.suppressNMI << "\n"
        "cycle             = " << op.cycle << "\n"
        "status            = " << op.status <<
        "\n--------------------------------------------------\n";
}

// Private Member Functions

// The first cycle is skipped on the first scanline if the frame is odd and rendering is enabled

void PPU::skipCycle0() {
    if (op.scanline == 0 && op.cycle == 0 && op.oddFrame && isRenderingEnabled()) {
        ++op.cycle;
    }
}

// Fetches data from the nametables, attribute tables, and pattern tables, which is then used later
// to form a palette index for a given pixel

void PPU::fetch(MMC& mmc) {
    const unsigned int fineYScroll = getFineYScroll();
    uint16_t addr = 0;
    switch (op.status) {
        case PPUOp::FetchNametableEntry:
            op.nametableAddr = getNametableAddr();
            op.nametableEntry = readVRAM(op.nametableAddr, mmc);
            break;
        case PPUOp::FetchAttributeEntry:
            updateAttribute();
            op.attributeEntry = readVRAM(op.attributeAddr, mmc);
            break;
        case PPUOp::FetchPatternEntryLo:
            // Nametable entries are tile numbers used to index into the pattern tables. Each 8x8
            // tile takes up 0x10 entries in the pattern table. The render line mod 8 represents the
            // tile row. The background pattern address is the default base address set by the CPU
            addr = op.nametableEntry * 0x10 + fineYScroll + getBGPatternAddr();
            op.patternEntryLo = readVRAM(addr, mmc);
            break;
        case PPUOp::FetchPatternEntryHi:
            // Each tile row has a low pattern entry and a high pattern entry. The low entries are
            // listed first (0 - 7) then the high entries are listed (8 - 0xf). E.g., the first tile
            // row would be located at 0 and 8, the second would be 1 and 9, etc.
            addr = op.nametableEntry * 0x10 + fineYScroll + getBGPatternAddr() + 8;
            op.patternEntryHi = readVRAM(addr, mmc);
            // The high byte of the pattern entry is the last fetch of the tile row, so that means
            // all info is done being fetched
            op.addTileRow();
            break;
        case PPUOp::FetchSpriteEntryLo:
            if (op.spriteNum < op.nextSprites.size()) {
                fetchSpriteEntry(mmc);
            }
            break;
        case PPUOp::FetchSpriteEntryHi:
            if (op.spriteNum < op.nextSprites.size()) {
                fetchSpriteEntry(mmc);
            }
    }
}

// Calculates the nametable address for the current tile row

uint16_t PPU::getNametableAddr() const {
    const unsigned int nametableBaseAddr = getNametableSelectAddr();
    // Coarse X scroll is like an x-coordinate in terms of tiles
    const unsigned int coarseXScroll = getCoarseXScroll();
    // Coarse Y scroll is like a y-coordinate in terms of tiles
    const unsigned int coarseYScroll = getCoarseYScroll();
    const unsigned int tilesPerRow = 32;
    return nametableBaseAddr + coarseXScroll + coarseYScroll * tilesPerRow;
}

// Returns the base nametable address for the current tile row

uint16_t PPU::getNametableSelectAddr() const {
    uint16_t nametableBaseAddr;
    const unsigned int nametableSelect = getNametableSelect();
    const uint16_t nametable0Start = 0x2000;
    const uint16_t nametable1Start = 0x2400;
    const uint16_t nametable2Start = 0x2800;
    const uint16_t nametable3Start = 0x2c00;
    switch (nametableSelect) {
        case 0:
            nametableBaseAddr = nametable0Start;
            break;
        case 1:
            nametableBaseAddr = nametable1Start;
            break;
        case 2:
            nametableBaseAddr = nametable2Start;
            break;
        case 3:
            nametableBaseAddr = nametable3Start;
    }
    return nametableBaseAddr;
}

// Updates both the attribute table address and attribute quadrant once the nametable address is
// known

void PPU::updateAttribute() {
    const unsigned int nametableBaseAddr = getNametableSelectAddr();
    // Coarse X scroll is like an x-coordinate in terms of tiles
    const unsigned int coarseXScroll = getCoarseXScroll();
    // Coarse Y scroll is like a y-coordinate in terms of tiles
    const unsigned int coarseYScroll = getCoarseYScroll();
    // Divide by 4 because each attribute table is 4x4 tiles
    const unsigned int attributeOffset = 0x3c0;
    const unsigned int xAttribute = coarseXScroll / 4;
    const unsigned int yAttribute = coarseYScroll / 4;
    const unsigned int tilesPerRow = 32;
    op.attributeAddr = nametableBaseAddr + attributeOffset + xAttribute + yAttribute * tilesPerRow /
        4;

    bool right = false;
    bool bottom = false;
    // The first 2 tiles / nametable addresses out of the 4 tiles are on the left side of the
    // attribute entry, while the last two are on the right
    if (coarseXScroll & 2) {
        right = true;
    }
    // Likewise, the first 2 tiles from the top are on the top side, while the last two are on the
    // bottom
    if (coarseYScroll & 2) {
        bottom = true;
    }

    if (!right && !bottom) {
        op.attributeQuadrant = PPUOp::TopLeft;
    } else if (right && !bottom) {
        op.attributeQuadrant = PPUOp::TopRight;
    } else if (!right && bottom) {
        op.attributeQuadrant = PPUOp::BottomLeft;
    } else {
        op.attributeQuadrant = PPUOp::BottomRight;
    }
}

// Fetches data from the pattern table for each sprite that was selected during sprite evaluation

void PPU::fetchSpriteEntry(MMC& mmc) {
    Sprite& sprite = op.nextSprites[op.spriteNum];
    uint16_t basePatternAddr = getSpritePatternAddr();
    const unsigned int spriteHeight = getSpriteHeight();
    uint8_t tileIndexNum = sprite.tileIndexNum;
    unsigned int patternEntriesPerSprite = 0x10;
    // This if statement prepares the fetch for 8x16 sprites:
    // https://www.nesdev.org/wiki/PPU_OAM#Byte_1
    if (spriteHeight == 16) {
        if (tileIndexNum & 1) {
            basePatternAddr = 0x1000;
        } else {
            basePatternAddr = 0;
        }
        tileIndexNum = tileIndexNum >> 1;
        patternEntriesPerSprite = 0x20;
    }

    uint16_t addr = basePatternAddr + tileIndexNum * patternEntriesPerSprite;
    unsigned int tileRowIndex = sprite.getTileRowIndex(op.scanline, spriteHeight);
    addr += tileRowIndex;
    if (op.status == PPUOp::FetchSpriteEntryLo) {
        sprite.patternEntryLo = readVRAM(addr, mmc);
    } else {
        // Each tile row has a low pattern entry and a high pattern entry. The low entries are
        // listed first (0 - 7) then the high entries are listed (8 - 0xf). E.g., the first tile row
        // would be located at 0 and 8, the second would be 1 and 9, etc.
        addr += 0x8;
        sprite.patternEntryHi = readVRAM(addr, mmc);
        ++op.spriteNum;
    }
}

// Clears the secondary OAM before sprite evaluation

void PPU::clearSecondaryOAM() {
    memset(secondaryOAM, 0xff, 0x20);
}

// Scans through the 64 sprites in the OAM to find at most 8 sprites that are in range of the
// current scanline: https://www.nesdev.org/wiki/PPU_sprite_evaluation

void PPU::evaluateSprites() {
    // If all sprites have been checked, the max of 8 sprites have been reached, or rendering is
    // disabled, then don't bother evaluating sprites
    if (op.spriteNum >= 64 || (op.nextSprites.size() >= 8 && op.oamEntryNum == 0) ||
            (!isRenderingEnabled())) {
        return;
    }

    // Read from OAM on odd cycles
    if (op.cycle % 2) {
        op.oamEntry = oam[op.spriteNum * 4 + op.oamEntryNum];
        return;
    }

    // Write to secondary OAM on even cycles
    Sprite sprite(op.oamEntry, op.spriteNum);
    const unsigned int spriteHeight = getSpriteHeight();
    unsigned int foundSpriteNum = op.nextSprites.size();
    switch (op.oamEntryNum) {
        case 0:
            // If the sprite is in range of the current scanline, add it to the list and start
            // collecting the remaining data for it. Otherwise, move on to the next sprite
            if (sprite.isYInRange(op.scanline, spriteHeight)) {
                secondaryOAM[foundSpriteNum * 4 + op.oamEntryNum] = op.oamEntry;
                op.nextSprites.push_back(sprite);
                ++op.oamEntryNum;
            } else {
                ++op.spriteNum;
            }
            break;
        case 1:
            --foundSpriteNum;
            secondaryOAM[foundSpriteNum * 4 + op.oamEntryNum] = op.oamEntry;
            op.nextSprites.back().tileIndexNum = op.oamEntry;
            ++op.oamEntryNum;
            break;
        case 2:
            --foundSpriteNum;
            secondaryOAM[foundSpriteNum * 4 + op.oamEntryNum] = op.oamEntry;
            op.nextSprites.back().attributes = op.oamEntry;
            ++op.oamEntryNum;
            break;
        case 3:
            --foundSpriteNum;
            secondaryOAM[foundSpriteNum * 4 + op.oamEntryNum] = op.oamEntry;
            op.nextSprites.back().xPos = op.oamEntry;
            // Done collecting data for the current sprite, so proceed with the next sprite
            op.oamEntryNum = 0;
            ++op.spriteNum;
    }
}

// Performs all rendering logic for the current pixel in the frame

void PPU::setPixel(MMC& mmc) {
    const uint8_t bgPalette = op.getPalette(x);
    const uint8_t bgPaletteLower = bgPalette & 3;
    uint8_t spritePalette = 0;
    uint8_t spritePaletteLower = 0;
    std::vector<Sprite>::iterator spriteIterator;
    bool foundSprite = false;
    for (spriteIterator = op.currentSprites.begin(); spriteIterator != op.currentSprites.end();
            ++spriteIterator) {
        // If the sprite is in range of the current pixel, then it needs to be considered for
        // rendering. The first sprite found is used, even if there are other sprites in range. The
        // sprites are prioritized in the order that they're listed in the OAM (sprite 0 being the
        // most prioritized)
        if (spriteIterator->isXInRange(op.pixel)) {
            spritePalette = spriteIterator->getPalette(op.pixel);
            spritePaletteLower = spritePalette & 3;
            // If the lower 2 bits are nonzero, then the sprite palette is not transparent. In other
            // words, a sprite has been found
            if (spritePaletteLower) {
                foundSprite = true;
                break;
            }
        }
    }

    bool spriteChosen = false;
    if (foundSprite) {
        // Determine whether to output the background pixel or the sprite pixel
        spriteChosen = spriteIterator->isChosen(bgPaletteLower, spritePaletteLower);
    }

    uint8_t paletteEntry = 0;
    const uint16_t mirroredV = getUpperMirrorAddr(v);
    const uint16_t paletteStart = 0x3f00;
    const uint16_t paletteEnd = 0x3fff;
    // The palette is used as an index for retrieving the palette entry, which is associated with
    // RGB values
    if (spriteChosen && areSpritesShown() && (op.pixel > 7 || areSpritesLeftColShown())) {
        const uint16_t spritePaletteStart = 0x3f10;
        // Output the sprite pixel
        paletteEntry = readVRAM(spritePaletteStart + spritePalette, mmc);
        setSprite0Hit(*spriteIterator, bgPalette);
    } else if (isBGShown() && (op.pixel > 7 || isBGLeftColShown())) {
        // Output the background pixel
        paletteEntry = readVRAM(paletteStart + bgPalette, mmc);
        if (foundSprite && areSpritesShown()) {
            setSprite0Hit(*spriteIterator, bgPalette);
        }
    } else if (!isRenderingEnabled() && mirroredV >= paletteStart && mirroredV <= paletteEnd) {
        // Output the current VRAM address:
        // https://www.nesdev.org/wiki/PPU_palettes#The_background_palette_hack
        paletteEntry = readVRAM(v, mmc);
    } else {
        // Output the universal background color:
        // https://www.nesdev.org/wiki/PPU_palettes#Memory_Map
        paletteEntry = readVRAM(paletteStart, mmc);
    }
    setRGB(paletteEntry);
}

// Sets the sprite 0 hit flag in the PPUSTATUS register:
// https://www.nesdev.org/wiki/PPU_OAM#Sprite_zero_hits

void PPU::setSprite0Hit(const Sprite& sprite, const uint8_t bgPalette) {
    if (sprite.spriteNum == 0 && bgPalette && isBGShown() && op.pixel != 255 &&
            (op.pixel > 7 || (isBGLeftColShown() && areSpritesLeftColShown()))) {
        registers[PPUStatus] |= 0x40;
    }
}

// Sets the current pixel's RGB values in the frame

void PPU::setRGB(const uint8_t paletteEntry) {
    const unsigned int frameWidth = 256;
    // Each 4 bytes in the frame is a pixel. The first byte is the blue value, the second is the
    // green value, the third is the red value, and the fourth is the opacity
    const unsigned int blueIndex = (op.pixel + op.scanline * frameWidth) * 4;
    const unsigned int greenIndex = blueIndex + 1;
    const unsigned int redIndex = blueIndex + 2;
    frame[redIndex] = palette[paletteEntry].red;
    frame[greenIndex] = palette[paletteEntry].green;
    frame[blueIndex] = palette[paletteEntry].blue;
    frame[blueIndex + 3] = SDL_ALPHA_OPAQUE;
}

// Renders a frame via SDL

void PPU::renderFrame(SDL_Renderer* renderer, SDL_Texture* texture) {
    if (renderer != nullptr && texture != nullptr) {
        SDL_RenderClear(renderer);
        uint8_t* lockedPixels = nullptr;
        int pitch = 0;
        SDL_LockTexture(texture, nullptr, (void**) &lockedPixels, &pitch);
        const unsigned int frameWidth = 256;
        const unsigned int frameHeight = 240;
        const unsigned int bytesPerPixel = 4;
        const unsigned int frameSize = frameWidth * frameHeight * bytesPerPixel;
        std::memcpy(lockedPixels, frame, frameSize);
        SDL_UnlockTexture(texture);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }
}

// Updates the current scrolling position in the v register if necessary

void PPU::updateScroll() {
    // The high byte of the pattern entry is the last fetch of the tile row, so coarse X scroll can
    // be incremented to the next tile if that was the last fetch
    if (op.status == PPUOp::FetchPatternEntryHi && op.canFetch()) {
        incrementCoarseXScroll();
    }

    const unsigned int lastRenderLine = 239;
    const unsigned int prerenderLine = 261;
    // The following cycles are from this timing diagram:
    // https://www.nesdev.org/w/images/default/d/d1/Ntsc_timing.png
    // If the cycle is 256 or 257, all tile rows are done being fetched for the current scanline and
    // it is safe to make updates
    if (op.cycle == 256 && op.scanline <= lastRenderLine) {
        incrementYScroll();
    } else if (op.cycle == 257 && (op.scanline <= lastRenderLine || op.scanline == prerenderLine)) {
        // Reset the horizontal scrolling position to the left side
        setCoarseXScroll(getTempCoarseXScroll());
        resetHorizontalNametable();
    // If the scanline is the pre-render line, then all tiles are done being fetched and it is time
    // to reset the vertical scrolling position
    } else if (op.cycle >= 280 && op.cycle <= 304 && op.scanline == prerenderLine) {
        setCoarseYScroll(getTempCoarseYScroll());
        setFineYScroll(getTempFineYScroll());
        setNametableSelect(getTempNametableSelect());
    }
}

// Increments the coarse X scroll in the v register. Credit to the NESdev wiki for this function:
// https://www.nesdev.org/wiki/PPU_scrolling#Coarse_X_increment

void PPU::incrementCoarseXScroll() {
    const unsigned int coarseXScroll = getCoarseXScroll();
    const unsigned int tilesPerRow = 32;
    // The scrolling positions start at 0, so the total tiles per row in a frame is subtracted by 1
    if (coarseXScroll == tilesPerRow - 1) {
        // Handle wraparound
        setCoarseXScroll(0);
        switchHorizontalNametable();
    } else {
        setCoarseXScroll(coarseXScroll + 1);
    }
}

// Increments the fine Y scroll and coarse Y scroll in the v register. Credit to the NESdev wiki for
// this function: https://www.nesdev.org/wiki/PPU_scrolling#Y_increment

void PPU::incrementYScroll() {
    const unsigned int fineYScroll = getFineYScroll();
    const unsigned int pixelsPerTile = 8;
    // The scrolling positions start at 0, so the total pixels per tile is subtracted by 1
    if (fineYScroll == pixelsPerTile - 1) {
        // Handle wraparound for inside the tile
        setFineYScroll(0);

        const unsigned int coarseYScroll = getCoarseYScroll();
        const unsigned int tilesPerRow = 32;
        const unsigned int tilesPerColumn = 30;
        // The scrolling positions start at 0, so the total tiles per row in a frame is subtracted
        // by 1
        if (coarseYScroll == tilesPerColumn - 1) {
            // Handle wraparound
            setCoarseYScroll(0);
            switchVerticalNametable();
        // If coarse Y scroll is 31, then it wraps around to 0 without switching the nametable
        } else if (coarseYScroll == tilesPerRow - 1) {
            setCoarseYScroll(0);
        } else {
            setCoarseYScroll(coarseYScroll + 1);
        }
    } else {
        setFineYScroll(fineYScroll + 1);
    }
}

// Switches the nametable from left to right or vice versa:
// https://www.nesdev.org/wiki/PPU_nametables. Nametable 0 is top left, 1 is top right, 2 is bottom
// left, and 3 is bottom right

void PPU::switchHorizontalNametable() {
    const unsigned int nametableSelect = getNametableSelect();
    if (nametableSelect == 0 || nametableSelect == 2) {
        setNametableSelect(nametableSelect + 1);
    } else {
        setNametableSelect(nametableSelect - 1);
    }
}

// Switches the nametable from top to bottom or vice versa:
// https://www.nesdev.org/wiki/PPU_nametables. Nametable 0 is top left, 1 is top right, 2 is bottom
// left, and 3 is bottom right

void PPU::switchVerticalNametable() {
    const unsigned int nametableSelect = getNametableSelect();
    if (nametableSelect == 0 || nametableSelect == 1) {
        setNametableSelect(nametableSelect + 2);
    } else {
        setNametableSelect(nametableSelect - 2);
    }
}

// Resets the nametable to horizontal position of the t register (the top left corner of the frame)
// without losing the current vertical position: https://www.nesdev.org/wiki/PPU_nametables.
// Nametable 0 is top left, 1 is top right, 2 is bottom left, and 3 is bottom right

void PPU::resetHorizontalNametable() {
    const unsigned int nametableSelect = getNametableSelect();
    const unsigned int tempNametableSelect = getTempNametableSelect();
    // If the nametable in v is either of the bottom nametables and the nametable in t is either of
    // the top nametables, reset v to the nametable directly below t's nametable
    if (nametableSelect >= 2 && tempNametableSelect <= 1) {
        setNametableSelect(tempNametableSelect + 2);
    // If the nametable in v is either of the top nametables and the nametable in t is either of the
    // bottom nametables, reset v to the nametable directly above t's nametable (as if it wrapped
    // around)
    } else if (nametableSelect <= 1 && tempNametableSelect >= 2) {
        setNametableSelect(tempNametableSelect - 2);
    // If both of the above cases are false, the vertical position of v's nametable is the same as
    // t's nametable, so v can reset to t's nametable
    } else {
        setNametableSelect(tempNametableSelect);
    }
}

// Sets the vblank flag when vblank starts, and clears the vblank flag and sprite 0 hit flag before
// rendering starts. This function is only called on cycle 1 as shown in this diagram:
// https://www.nesdev.org/w/images/default/d/d1/Ntsc_timing.png

void PPU::updatePPUStatus(MMC& mmc) {
    const unsigned int prerenderLine = 261;
    if (op.scanline == 241 && !op.suppressNMI) {
        registers[PPUStatus] |= 0x80;
    } else if (op.scanline == prerenderLine) {
        registers[PPUStatus] &= 0x3f;
    }
}

// Retrieves the new position of the top left corner of the frame from the CPU and stores it in the
// t register: https://www.nesdev.org/wiki/PPU_scrolling#Register_controls

void PPU::writePPUScroll(const uint8_t val) {
    if (w) {
        setTempCoarseYScroll(val >> 3);
        setTempFineYScroll(val & 7);
        w = false;
    } else {
        setTempCoarseXScroll(val >> 3);
        x = val & 7;
        w = true;
    }
}

// Gets the high and low bytes of the VRAM address from the CPU and eventually stores it in the v
// register: https://www.nesdev.org/wiki/PPU_registers#Address_($2006)_%3E%3E_write_x2

void PPU::writePPUAddr(const uint8_t val) {
    if (w) {
        t &= 0xff00;
        t |= val;
        v = t;
        w = false;
    } else {
        t &= 0xff;
        t |= (val & 0x3f) << 8;
        w = true;
    }
}

// Handles VRAM reads by both the CPU and PPU, deals with mirrored address ranges, and checks when
// to send the read request to the MMC if the address is in the pattern table ($0000 - $1fff)

uint8_t PPU::readVRAM(const uint16_t addr, MMC& mmc) const {
    uint16_t localAddr = getLocalVRAMAddr(addr, mmc, true);
    const uint16_t upperMirrorAddr = getUpperMirrorAddr(addr);
    const uint16_t nametable0Start = 0x2000;
    // Since addresses $0000 - $1fff are in the CHR memory on the cartridge and not the VRAM,
    // getLocalVRAMAddr subtracts its resulting address by 0x2000. To identify whether the address
    // is for CHR memory or VRAM, the mirrored address in the range $0000 - $3fff is checked to see
    // if it lies within $0000 - $1fff
    if (upperMirrorAddr < nametable0Start) {
        return mmc.readCHR(upperMirrorAddr);
    }
    return vram[localAddr];
}

// Handles VRAM writes by both the CPU and PPU, deals with mirrored address ranges, and checks when
// to send the write request to the MMC if the address is in the pattern table ($0000 - $1fff)

void PPU::writeVRAM(const uint16_t addr, const uint8_t val, MMC& mmc, const bool mute) {
    if (!mute) {
        std::cout << std::hex << "0x" << (unsigned int) val << " has been written to the VRAM "
            "address 0x" << (unsigned int) addr <<
            "\n--------------------------------------------------\n" << std::dec;
    }

    uint16_t localAddr = getLocalVRAMAddr(addr, mmc, false);
    const uint16_t upperMirrorAddr = getUpperMirrorAddr(addr);
    const uint16_t nametable0Start = 0x2000;
    // Since addresses $0000 - $1fff are in the CHR memory on the cartridge and not the VRAM,
    // getLocalVRAMAddr subtracts its resulting address by 0x2000. To identify whether the address
    // is for CHR memory or VRAM, the mirrored address in the range $0000 - $3fff is checked to see
    // if it lies within $0000 - $1fff
    if (upperMirrorAddr < nametable0Start) {
        mmc.writeCHR(upperMirrorAddr, val);
    } else {
        vram[localAddr] = val;
    }
}

// Maps the CPU address to the PPU's local field, registers

uint16_t PPU::getLocalRegisterAddr(const uint16_t addr) const {
    // Addresses $2008 - $3fff in the CPU memory map are mirrors of $2000 - $2007, so only the lower
    // 7 bits are needed
    return addr & 7;
}

// Maps the PPU/VRAM address to the PPU's local field, vram

uint16_t PPU::getLocalVRAMAddr(const uint16_t addr, MMC& mmc, const bool isRead) const {
    uint16_t localAddr = getUpperMirrorAddr(addr);
    const uint16_t nametable0Start = 0x2000;
    const uint16_t paletteStart = 0x3f00;
    if (localAddr >= paletteStart) {
        // Addresses $3f20 - $3fff are mirrors of $3f00 - $3f1f
        localAddr &= 0x3f1f;
        if (localAddr % 4 == 0) {
            // Addresses $3f10, $3f14, $3f18, and $3f1c are mirrors of $3f00, $3f04, $3f08, and
            // $3f0c, respectively: https://www.nesdev.org/wiki/PPU_palettes#Memory_Map
            if (localAddr >= 0x3f10) {
                localAddr &= 0xffef;
            }
            // Every fourth address ($3f04, $3f08, etc.) is a mirror of the universal background
            // color ($3f00) while rendering. However, this isn't the case when rendering is
            // disabled: https://www.nesdev.org/wiki/PPU_palettes#The_background_palette_hack
            if (isRead && isRenderingEnabled()) {
                localAddr = paletteStart;
            }
        }
        // The PPU only stores 2 nametables while the other two are mirrored or on the
        // cartridge/MMC. Since only 2 nametables are actually being stored, the palette address is
        // subtracted by 0x1700 to reduce it to where the third nametable is, ensuring that the
        // palette data starts immediately after the two nametables in the vram field
        localAddr -= 0x1700;
    } else if (localAddr >= nametable0Start) {
        // Addresses $3000 - $3eff are mirrors are $2000 - $2eff
        localAddr &= 0x2fff;

        // Nametable-specific mirroring is now handled here depending on which mirroring is chosen
        // by the cartridge/MMC
        switch (mmc.getMirroring()) {
            case MMC::Horizontal:
                localAddr = getHorizontalMirrorAddr(addr);
                break;
            case MMC::Vertical:
                localAddr = getVerticalMirrorAddr(addr);
                break;
            case MMC::SingleScreen0:
                localAddr = getSingle0MirrorAddr(addr);
                break;
            case MMC::SingleScreen1:
                localAddr = getSingle1MirrorAddr(addr);
                break;
            // The following types of mirroring require mappers that haven't been impelemented yet,
            // which store 1 or 2 nametables on the cartridge/MMC
            case MMC::SingleScreen2:
                std::cerr << "Single-screen 2 mirroring is not implemented\n";
                exit(1);
            case MMC::SingleScreen3:
                std::cerr << "Single-screen 3 mirroring is not implemented\n";
                exit(1);
            case MMC::FourScreen:
                std::cerr << "Four-screen mirroring is not implemented\n";
                exit(1);
        }
    }
    // Lastly, the address is subtracted by 0x2000 because the address range $0000 - $1fff is in the
    // cartridge/MMC, so the first nametable can start at the very beginning of the vram field
    // instead
    return localAddr - 0x2000;
}

// The upper addresses $4000 - $ffff are mirrors of $0000 - $3fff, so this function mirrors the
// address to stay in the latter range

uint16_t PPU::getUpperMirrorAddr(const uint16_t addr) const {
    return addr & 0x3fff;
}

// Shifts the address ranges so that nametable 0 and attribute table 0 ($2000 - $23ff) as well as
// nametable 1 and attribute table 1 ($2400 - $27ff) refer to the first nametable ($2000 - $23ff),
// while nametable 2 and attribute table 2 ($2800 - $2bff) as well as nametable 3 and attribute
// table 3 ($2c00 - $2fff) refer to the second nametable ($2400 - $27ff)
// https://www.nesdev.org/wiki/Mirroring#Horizontal

uint16_t PPU::getHorizontalMirrorAddr(const uint16_t addr) const {
    uint16_t localAddr = addr;
    const uint16_t nametable1Start = 0x2400;
    const uint16_t nametable2Start = 0x2800;
    const uint16_t nametable3Start = 0x2c00;
    if (localAddr >= nametable3Start) {
        localAddr -= 0x800;
    } else if (localAddr >= nametable2Start) {
        localAddr -= 0x400;
    } else if (localAddr >= nametable1Start) {
        localAddr -= 0x400;
    }
    return localAddr;
}

// Shifts the address ranges so that nametable 0 and attribute table 0 ($2000 - $23ff) as well as
// nametable 2 and attribute table 2 ($2800 - $2bff) refer to the first nametable ($2000 - $23ff),
// while nametable 1 and attribute table 1 ($2400 - $27ff) as well as nametable 3 and attribute
// table 3 ($2c00 - $2fff) refer to the second nametable ($2400 - $27ff)
// https://www.nesdev.org/wiki/Mirroring#Vertical

uint16_t PPU::getVerticalMirrorAddr(const uint16_t addr) const {
    uint16_t localAddr = addr;
    const uint16_t nametable2Start = 0x2800;
    const uint16_t nametable3Start = 0x2c00;
    if (localAddr >= nametable3Start) {
        localAddr -= 0x800;
    } else if (localAddr >= nametable2Start) {
        localAddr -= 0x800;
    }
    return localAddr;
}

// Shifts the address ranges so that nametable 0 and attribute table 0 ($2000 - $23ff), nametable 1
// and attribute table 1 ($2400 - $27ff), nametable 2 and attribute table 2 ($2800 - $2bff), and
// nametable 3 and attribute table 3 ($2c00 - $2fff) all refer to the first nametable ($2000 -
// $23ff)
// https://www.nesdev.org/wiki/Mirroring#Single-Screen

uint16_t PPU::getSingle0MirrorAddr(const uint16_t addr) const {
    uint16_t localAddr = addr;
    const uint16_t nametable1Start = 0x2400;
    const uint16_t nametable2Start = 0x2800;
    const uint16_t nametable3Start = 0x2c00;
    if (localAddr >= nametable3Start) {
        localAddr -= 0xc00;
    } else if (localAddr >= nametable2Start) {
        localAddr -= 0x800;
    } else if (localAddr >= nametable1Start) {
        localAddr -= 0x400;
    }
    return localAddr;
}

// Shifts the address ranges so that nametable 0 and attribute table 0 ($2000 - $23ff), nametable 1
// and attribute table 1 ($2400 - $27ff), nametable 2 and attribute table 2 ($2800 - $2bff), and
// nametable 3 and attribute table 3 ($2c00 - $2fff) all refer to the second nametable ($2400 -
// $27ff)
// https://www.nesdev.org/wiki/Mirroring#Single-Screen

uint16_t PPU::getSingle1MirrorAddr(const uint16_t addr) const {
    uint16_t localAddr = addr;
    const uint16_t nametable1Start = 0x2400;
    const uint16_t nametable2Start = 0x2800;
    const uint16_t nametable3Start = 0x2c00;
    if (localAddr >= nametable3Start) {
        localAddr -= 0x800;
    } else if (localAddr >= nametable2Start) {
        localAddr -= 0x400;
    } else if (localAddr < nametable1Start) {
        localAddr += 0x400;
    }
    return localAddr;
}

uint16_t PPU::getNametableBaseAddr() const {
    const uint8_t flag = registers[PPUCtrl] & 3;
    const uint16_t nametable0Start = 0x2000;
    const uint16_t nametable1Start = 0x2400;
    const uint16_t nametable2Start = 0x2800;
    const uint16_t nametable3Start = 0x2c00;
    switch (flag) {
        case 0:
            return nametable0Start;
        case 1:
            return nametable1Start;
        case 2:
            return nametable2Start;
    }
    return nametable3Start;
}

unsigned int PPU::getPPUAddrInc() const {
    const uint8_t flag = registers[PPUCtrl] & 4;
    if (flag) {
        return 0x20;
    }
    return 1;
}

uint16_t PPU::getSpritePatternAddr() const {
    const uint8_t flag = registers[PPUCtrl] & 8;
    if (flag) {
        return 0x1000;
    }
    return 0;
}

uint16_t PPU::getBGPatternAddr() const {
    const uint8_t flag = registers[PPUCtrl] & 0x10;
    if (flag) {
        return 0x1000;
    }
    return 0;
}

unsigned int PPU::getSpriteHeight() const {
    const uint8_t flag = registers[PPUCtrl] & 0x20;
    if (flag) {
        return 16;
    }
    return 8;
}

bool PPU::isColorOutputOnEXT() const {
    return registers[PPUCtrl] & 0x40;
}

bool PPU::isNMIEnabled() const {
    return registers[PPUCtrl] & 0x80;
}

bool PPU::isGrayscale() const {
    return registers[PPUMask] & 1;
}

bool PPU::isBGLeftColShown() const {
    return registers[PPUMask] & 2;
}

bool PPU::areSpritesLeftColShown() const {
    return registers[PPUMask] & 4;
}

bool PPU::isBGShown() const {
    return registers[PPUMask] & 8;
}

bool PPU::areSpritesShown() const {
    return registers[PPUMask] & 0x10;
}

bool PPU::isRenderingEnabled() const {
    return isBGShown() || areSpritesShown();
}

bool PPU::isRedEmphasized() const {
    return registers[PPUMask] & 0x20;
}

bool PPU::isGreenEmphasized() const {
    return registers[PPUMask] & 0x40;
}

bool PPU::isBlueEmphasized() const {
    return registers[PPUMask] & 0x80;
}

bool PPU::isSpriteOverflow() const {
    return registers[PPUStatus] & 0x20;
}

bool PPU::isSpriteZeroHit() const {
    return registers[PPUStatus] & 0x40;
}

bool PPU::isVblank() const {
    return registers[PPUStatus] & 0x80;
}

unsigned int PPU::getCoarseXScroll() const {
    return v & 0x1f;
}

unsigned int PPU::getTempCoarseXScroll() const {
    return t & 0x1f;
}

unsigned int PPU::getCoarseYScroll() const {
    return (v >> 5) & 0x1f;
}

unsigned int PPU::getTempCoarseYScroll() const {
    return (t >> 5) & 0x1f;
}

unsigned int PPU::getNametableSelect() const {
    return (v >> 10) & 3;
}

unsigned int PPU::getTempNametableSelect() const {
    return (t >> 10) & 3;
}

unsigned int PPU::getFineYScroll() const {
    return (v >> 12) & 7;
}

unsigned int PPU::getTempFineYScroll() const {
    return (t >> 12) & 7;
}

void PPU::setCoarseXScroll(const unsigned int val) {
    v &= 0x7fe0;
    v |= val & 0x1f;
}

void PPU::setTempCoarseXScroll(const unsigned int val) {
    t &= 0x7fe0;
    t |= val & 0x1f;
}

void PPU::setCoarseYScroll(const unsigned int val) {
    v &= 0x7c1f;
    v |= (val & 0x1f) << 5;
}

void PPU::setTempCoarseYScroll(const unsigned int val) {
    t &= 0x7c1f;
    t |= (val & 0x1f) << 5;
}

void PPU::setNametableSelect(const unsigned int val) {
    v &= 0x73ff;
    v |= (val & 3) << 10;
}

void PPU::setTempNametableSelect(const unsigned int val) {
    t &= 0x73ff;
    t |= (val & 3) << 10;
}

void PPU::setFineYScroll(const unsigned int val) {
    v &= 0xfff;
    v |= (val & 7) << 12;
}

void PPU::setTempFineYScroll(const unsigned int val) {
    t &= 0xfff;
    t |= (val & 7) << 12;
}

// Initializes the palette with interpreted RGB values that correspond to the given palette entry
// (the array indices). These RGB values were taken from: https://www.nesdev.com/NESDoc.pdf

void PPU::initializePalette() {
    palette[0x00] = {0x75, 0x75, 0x75};
    palette[0x01] = {0x27, 0x1b, 0x8f};
    palette[0x02] = {0x00, 0x00, 0xab};
    palette[0x03] = {0x47, 0x00, 0x9f};
    palette[0x04] = {0x8f, 0x00, 0x77};
    palette[0x05] = {0xab, 0x00, 0x13};
    palette[0x06] = {0xa7, 0x00, 0x00};
    palette[0x07] = {0x7f, 0x0b, 0x00};
    palette[0x08] = {0x43, 0x2f, 0x00};
    palette[0x09] = {0x00, 0x47, 0x00};
    palette[0x0a] = {0x00, 0x51, 0x00};
    palette[0x0b] = {0x00, 0x3f, 0x17};
    palette[0x0c] = {0x1b, 0x3f, 0x5f};
    palette[0x0d] = {0x00, 0x00, 0x00};
    palette[0x0e] = {0x00, 0x00, 0x00};
    palette[0x0f] = {0x00, 0x00, 0x00};
    palette[0x10] = {0xbc, 0xbc, 0xbc};
    palette[0x11] = {0x00, 0x73, 0xef};
    palette[0x12] = {0x23, 0x3b, 0xef};
    palette[0x13] = {0x83, 0x00, 0xf3};
    palette[0x14] = {0xbf, 0x00, 0xbf};
    palette[0x15] = {0xe7, 0x00, 0x5b};
    palette[0x16] = {0xdb, 0x2b, 0x00};
    palette[0x17] = {0xcb, 0x4f, 0x0f};
    palette[0x18] = {0x8b, 0x73, 0x00};
    palette[0x19] = {0x00, 0x97, 0x00};
    palette[0x1a] = {0x00, 0xab, 0x00};
    palette[0x1b] = {0x00, 0x93, 0x3b};
    palette[0x1c] = {0x00, 0x83, 0x8b};
    palette[0x1d] = {0x00, 0x00, 0x00};
    palette[0x1e] = {0x00, 0x00, 0x00};
    palette[0x1f] = {0x00, 0x00, 0x00};
    palette[0x20] = {0xff, 0xff, 0xff};
    palette[0x21] = {0x3f, 0xbf, 0xff};
    palette[0x22] = {0x5f, 0x97, 0xff};
    palette[0x23] = {0xa7, 0x8b, 0xfd};
    palette[0x24] = {0xf7, 0x7b, 0xff};
    palette[0x25] = {0xff, 0x77, 0xb7};
    palette[0x26] = {0xff, 0x77, 0x63};
    palette[0x27] = {0xff, 0x9b, 0x3b};
    palette[0x28] = {0xf3, 0xbf, 0x3f};
    palette[0x29] = {0x83, 0xd3, 0x13};
    palette[0x2a] = {0x4f, 0xdf, 0x4b};
    palette[0x2b] = {0x58, 0xf8, 0x98};
    palette[0x2c] = {0x00, 0xeb, 0xdb};
    palette[0x2d] = {0x00, 0x00, 0x00};
    palette[0x2e] = {0x00, 0x00, 0x00};
    palette[0x2f] = {0x00, 0x00, 0x00};
    palette[0x30] = {0xff, 0xff, 0xff};
    palette[0x31] = {0xab, 0xe7, 0xff};
    palette[0x32] = {0xc7, 0xd7, 0xff};
    palette[0x33] = {0xd7, 0xcb, 0xff};
    palette[0x34] = {0xff, 0xc7, 0xff};
    palette[0x35] = {0xff, 0xc7, 0xdb};
    palette[0x36] = {0xff, 0xbf, 0xb3};
    palette[0x37] = {0xff, 0xdb, 0xab};
    palette[0x38] = {0xff, 0xe7, 0xa3};
    palette[0x39] = {0xe3, 0xff, 0xa3};
    palette[0x3a] = {0xab, 0xf3, 0xbf};
    palette[0x3b] = {0xb3, 0xff, 0xcf};
    palette[0x3c] = {0x9f, 0xff, 0xf3};
    palette[0x3d] = {0x00, 0x00, 0x00};
    palette[0x3e] = {0x00, 0x00, 0x00};
    palette[0x3f] = {0x00, 0x00, 0x00};
}