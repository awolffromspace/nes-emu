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
    memset(registers, 0, PPU_REGISTER_SIZE);
    vram[UNIVERSAL_BG_INDEX] = BLACK;
    initializePalette();
}

void PPU::clear() {
    memset(registers, 0, PPU_REGISTER_SIZE);
    oamDMA = 0;
    v = 0;
    t = 0;
    x = 0;
    w = false;
    memset(oam, 0, OAM_SIZE);
    memset(secondaryOAM, 0xff, SECONDARY_OAM_SIZE);
    memset(vram, 0, VRAM_SIZE);
    vram[UNIVERSAL_BG_INDEX] = BLACK;
    memset(frame, 0, FRAME_SIZE);
    ppuDataBuffer = 0;
    op.clear();
    totalCycles = 0;
}

// Executes exactly one PPU cycle

void PPU::step(MMC& mmc, SDL_Renderer* renderer, SDL_Texture* texture, bool mute) {
    skipCycle0();
    if (op.scanline <= LAST_RENDER_LINE || op.scanline == PRERENDER_LINE) {
        if (op.canFetch()) {
            fetch(mmc);
        }
        if (op.scanline != PRERENDER_LINE) {
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
        // If the current scanline is the last render line, and the current cycle is the last cycle
        // to set a pixel in the frame, then the frame is ready to be rendered
        if (op.scanline == LAST_RENDER_LINE && op.cycle == LAST_CYCLE_TO_OUTPUT_PIXEL) {
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

uint8_t PPU::readRegister(uint16_t addr, MMC& mmc) {
    uint16_t localAddr = getLocalRegisterAddr(addr);
    if (localAddr == 2) {
        uint8_t val = registers[2];
        registers[2] &= 0x7f;
        w = false;
        if (op.cycle >= 1 && op.cycle <= 3 && op.scanline == 241) {
            op.suppressNMI = true;
            if (op.cycle >= 2) {
                return val | 0x80;
            }
        }
        return val;
    // PPUDATA read buffer logic:
    // https://www.nesdev.org/wiki/PPU_registers#The_PPUDATA_read_buffer_(post-fetch)
    } else if (localAddr == 7) {
        // If the v register is in not in the palette, then the read is buffered
        if ((v & 0x3fff) < IMAGE_PALETTE_START) {
            registers[7] = ppuDataBuffer;
            ppuDataBuffer = readVRAM(v, mmc);
        // If the v register is in the palette, then the read is immediate
        } else {
            registers[7] = readVRAM(v, mmc);
            // Subtract by 0x1000 to stay in the nametable:
            // https://forums.nesdev.org/viewtopic.php?f=3&t=18627
            ppuDataBuffer = readVRAM(v - 0x1000, mmc);
        }
        v += getPPUAddrInc();
    } else if (addr == OAMDMA) {
        return oamDMA;
    }
    return registers[localAddr];
}

// Handles register writes from the CPU

void PPU::writeRegister(uint16_t addr, uint8_t val, MMC& mmc, bool mute) {
    uint16_t localAddr = getLocalRegisterAddr(addr);
    switch (localAddr) {
        case 0:
            setTempNametableSelect(val & 3);
            if (isNMIEnabled() != (bool) (val & 0x80)) {
                op.nmiOccurred = false;
            }
            break;
        case 5:
            writePPUScroll(val);
            break;
        case 6:
            writePPUAddr(val);
            break;
        case 7:
            writeVRAM(v, val, mmc, mute);
            v += getPPUAddrInc();
    }

    if (addr == OAMDMA) {
        oamDMA = val;
    } else if (localAddr != 2) {
        registers[localAddr] = val;
    }
}

// Allows the CPU to write to the PPU's OAM

void PPU::writeOAM(uint8_t addr, uint8_t val) {
    oam[addr] = val;
}

// Tells the CPU when it should enter an NMI

bool PPU::isNMIActive(MMC& mmc, bool mute) {
    if (op.cycle == 1 && op.scanline == PRERENDER_LINE) {
        return false;
    }
    if (isNMIEnabled() && (isVblank() || (op.cycle == 1 && op.scanline == 241)) &&
            !op.nmiOccurred && !op.suppressNMI) {
        op.nmiOccurred = true;
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

void PPU::print(bool isCycleDone) const {
    unsigned int inc = 0;
    std::string time;
    if (isCycleDone) {
        time = "After";
    } else {
        time = "Before";
        ++inc;
        std::cout << "--------------------------------------------------\n";
    }
    std::cout << "Cycle " << totalCycles + inc << ": " << time
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
    unsigned int fineYScroll = getFineYScroll();
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

uint16_t PPU::getNametableAddr() const {
    unsigned int nametableBaseAddr = getNametableSelectAddr();
    unsigned int coarseXScroll = getCoarseXScroll();
    unsigned int coarseYScroll = getCoarseYScroll();
    return nametableBaseAddr + coarseXScroll + coarseYScroll * TILES_PER_ROW;
}

uint16_t PPU::getNametableSelectAddr() const {
    unsigned int nametableSelect = getNametableSelect();
    uint16_t nametableBaseAddr;
    switch (nametableSelect) {
        case 0:
            nametableBaseAddr = NAMETABLE0_START;
            break;
        case 1:
            nametableBaseAddr = NAMETABLE1_START;
            break;
        case 2:
            nametableBaseAddr = NAMETABLE2_START;
            break;
        case 3:
            nametableBaseAddr = NAMETABLE3_START;
    }
    return nametableBaseAddr;
}

void PPU::updateAttribute() {
    unsigned int nametableBaseAddr = getNametableSelectAddr();
    unsigned int xTile = (op.nametableAddr - nametableBaseAddr) % TILES_PER_ROW;
    unsigned int yTile = (op.nametableAddr - nametableBaseAddr) / TILES_PER_ROW;
    unsigned int xAttribute = xTile / 4;
    unsigned int yAttribute = yTile / 4;
    op.attributeAddr = nametableBaseAddr + ATTRIBUTE_OFFSET + xAttribute + yAttribute * 8;

    bool right = false;
    bool bottom = false;
    if (op.nametableAddr & 2) {
        right = true;
    }
    if (yTile & 2) {
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
    unsigned int spriteHeight = getSpriteHeight();
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
    unsigned int renderLine = op.getRenderLine();
    unsigned int tileRowIndex = sprite.getTileRowIndex(renderLine, spriteHeight);
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
    memset(secondaryOAM, 0xff, SECONDARY_OAM_SIZE);
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
    unsigned int renderLine = op.getRenderLine();
    unsigned int spriteHeight = getSpriteHeight();
    unsigned int foundSpriteNum = op.nextSprites.size();
    switch (op.oamEntryNum) {
        case 0:
            // If the sprite is in range of the current scanline, add it to the list and start
            // collecting the remaining data for it. Otherwise, move on to the next sprite
            if (sprite.isYInRange(renderLine, spriteHeight)) {
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
    uint8_t bgPalette = op.getPalette(x);
    uint8_t bgPaletteLower = bgPalette & 3;
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
    // The palette is used as an index for retrieving the palette entry, which is associated with
    // RGB values
    if (spriteChosen && areSpritesShown() && (op.pixel > 7 || areSpritesLeftColShown())) {
        // Output the sprite pixel
        paletteEntry = readVRAM(SPRITE_PALETTE_START + spritePalette, mmc);
        setSprite0Hit(*spriteIterator, bgPalette);
    } else if (isBGShown() && (op.pixel > 7 || isBGLeftColShown())) {
        // Output the background pixel
        paletteEntry = readVRAM(IMAGE_PALETTE_START + bgPalette, mmc);
        if (foundSprite && areSpritesShown()) {
            setSprite0Hit(*spriteIterator, bgPalette);
        }
    } else {
        // Output the universal background color:
        // https://www.nesdev.org/wiki/PPU_palettes#Memory_Map
        paletteEntry = readVRAM(IMAGE_PALETTE_START, mmc);
    }
    setRGB(paletteEntry);
}

// Sets the sprite 0 hit flag in the PPUSTATUS register:
// https://www.nesdev.org/wiki/PPU_OAM#Sprite_zero_hits

void PPU::setSprite0Hit(const Sprite& sprite, uint8_t bgPalette) {
    if (sprite.spriteNum == 0 && bgPalette && isBGShown() && op.pixel != 255 &&
            (op.pixel > 7 || (isBGLeftColShown() && areSpritesLeftColShown()))) {
        registers[2] |= 0x40;
    }
}

// Sets the current pixel's RGB values in the frame

void PPU::setRGB(uint8_t paletteEntry) {
    unsigned int renderLine = op.getRenderLine();
    // Each 4 bytes in the frame is a pixel. The first byte is the blue value, the second is the
    // green value, the third is the red value, and the fourth is the opacity
    unsigned int blueIndex = (op.pixel + renderLine * FRAME_WIDTH) * 4;
    unsigned int greenIndex = blueIndex + 1;
    unsigned int redIndex = blueIndex + 2;
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
        std::memcpy(lockedPixels, frame, FRAME_SIZE);
        SDL_UnlockTexture(texture);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }
}

void PPU::updateScroll() {
    if (op.status == PPUOp::FetchPatternEntryHi && op.canFetch()) {
        incrementCoarseXScroll();
    }

    if (op.cycle == 256 && op.scanline <= LAST_RENDER_LINE) {
        incrementYScroll();
    } else if (op.cycle == 257 && (op.scanline <= LAST_RENDER_LINE ||
            op.scanline == PRERENDER_LINE)) {
        setCoarseXScroll(getTempCoarseXScroll());
        resetHorizontalNametable();
    } else if (op.cycle >= 280 && op.cycle <= 304 && op.scanline == PRERENDER_LINE) {
        setCoarseYScroll(getTempCoarseYScroll());
        setFineYScroll(getTempFineYScroll());
        setNametableSelect(getTempNametableSelect());
    }
}

// Increments the coarse X scroll in the v register. Credit to the NESdev wiki for this function:
// https://www.nesdev.org/wiki/PPU_scrolling#Coarse_X_increment

void PPU::incrementCoarseXScroll() {
    unsigned int coarseXScroll = getCoarseXScroll();
    if (coarseXScroll == 31) {
        setCoarseXScroll(0);
        switchHorizontalNametable();
    } else {
        setCoarseXScroll(coarseXScroll + 1);
    }
}

// Increments the fine Y scroll and coarse Y scroll in the v register. Credit to the NESdev wiki for
// this function: https://www.nesdev.org/wiki/PPU_scrolling#Y_increment

void PPU::incrementYScroll() {
    unsigned int fineYScroll = getFineYScroll();
    if (fineYScroll < 7) {
        setFineYScroll(fineYScroll + 1);
    } else {
        setFineYScroll(0);

        unsigned int coarseYScroll = getCoarseYScroll();
        if (coarseYScroll == 29) {
            setCoarseYScroll(0);
            switchVerticalNametable();
        } else if (coarseYScroll == 31) {
            setCoarseYScroll(0);
        } else {
            setCoarseYScroll(coarseYScroll + 1);
        }
    }
}

void PPU::switchHorizontalNametable() {
    unsigned int nametableSelect = getNametableSelect();
    if (nametableSelect == 0 || nametableSelect == 2) {
        setNametableSelect(nametableSelect + 1);
    } else {
        setNametableSelect(nametableSelect - 1);
    }
}

void PPU::switchVerticalNametable() {
    unsigned int nametableSelect = getNametableSelect();
    if (nametableSelect == 0 || nametableSelect == 1) {
        setNametableSelect(nametableSelect + 2);
    } else {
        setNametableSelect(nametableSelect - 2);
    }
}

void PPU::resetHorizontalNametable() {
    unsigned int nametableSelect = getNametableSelect();
    unsigned int tempNametableSelect = getTempNametableSelect();
    if (nametableSelect >= 2 && tempNametableSelect <= 1) {
        setNametableSelect(tempNametableSelect + 2);
    } else if (nametableSelect <= 1 && tempNametableSelect >= 2) {
        setNametableSelect(tempNametableSelect - 2);
    } else {
        setNametableSelect(tempNametableSelect);
    }
}

void PPU::updatePPUStatus(MMC& mmc) {
    if (op.scanline == 241 && !op.suppressNMI) {
        registers[2] |= 0x80;
    } else if (op.scanline == PRERENDER_LINE) {
        registers[2] &= 0x3f;
    }
}

void PPU::writePPUScroll(uint8_t val) {
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

void PPU::writePPUAddr(uint8_t val) {
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

uint8_t PPU::readVRAM(uint16_t addr, MMC& mmc) const {
    uint16_t upperMirrorAddr = getUpperMirrorAddr(addr);
    addr = getLocalVRAMAddr(addr, mmc, true);
    // Since addresses $0000 - $1fff are in the CHR memory on the cartridge and not the VRAM,
    // getLocalVRAMAddr subtracts its resulting address by 0x2000. To identify whether the address
    // is for CHR memory or VRAM, the mirrored address in the range $0000 - $3fff is checked to see
    // if it lies within $0000 - $1fff
    if (upperMirrorAddr < NAMETABLE0_START) {
        return mmc.readCHR(upperMirrorAddr);
    }
    return vram[addr];
}

void PPU::writeVRAM(uint16_t addr, uint8_t val, MMC& mmc, bool mute) {
    if (!mute) {
        std::cout << std::hex << "0x" << (unsigned int) val << " has been written to the VRAM "
            "address 0x" << (unsigned int) addr <<
            "\n--------------------------------------------------\n" << std::dec;
    }

    uint16_t upperMirrorAddr = getUpperMirrorAddr(addr);
    addr = getLocalVRAMAddr(addr, mmc, false);
    // Since addresses $0000 - $1fff are in the CHR memory on the cartridge and not the VRAM,
    // getLocalVRAMAddr subtracts its resulting address by 0x2000. To identify whether the address
    // is for CHR memory or VRAM, the mirrored address in the range $0000 - $3fff is checked to see
    // if it lies within $0000 - $1fff
    if (upperMirrorAddr < NAMETABLE0_START) {
        mmc.writeCHR(upperMirrorAddr, val);
    } else {
        vram[addr] = val;
    }
}

uint16_t PPU::getLocalRegisterAddr(uint16_t addr) const {
    // Addresses $2008 - $3fff in the CPU memory map are mirrors of $2000 - $2007, so only the lower
    // 7 bits are needed
    return addr & 7;
}

uint16_t PPU::getLocalVRAMAddr(uint16_t addr, MMC& mmc, bool isRead) const {
    addr = getUpperMirrorAddr(addr);
    if (addr >= IMAGE_PALETTE_START) {
        addr &= 0x3f1f;
        if (addr % 4 == 0 && isRead) {
            addr = IMAGE_PALETTE_START;
        } else if (addr == 0x3f10 && !isRead) {
            addr = IMAGE_PALETTE_START;
        }
        addr -= 0x1700;
    } else if (addr >= NAMETABLE0_START) {
        if (addr >= 0x3000) {
            addr &= 0x2fff;
        }

        switch (mmc.getMirroring()) {
            case MMC::Horizontal:
                addr = getHorizontalMirrorAddr(addr);
                break;
            case MMC::Vertical:
                addr = getVerticalMirrorAddr(addr);
                break;
            case MMC::SingleScreenLowerBank:
                addr = getSingleLowerMirrorAddr(addr);
                break;
            case MMC::SingleScreenUpperBank:
                addr = getSingleUpperMirrorAddr(addr);
                break;
            case MMC::FourScreen:
                addr = getFourScreenMirrorAddr(addr);
        }
    }
    return addr - 0x2000;
}

// The upper addresses $4000 - $ffff are mirrors of $0000 - $3fff, so this function mirrors the
// address to stay in the latter range

uint16_t PPU::getUpperMirrorAddr(uint16_t addr) const {
    return addr & 0x3fff;
}

uint16_t PPU::getHorizontalMirrorAddr(uint16_t addr) const {
    if (addr >= NAMETABLE3_START) {
        addr -= 0x800;
    } else if (addr >= NAMETABLE2_START) {
        addr -= 0x400;
    } else if (addr >= NAMETABLE1_START) {
        addr -= 0x400;
    }
    return addr;
}

uint16_t PPU::getVerticalMirrorAddr(uint16_t addr) const {
    if (addr >= NAMETABLE3_START) {
        addr -= 0x800;
    } else if (addr >= NAMETABLE2_START) {
        addr -= 0x800;
    }
    return addr;
}

uint16_t PPU::getSingleLowerMirrorAddr(uint16_t addr) const {
    if (addr >= NAMETABLE3_START) {
        addr -= 0xc00;
    } else if (addr >= NAMETABLE2_START) {
        addr -= 0x800;
    } else if (addr >= NAMETABLE1_START) {
        addr -= 0x400;
    }
    return addr;
}

uint16_t PPU::getSingleUpperMirrorAddr(uint16_t addr) const {
    if (addr >= NAMETABLE3_START) {
        addr -= 0x800;
    } else if (addr >= NAMETABLE2_START) {
        addr -= 0x400;
    } else if (addr < NAMETABLE1_START) {
        addr += 0x400;
    }
    return addr;
}

uint16_t PPU::getFourScreenMirrorAddr(uint16_t addr) const {
    std::cerr << "Four-screen mirroring is not implemented\n";
    exit(1);
    return 0;
}

uint16_t PPU::getNametableBaseAddr() const {
    uint8_t flag = registers[0] & 3;
    switch (flag) {
        case 0:
            return NAMETABLE0_START;
        case 1:
            return NAMETABLE1_START;
        case 2:
            return NAMETABLE2_START;
    }
    return NAMETABLE3_START;
}

unsigned int PPU::getPPUAddrInc() const {
    uint8_t flag = registers[0] & 4;
    if (flag) {
        return 0x20;
    }
    return 1;
}

uint16_t PPU::getSpritePatternAddr() const {
    uint8_t flag = registers[0] & 8;
    if (flag) {
        return 0x1000;
    }
    return 0;
}

uint16_t PPU::getBGPatternAddr() const {
    uint8_t flag = registers[0] & 0x10;
    if (flag) {
        return 0x1000;
    }
    return 0;
}

unsigned int PPU::getSpriteHeight() const {
    uint8_t flag = registers[0] & 0x20;
    if (flag) {
        return 16;
    }
    return 8;
}

bool PPU::isColorOutputOnEXT() const {
    return registers[0] & 0x40;
}

bool PPU::isNMIEnabled() const {
    return registers[0] & 0x80;
}

bool PPU::isGrayscale() const {
    return registers[1] & 1;
}

bool PPU::isBGLeftColShown() const {
    return registers[1] & 2;
}

bool PPU::areSpritesLeftColShown() const {
    return registers[1] & 4;
}

bool PPU::isBGShown() const {
    return registers[1] & 8;
}

bool PPU::areSpritesShown() const {
    return registers[1] & 0x10;
}

bool PPU::isRenderingEnabled() const {
    return isBGShown() || areSpritesShown();
}

bool PPU::isRedEmphasized() const {
    return registers[1] & 0x20;
}

bool PPU::isGreenEmphasized() const {
    return registers[1] & 0x40;
}

bool PPU::isBlueEmphasized() const {
    return registers[1] & 0x80;
}

bool PPU::isSpriteOverflow() const {
    return registers[2] & 0x20;
}

bool PPU::isSpriteZeroHit() const {
    return registers[2] & 0x40;
}

bool PPU::isVblank() const {
    return registers[2] & 0x80;
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

void PPU::setCoarseXScroll(unsigned int val) {
    v &= 0x7fe0;
    v |= val & 0x1f;
}

void PPU::setTempCoarseXScroll(unsigned int val) {
    t &= 0x7fe0;
    t |= val & 0x1f;
}

void PPU::setCoarseYScroll(unsigned int val) {
    v &= 0x7c1f;
    v |= (val & 0x1f) << 5;
}

void PPU::setTempCoarseYScroll(unsigned int val) {
    t &= 0x7c1f;
    t |= (val & 0x1f) << 5;
}

void PPU::setNametableSelect(unsigned int val) {
    v &= 0x73ff;
    v |= (val & 3) << 10;
}

void PPU::setTempNametableSelect(unsigned int val) {
    t &= 0x73ff;
    t |= (val & 3) << 10;
}

void PPU::setFineYScroll(unsigned int val) {
    v &= 0xfff;
    v |= (val & 7) << 12;
}

void PPU::setTempFineYScroll(unsigned int val) {
    t &= 0xfff;
    t |= (val & 7) << 12;
}

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