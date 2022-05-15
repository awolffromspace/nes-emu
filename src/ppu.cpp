#include "ppu.h"

PPU::PPU() :
        oamDMA(0),
        ppuAddr(0),
        ppuDataBuffer(0),
        writeLoAddr(false),
        mirroring(0),
        totalCycles(0) {
    vram[UNIVERSAL_BG_INDEX] = BLACK;
    initializePalette();
}

void PPU::clear(bool mute) {
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

    if (!mute) {
        std::cout << "PPU was cleared\n";
    }
}

void PPU::step(MMC& mmc, SDL_Renderer* renderer, SDL_Texture* texture,
        bool mute) {
    if (op.scanline <= LAST_RENDER_LINE || op.scanline == PRERENDER_LINE) {
        fetch(mmc);
        addTileRow();
        clearSecondaryOAM();
        evaluateSprites(mmc);
        setPixel(mmc);

        if (op.scanline <= LAST_RENDER_LINE && op.cycle == 240 &&
                renderer != nullptr && texture != nullptr &&
                op.frameNum % 86 == 0) {
            SDL_RenderClear(renderer);
            uint8_t* lockedPixels = nullptr;
            int pitch = 0;
            SDL_LockTexture(texture, nullptr, (void**) &lockedPixels, &pitch);
            std::memcpy(lockedPixels, frame, FRAME_SIZE);
            SDL_UnlockTexture(texture);
            // SDL_UpdateTexture(texture, nullptr, frame, 256 * 4);
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
        }
    }
    updateFlags(mmc, mute);
    prepNextCycle(mmc);
    ++totalCycles;
}

uint8_t PPU::readIO(uint16_t addr, MMC& mmc, bool mute) {
    uint8_t val = readRegister(addr, mmc);
    if (addr == PPUSTATUS) {
        uint8_t clearedVblank = val & 0x7f;
        writeRegister(PPUSTATUS, clearedVblank, mmc, mute);
    }
    return val;
}

void PPU::writeIO(uint16_t addr, uint8_t val, MMC& mmc, bool mute) {
    if (addr == PPUSTATUS) {
        return;
    }
    writeRegister(addr, val, mmc, mute);
}

void PPU::writeOAM(uint8_t addr, uint8_t val) {
    oam[addr] = val;
}

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
        << std::hex <<
        "ppuAddr           = " << (unsigned int) ppuAddr << "\n"
        "ppuDataBuffer     = " << (unsigned int) ppuDataBuffer << "\n"
        << std::dec <<
        "writeLoAddr       = " << (bool) writeLoAddr << "\n"
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
        << std::dec <<
        "scanline          = " << op.scanline << "\n"
        "pixel             = " << op.pixel << "\n"
        "attributeQuadrant = " << op.attributeQuadrant << "\n"
        "oddFrame          = " << (bool) op.oddFrame << "\n"
        "frameNum          = " << op.frameNum << "\n"
        "cycle             = " << op.cycle << "\n"
        "delayedFirstCycle = " << (bool) op.delayedFirstCycle << "\n"
        "status            = " << op.status <<
        "\n--------------------------------------------------\n";
}

void PPU::fetch(MMC& mmc) {
    if (op.cycle % 2 == 1) {
        return;
    }

    unsigned int renderLine = getRenderLine();
    uint16_t addr = 0;
    switch (op.status) {
        case PPUOp::FetchNametableEntry:
            op.nametableEntry = readVRAM(op.nametableAddr, mmc);
            break;
        case PPUOp::FetchAttributeEntry:
            op.attributeEntry = readVRAM(op.attributeAddr, mmc);
            break;
        case PPUOp::FetchPatternEntryLo:
            addr = op.nametableEntry * 0x10 + (renderLine % 8) +
                getBGPatternAddr();
            op.patternEntryLo = readVRAM(addr, mmc);
            break;
        case PPUOp::FetchPatternEntryHi:
            addr = op.nametableEntry * 0x10 + (renderLine % 8) +
                getBGPatternAddr() + 8;
            op.patternEntryHi = readVRAM(addr, mmc);
            break;
        case PPUOp::FetchSpriteEntryLo:
            fetchSpriteEntry(mmc);
            break;
        case PPUOp::FetchSpriteEntryHi:
            fetchSpriteEntry(mmc);
    }
}

void PPU::fetchSpriteEntry(MMC& mmc) {
    if (op.spriteNum >= op.nextSprites.size()) {
        return;
    }

    uint16_t spritePatternAddr = getSpritePatternAddr();
    unsigned int spriteHeight = getSpriteHeight();
    uint8_t tileIndexNum = op.nextSprites[op.spriteNum].tileIndexNum;
    unsigned int patternEntriesPerSprite = 0x10;
    if (spriteHeight == 16) {
        if (tileIndexNum & 1) {
            spritePatternAddr = 0x1000;
        } else {
            spritePatternAddr = 0;
        }
        tileIndexNum = tileIndexNum >> 1;
        patternEntriesPerSprite = 0x20;
    }

    unsigned int yPos = op.nextSprites[op.spriteNum].yPos;
    unsigned int spriteRenderLineDiff = getSpriteRenderLineDiff(yPos);
    uint16_t addr = spritePatternAddr + tileIndexNum * patternEntriesPerSprite;
    if (isSpriteFlippedVertically(op.nextSprites[op.spriteNum].attributes)) {
        addr += 0x7;
        if (spriteHeight == 16) {
            addr += 0x10;
        }
        if (spriteRenderLineDiff >= 8) {
            addr -= 0x10;
            spriteRenderLineDiff -= 8;
        }
        addr -= spriteRenderLineDiff;
    } else {
        if (spriteRenderLineDiff >= 8) {
            addr += 0x10;
            spriteRenderLineDiff -= 8;
        }
        addr += spriteRenderLineDiff;
    }
    if (op.status == PPUOp::FetchSpriteEntryLo) {
        op.nextSprites[op.spriteNum].patternEntryLo = readVRAM(addr, mmc);
    } else {
        addr += 0x8;
        op.nextSprites[op.spriteNum].patternEntryHi = readVRAM(addr, mmc);
    }
}

void PPU::addTileRow() {
    if (op.cycle % 2 == 1 || op.status != PPUOp::FetchPatternEntryHi ||
            !isValidFetch()) {
        return;
    }

    op.tileRows.push({op.nametableEntry, op.attributeEntry, op.patternEntryLo,
        op.patternEntryHi, op.attributeQuadrant});
}

void PPU::clearSecondaryOAM() {
    if (op.cycle == 64 && op.scanline <= LAST_RENDER_LINE) {
        memset(secondaryOAM, 0xff, SECONDARY_OAM_SIZE);
    }
}

void PPU::evaluateSprites(MMC& mmc) {
    if (op.cycle < 65 || op.cycle > 256 || op.scanline > LAST_RENDER_LINE) {
        return;
    }
    if (op.spriteNum >= 64) {
        return;
    }
    if (op.nextSprites.size() >= 8 && op.oamEntryNum == 0) {
        return;
    }
    if (!isBGShown() && !areSpritesShown()) {
        return;
    }

    if (op.cycle % 2) {
        op.oamEntry = oam[op.spriteNum * 4 + op.oamEntryNum];
    } else {
        if (op.oamEntryNum == 0 && isSpriteYInRange(op.oamEntry)) {
            unsigned int foundSpriteNum = op.nextSprites.size();
            secondaryOAM[foundSpriteNum * 4 + op.oamEntryNum] = op.oamEntry;
            op.nextSprites.push_back({op.oamEntry, 0, 0, 0, 0, 0});
            ++op.oamEntryNum;
        } else if (op.oamEntryNum == 1) {
            unsigned int foundSpriteNum = op.nextSprites.size() - 1;
            secondaryOAM[foundSpriteNum * 4 + op.oamEntryNum] = op.oamEntry;
            op.nextSprites.back().tileIndexNum = op.oamEntry;
            ++op.oamEntryNum;
        } else if (op.oamEntryNum == 2) {
            unsigned int foundSpriteNum = op.nextSprites.size() - 1;
            secondaryOAM[foundSpriteNum * 4 + op.oamEntryNum] = op.oamEntry;
            op.nextSprites.back().attributes = op.oamEntry;
            ++op.oamEntryNum;
        } else if (op.oamEntryNum == 3) {
            unsigned int foundSpriteNum = op.nextSprites.size() - 1;
            secondaryOAM[foundSpriteNum * 4 + op.oamEntryNum] = op.oamEntry;
            op.nextSprites.back().xPos = op.oamEntry;
            op.oamEntryNum = 0;
            ++op.spriteNum;
        } else {
            ++op.spriteNum;
        }
    }
}

void PPU::setPixel(MMC& mmc) {
    if (!isRendering()) {
        return;
    }

    struct PPUOp::TileRow tileRow = op.tileRows.front();
    uint8_t upperPaletteBits = getPaletteFromAttribute();
    uint8_t bgPixel = 0;
    if (tileRow.patternEntryLo & (0x80 >> (op.pixel % 8))) {
        bgPixel |= 1;
    }
    if (tileRow.patternEntryHi & (0x80 >> (op.pixel % 8))) {
        bgPixel |= 2;
    }
    if (upperPaletteBits & 1) {
        bgPixel |= 4;
    }
    if (upperPaletteBits & 2) {
        bgPixel |= 8;
    }

    uint8_t spritePixel = 0;
    struct PPUOp::Sprite currentSprite;
    unsigned int foundSpriteNum = 0;
    bool foundSprite = false;
    for (; foundSpriteNum < op.currentSprites.size(); ++foundSpriteNum) {
        currentSprite = op.currentSprites[foundSpriteNum];
        unsigned int spritePixelDiff = getSpritePixelDiff(currentSprite.xPos);
        if (isSpriteXInRange(currentSprite.xPos)) {
            uint8_t pixelBit = 0x80 >> spritePixelDiff;
            if (isSpriteFlippedHorizontally(currentSprite.attributes)) {
                pixelBit = 1 << spritePixelDiff;
            }
            if (currentSprite.patternEntryLo & pixelBit) {
                spritePixel |= 1;
            }
            if (currentSprite.patternEntryHi & pixelBit) {
                spritePixel |= 2;
            }
            if (spritePixel) {
                foundSprite = true;
                break;
            }
            spritePixel = 0;
        }
    }

    bool spriteChosen = false;
    if (foundSprite) {
        bool priority = isSpritePrioritized(currentSprite.attributes);
        if (bgPixel && spritePixel && priority) {
            spriteChosen = true;
        } else if (!bgPixel && spritePixel) {
            spriteChosen = true;
        }

        upperPaletteBits = getPaletteFromSpriteAttributes(
            currentSprite.attributes);
        if (upperPaletteBits & 1) {
            spritePixel |= 4;
        }
        if (upperPaletteBits & 2) {
            spritePixel |= 8;
        }
    }

    uint8_t paletteEntry = 0;
    if (spriteChosen && areSpritesShown() &&
            (areSpritesLeftColShown() || op.pixel > 7)) {
        paletteEntry = readVRAM(SPRITE_PALETTE_START + spritePixel, mmc);
    } else if (!spriteChosen && isBGShown() &&
            (isBGLeftColShown() || op.pixel > 7)) {
        paletteEntry = readVRAM(IMAGE_PALETTE_START + bgPixel, mmc);
    } else {
        paletteEntry = readVRAM(IMAGE_PALETTE_START, mmc);
    }
    setRGB(paletteEntry);
}

void PPU::setRGB(uint8_t paletteEntry) {
    unsigned int renderLine = getRenderLine();
    unsigned int blueIndex = (op.pixel + renderLine * FRAME_WIDTH) * 4;
    unsigned int greenIndex = blueIndex + 1;
    unsigned int redIndex = blueIndex + 2;
    frame[redIndex] = palette[paletteEntry].red;
    frame[greenIndex] = palette[paletteEntry].green;
    frame[blueIndex] = palette[paletteEntry].blue;
    frame[blueIndex + 3] = SDL_ALPHA_OPAQUE;
}

void PPU::updateFlags(MMC& mmc, bool mute) {
    if (op.cycle == 1 && op.scanline == 241 && !isVblank() &&
            !op.delayedFirstCycle) {
        uint8_t flags = readRegister(PPUSTATUS, mmc) | 0x80;
        writeRegister(PPUSTATUS, flags, mmc, mute);
    } else if (op.cycle == 1 && op.scanline == PRERENDER_LINE && isVblank() &&
            !op.delayedFirstCycle) {
        uint8_t flags = readRegister(PPUSTATUS, mmc) & 0x7f;
        writeRegister(PPUSTATUS, flags, mmc, mute);
    }
}

void PPU::prepNextCycle(MMC& mmc) {
    if (op.cycle % 2 == 0) {
        if (isValidFetch()) {
            updateNametableAddr(mmc);
            updateAttributeAddr();
        }
        if (op.status == PPUOp::FetchSpriteEntryHi) {
            ++op.spriteNum;
        }
        if ((op.scanline <= LAST_RENDER_LINE || op.scanline == PRERENDER_LINE) &&
                op.cycle < 337) {
            updateOpStatus();
        }
    }

    if (isRendering()) {
        ++op.pixel;
        if (op.pixel % 8 == 0) {
            op.tileRows.pop();
        }
        if (op.pixel == 256) {
            op.pixel = 0;
        }
    }

    if (op.cycle == 256) {
        op.spriteNum = 0;
    } else if (op.cycle == 320) {
        op.currentSprites = op.nextSprites;
        op.nextSprites.clear();
        op.spriteNum = 0;
        op.oamEntryNum = 0;
    }

    bool skipFirstCycle = op.oddFrame && (isBGShown() || areSpritesShown());
    if (op.scanline == PRERENDER_LINE && op.cycle == 340) {
        op.scanline = 0;
        op.oddFrame = !op.oddFrame;
        op.cycle = 1;
        op.delayedFirstCycle = false;
        ++op.frameNum;
    } else if (op.cycle == 340) {
        ++op.scanline;
        op.oddFrame = !op.oddFrame;
        op.cycle = 1;
        op.delayedFirstCycle = false;
        ++op.frameNum;
    } else if (op.cycle == 1 && !skipFirstCycle && !op.delayedFirstCycle) {
        op.delayedFirstCycle = true;
    } else {
        ++op.cycle;
    }

    if (op.frameNum == 4294967280) {
        op.frameNum = 0;
    }
}

void PPU::updateNametableAddr(MMC& mmc) {
    if (op.status == PPUOp::FetchPatternEntryHi) {
        ++op.nametableAddr;

        unsigned int renderLine = getRenderLine();
        if (op.cycle == 240 && (renderLine + 1) % 8 != 0) {
            op.nametableAddr -= 32;
        }
    }

    if (op.nametableAddr == ATTRIBUTE0_START) {
        op.nametableAddr = NAMETABLE0_START;
    }
}

void PPU::updateAttributeAddr() {
    if (op.status != PPUOp::FetchPatternEntryHi || op.nametableAddr % 2 == 1) {
        return;
    }

    unsigned int renderLine = getRenderLine();
    switch (op.attributeQuadrant) {
        case PPUOp::TopLeft:
            op.attributeQuadrant = PPUOp::TopRight;
            break;
        case PPUOp::TopRight:
            if (op.nametableAddr % 0x40 == 0 && (renderLine + 1) % 16 == 0) {
                op.attributeQuadrant = PPUOp::BottomLeft;
                op.attributeAddr &= 0xfff8;
            } else if (op.nametableAddr % 0x20 == 0) {
                op.attributeQuadrant = PPUOp::TopLeft;
                op.attributeAddr &= 0xfff8;
            } else {
                op.attributeQuadrant = PPUOp::TopLeft;
                ++op.attributeAddr;
            }
            break;
        case PPUOp::BottomLeft:
            op.attributeQuadrant = PPUOp::BottomRight;
            break;
        case PPUOp::BottomRight:
            if (op.nametableAddr == NAMETABLE0_START) {
                op.attributeQuadrant = PPUOp::TopLeft;
                op.attributeAddr = ATTRIBUTE0_START;
            } else if (op.nametableAddr % 0x80 == 0 &&
                    (renderLine + 1) % 32 == 0) {
                op.attributeQuadrant = PPUOp::TopLeft;
                ++op.attributeAddr;
            } else if (op.nametableAddr % 0x20 == 0) {
                op.attributeQuadrant = PPUOp::BottomLeft;
                op.attributeAddr &= 0xfff8;
            } else {
                op.attributeQuadrant = PPUOp::BottomLeft;
                ++op.attributeAddr;
            }
    }
}

void PPU::updateOpStatus() {
    if (op.status == PPUOp::FetchAttributeEntry) {
        if (op.cycle < 257 || op.cycle > 320) {
            op.status = PPUOp::FetchPatternEntryLo;
        } else {
            op.status = PPUOp::FetchSpriteEntryLo;
        }
    } else if (op.status == PPUOp::FetchPatternEntryHi ||
            op.status == PPUOp::FetchSpriteEntryHi) {
        op.status = PPUOp::FetchNametableEntry;
    } else {
        ++op.status;
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
        updatePPUAddr(val, mmc, mute);
    } else if (addr == PPUDATA) {
        writeVRAM(ppuAddr, val, mmc, mute);
        ppuAddr += getPPUAddrInc();
    }

    if (!mute) {
        std::cout << std::hex << "0x" << (unsigned int) val << " has " <<
            "been written to the address 0x" << (unsigned int) addr <<
            "\n--------------------------------------------------\n" <<
            std::dec;
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
                addr = getFourScreenMirrorAddr(addr, mmc);
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
        if (localAddr % 4 == 0) {
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
                localAddr = getFourScreenMirrorAddr(localAddr, mmc);
        }
    }

    if (localAddr < NAMETABLE0_START) {
        mmc.writeCHR(localAddr, val, mute);
    } else {
        localAddr -= 0x2000;
        vram[localAddr] = val;
    }

    if (!mute) {
        std::cout << std::hex << "0x" << (unsigned int) val << " has " <<
            "been written to the VRAM address 0x" << (unsigned int) addr <<
            "\n--------------------------------------------------\n" <<
            std::dec;
    }
}

void PPU::updatePPUAddr(uint8_t val, MMC& mmc, bool mute) {
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

uint16_t PPU::getFourScreenMirrorAddr(uint16_t addr, MMC& mmc) const {
    // TODO: Implement CHR memory banks
    return getHorizontalMirrorAddr(addr);
}

unsigned int PPU::getRenderLine() const {
    if (op.cycle <= 320) {
        return op.scanline;
    }
    if (op.scanline == PRERENDER_LINE) {
        return 0;
    } else {
        return op.scanline + 1;
    }
}

bool PPU::isRendering() const {
    if (op.scanline <= LAST_RENDER_LINE && op.cycle >= 4 && op.cycle <= 259) {
        return true;
    }
    return false;
}

bool PPU::isValidFetch() const {
    if (op.scanline < LAST_RENDER_LINE && (op.cycle < 241 || op.cycle > 320) &&
            op.cycle < 337) {
        return true;
    }
    if (op.scanline == LAST_RENDER_LINE && op.cycle < 241) {
        return true;
    }
    if (op.scanline == PRERENDER_LINE && op.cycle > 320 && op.cycle < 337) {
        return true;
    }
    return false;
}

uint8_t PPU::getPaletteFromAttribute() const {
    struct PPUOp::TileRow tileRow = op.tileRows.front();
    unsigned int shiftNum = 0;
    switch (tileRow.attributeQuadrant) {
        case PPUOp::TopRight:
            shiftNum = 2;
            break;
        case PPUOp::BottomLeft:
            shiftNum = 4;
            break;
        case PPUOp::BottomRight:
            shiftNum = 6;
    }
    return (tileRow.attributeEntry >> shiftNum) & 3;
}

unsigned int PPU::getSpriteRenderLineDiff(unsigned int yPos) const {
    unsigned int renderLine = getRenderLine();
    return renderLine - yPos;
}

bool PPU::isSpriteYInRange(unsigned int yPos) const {
    unsigned int spriteRenderLineDiff = getSpriteRenderLineDiff(yPos);
    unsigned int spriteHeight = getSpriteHeight();
    if (spriteRenderLineDiff <= spriteHeight - 1) {
        return true;
    }
    return false;
}

unsigned int PPU::getSpritePixelDiff(unsigned int xPos) const {
    return op.pixel - xPos;
}

bool PPU::isSpriteXInRange(unsigned int xPos) const {
    unsigned int spritePixelDiff = getSpritePixelDiff(xPos);
    if (spritePixelDiff <= 7) {
        return true;
    }
    return false;
}

uint8_t PPU::getPaletteFromSpriteAttributes(uint8_t spriteAttributes) const {
    return spriteAttributes & 3;
}

bool PPU::isSpritePrioritized(uint8_t spriteAttributes) const {
    return !(spriteAttributes & 0x20);
}

bool PPU::isSpriteFlippedHorizontally(uint8_t spriteAttributes) const {
    return spriteAttributes & 0x40;
}

bool PPU::isSpriteFlippedVertically(uint8_t spriteAttributes) const {
    return spriteAttributes & 0x80;
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