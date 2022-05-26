#include "ppu.h"

// Public Member Functions

PPU::PPU() :
        oamDMA(0),
        ppuAddr(0),
        ppuDataBuffer(0),
        writeLoAddr(false),
        mirroring(0),
        totalCycles(0),
        startTime(SDL_GetTicks64()),
        stopTime(SDL_GetTicks64()) {
    vram[UNIVERSAL_BG_INDEX] = BLACK;
    initializePalette();
}

void PPU::clear() {
    memset(registers, 0, PPU_REGISTER_SIZE);
    oamDMA = 0;
    memset(vram, 0, VRAM_SIZE);
    vram[UNIVERSAL_BG_INDEX] = BLACK;
    memset(oam, 0, OAM_SIZE);
    memset(secondaryOAM, 0, SECONDARY_OAM_SIZE);
    memset(frame, 0, FRAME_SIZE);
    op.clear();
    ppuAddr = 0;
    ppuDataBuffer = 0;
    writeLoAddr = false;
    mirroring = 0;
    totalCycles = 0;
    startTime = SDL_GetTicks64();
    stopTime = SDL_GetTicks64();
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
    // Vblank is only updated on cycle 1
    if (op.cycle == 1) {
        updateVblank(mmc);
    }
    op.prepNextCycle(registers[2]);
    ++totalCycles;
}

// Handles I/O reads from the CPU

uint8_t PPU::readIO(uint16_t addr, MMC& mmc, bool mute) {
    uint8_t val = readRegister(addr, mmc);
    if (addr == PPUSTATUS) {
        uint8_t clearedVblank = val & 0x7f;
        writeRegister(PPUSTATUS, clearedVblank, mmc, mute);
    }
    return val;
}

// Handles I/O writes from the CPU

void PPU::writeIO(uint16_t addr, uint8_t val, MMC& mmc, bool mute) {
    if (addr == PPUSTATUS) {
        return;
    }
    writeRegister(addr, val, mmc, mute);
}

// Allows the CPU to write to the PPU's OAM

void PPU::writeOAM(uint8_t addr, uint8_t val) {
    oam[addr] = val;
}

// Tells the CPU when it should enter an NMI

bool PPU::isNMIActive(MMC& mmc, bool mute) {
    if (isNMIEnabled() && isVblank()) {
        readIO(PPUSTATUS, mmc, mute);
        return true;
    }
    return false;
}

unsigned int PPU::getTotalCycles() const {
    return totalCycles;
}

void PPU::setMirroring(unsigned int mirroring) {
    this->mirroring = mirroring;
}

void PPU::clearTotalCycles() {
    totalCycles = 0;
}

void PPU::print(bool isCycleDone, bool mute) const {
    if (mute) {
        return;
    }

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
        "ppuAddr           = 0x" << (unsigned int) ppuAddr << "\n"
        "ppuDataBuffer     = 0x" << (unsigned int) ppuDataBuffer << "\n"
        "writeLoAddr       = " << std::dec << writeLoAddr << "\n"
        "mirroring         = " << mirroring << "\n"
        "totalCycles       = " << totalCycles <<
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
    unsigned int renderLine = op.getRenderLine();
    uint16_t addr = 0;
    switch (op.status) {
        case PPUOp::FetchNametableEntry:
            op.nametableEntry = readVRAM(op.nametableAddr, mmc);
            break;
        case PPUOp::FetchAttributeEntry:
            op.attributeEntry = readVRAM(op.attributeAddr, mmc);
            break;
        case PPUOp::FetchPatternEntryLo:
            // Nametable entries are tile numbers used to index into the pattern tables. Each 8x8
            // tile takes up 0x10 entries in the pattern table. The render line mod 8 represents the
            // tile row. The background pattern address is the default base address set by the CPU
            addr = op.nametableEntry * 0x10 + (renderLine % 8) + getBGPatternAddr();
            op.patternEntryLo = readVRAM(addr, mmc);
            break;
        case PPUOp::FetchPatternEntryHi:
            // Each tile row has a low pattern entry and a high pattern entry. The low entries are
            // listed first (0 - 7) then the high entries are listed (8 - 0xf). E.g., the first tile
            // row would be located at 0 and 8, the second would be 1 and 9, etc.
            addr = op.nametableEntry * 0x10 + (renderLine % 8) + getBGPatternAddr() + 8;
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
    uint8_t bgPalette = op.getPalette();
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
    unsigned int blueIndex = (op.pixel + renderLine * FRAME_WIDTH) * 4;
    unsigned int greenIndex = blueIndex + 1;
    unsigned int redIndex = blueIndex + 2;
    frame[redIndex] = palette[paletteEntry].red;
    frame[greenIndex] = palette[paletteEntry].green;
    frame[blueIndex] = palette[paletteEntry].blue;
    frame[blueIndex + 3] = SDL_ALPHA_OPAQUE;
}

// Renders a frame 60 times a second via SDL

void PPU::renderFrame(SDL_Renderer* renderer, SDL_Texture* texture) {
    if (renderer != nullptr && texture != nullptr) {
        stopTime = SDL_GetTicks64();
        Uint64 duration = stopTime - startTime;
        // If a 60th of a second hasn't passed since the last rendered frame, wait until it has
        if (duration < SIXTIETH_OF_A_SECOND) {
            SDL_Delay(SIXTIETH_OF_A_SECOND - duration);
        }
        startTime = SDL_GetTicks64();
        stopTime = SDL_GetTicks64();

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

// Sets the Vblank flag in the PPUSTATUS register

void PPU::updateVblank(MMC& mmc) {
    if (op.scanline == 241 && !isVblank()) {
        registers[2] |= 0x80;
    } else if (op.scanline == PRERENDER_LINE && isVblank()) {
        registers[2] &= 0x7f;
    }
}

uint8_t PPU::readRegister(uint16_t addr, MMC& mmc) {
    if (addr == OAMDMA) {
        return oamDMA;
    }
    if (addr == PPUDATA) {
        if ((ppuAddr & 0x3fff) < IMAGE_PALETTE_START) {
            registers[7] = ppuDataBuffer;
        } else {
            registers[7] = readVRAM(addr, mmc);
        }
        ppuDataBuffer = readVRAM(ppuAddr, mmc);
        ppuAddr += getPPUAddrInc();
    }
    addr &= 7;
    return registers[addr];
}

void PPU::writeRegister(uint16_t addr, uint8_t val, MMC& mmc, bool mute) {
    if (addr == OAMDMA) {
        oamDMA = val;
    } else {
        uint16_t localAddr = addr & 7;
        registers[localAddr] = val;
    }

    if (addr != PPUSTATUS) {
        registers[2] &= 0xe0;
        registers[2] |= val & 0x1f;
    }

    if (addr == PPUADDR) {
        updatePPUAddr(val);
    } else if (addr == PPUDATA) {
        writeVRAM(ppuAddr, val, mmc, mute);
        ppuAddr += getPPUAddrInc();
    }
}

uint8_t PPU::readVRAM(uint16_t addr, MMC& mmc) const {
    addr &= 0x3fff;
    if (addr >= IMAGE_PALETTE_START) {
        addr &= 0x3f1f;
        if (addr % 4 == 0) {
            addr = IMAGE_PALETTE_START;
        }
        addr -= 0x1700;
    } else if (addr >= NAMETABLE0_START) {
        if (addr >= 0x3000) {
            addr &= 0x2fff;
        }

        switch (mirroring) {
            case PPU::Horizontal:
                addr = getHorizontalMirrorAddr(addr);
                break;
            case PPU::Vertical:
                addr = getVerticalMirrorAddr(addr);
                break;
            case PPU::SingleScreen:
                addr = getSingleScreenMirrorAddr(addr);
                break;
            case PPU::FourScreen:
                addr = getFourScreenMirrorAddr(addr);
        }
    }

    if (addr < NAMETABLE0_START) {
        return mmc.readCHR(addr);
    }
    addr -= 0x2000;
    return vram[addr];
}

void PPU::writeVRAM(uint16_t addr, uint8_t val, MMC& mmc, bool mute) {
    uint16_t localAddr = addr & 0x3fff;
    if (localAddr >= IMAGE_PALETTE_START) {
        localAddr &= 0x3f1f;
        if (localAddr == 0x3f10) {
            localAddr = IMAGE_PALETTE_START;
        }
        localAddr -= 0x1700;
    } else if (localAddr >= NAMETABLE0_START) {
        if (localAddr >= 0x3000) {
            localAddr &= 0x2fff;
        }

        switch (mirroring) {
            case PPU::Horizontal:
                localAddr = getHorizontalMirrorAddr(localAddr);
                break;
            case PPU::Vertical:
                localAddr = getVerticalMirrorAddr(localAddr);
                break;
            case PPU::SingleScreen:
                localAddr = getSingleScreenMirrorAddr(localAddr);
                break;
            case PPU::FourScreen:
                localAddr = getFourScreenMirrorAddr(localAddr);
        }
    }

    if (localAddr < NAMETABLE0_START) {
        mmc.writeCHR(localAddr, val);
    } else {
        localAddr -= 0x2000;
        vram[localAddr] = val;
    }

    if (!mute) {
        std::cout << std::hex << "0x" << (unsigned int) val << " has been written to the VRAM "
            "address 0x" << (unsigned int) addr <<
            "\n--------------------------------------------------\n" << std::dec;
    }
}

void PPU::updatePPUAddr(uint8_t val) {
    if (writeLoAddr) {
        ppuAddr &= 0xff00;
        ppuAddr |= val;
        writeLoAddr = false;
    } else {
        ppuAddr &= 0xff;
        ppuAddr |= val << 8;
        writeLoAddr = true;
    }
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

uint16_t PPU::getSingleScreenMirrorAddr(uint16_t addr) const {
    if (addr >= NAMETABLE3_START) {
        addr -= 0xc00;
    } else if (addr >= NAMETABLE2_START) {
        addr -= 0x800;
    } else if (addr >= NAMETABLE1_START) {
        addr -= 0x400;
    }
    return addr;
}

uint16_t PPU::getFourScreenMirrorAddr(uint16_t addr) const {
    // TODO: Implement CHR memory banks
    return getHorizontalMirrorAddr(addr);
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