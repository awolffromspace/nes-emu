#include "ppu.h"

PPU::PPU() :
        oamDMA(0),
        ppuAddr(0),
        ppuDataBuffer(0),
        writeLoAddr(false),
        mirroring(0),
        totalCycles(0) {
    vram[0x3f00 - 0x3700] = 0xf;
    initializePalette();
    ctrl.nametableBaseAddr = 0x2000;
    ctrl.ppuAddrInc = 1;
    ctrl.spritePatternAddr = 0;
    ctrl.bgPatternAddr = 0;
    ctrl.spriteHeight = 8;
    ctrl.outputColorOnEXT = false;
    ctrl.nmiAtVblank = false;
    mask.grayscale = false;
    mask.showBGLeftCol = false;
    mask.showSpriteLeftCol = false;
    mask.showBG = false;
    mask.showSprites = false;
    mask.redEmphasis = false;
    mask.greenEmphasis = false;
    mask.blueEmphasis = false;
    status.spriteOverflow = false;
    status.spriteZeroHit = false;
    status.inVblank = false;
}

void PPU::clear(bool mute) {
    memset(registers, 0, 8);
    oamDMA = 0;
    memset(vram, 0, 0x820);
    vram[0x3f00 - 0x3700] = 0xf;
    memset(oam, 0, 0x100);
    memset(frame, 0, 256 * 240 * 4);
    op.clear();
    ctrl.nametableBaseAddr = 0x2000;
    ctrl.ppuAddrInc = 1;
    ctrl.spritePatternAddr = 0;
    ctrl.bgPatternAddr = 0;
    ctrl.spriteHeight = 8;
    ctrl.outputColorOnEXT = false;
    ctrl.nmiAtVblank = false;
    mask.grayscale = false;
    mask.showBGLeftCol = false;
    mask.showSpriteLeftCol = false;
    mask.showBG = false;
    mask.showSprites = false;
    mask.redEmphasis = false;
    mask.greenEmphasis = false;
    mask.blueEmphasis = false;
    status.spriteOverflow = false;
    status.spriteZeroHit = false;
    status.inVblank = false;
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
    if (op.scanline < 240 || op.scanline == 261) {
        fetch(mmc);
        getPixel(mmc);

        if (op.scanline < 240 && op.cycle == 240 && op.changedFrame &&
                renderer != nullptr && texture != nullptr) {
            SDL_RenderClear(renderer);
            uint8_t* lockedPixels = nullptr;
            int pitch = 0;
            SDL_LockTexture(texture, nullptr, (void**) &lockedPixels, &pitch);
            std::memcpy(lockedPixels, frame, 256 * 240 * 4);
            SDL_UnlockTexture(texture);
            // SDL_UpdateTexture(texture, nullptr, frame, 256 * 4);
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
            op.changedFrame = false;
        }
    }
    updateFlags(mmc, mute);
    prepNextCycle(mmc);
    ++totalCycles;
}

uint8_t PPU::readIO(uint16_t addr, MMC& mmc, bool mute) {
    uint8_t val = readRegister(addr, mmc);
    if (addr == 0x2002) {
        uint8_t clearedVblank = val & 0x7f;
        writeRegister(0x2002, clearedVblank, mmc, mute);
    }
    return val;
}

void PPU::writeIO(uint16_t addr, uint8_t val, MMC& mmc, bool mute) {
    if (addr == 0x2002) {
        return;
    }
    writeRegister(addr, val, mmc, mute);
}

void PPU::writeOAM(uint8_t addr, uint8_t val) {
    oam[addr] = val;
}

bool PPU::isNMIActive(MMC& mmc, bool mute) {
    if (ctrl.nmiAtVblank && status.inVblank) {
        readIO(0x2002, mmc, mute);
        return true;
    }
    return false;
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
        << std::hex << "\n------------------------\n"
        "PPU Fields\n------------------------\n"
        "registers[0]           = 0x" << (unsigned int) registers[0] << "\n"
        "registers[1]           = 0x" << (unsigned int) registers[1] << "\n"
        "registers[2]           = 0x" << (unsigned int) registers[2] << "\n"
        "registers[3]           = 0x" << (unsigned int) registers[3] << "\n"
        "registers[4]           = 0x" << (unsigned int) registers[4] << "\n"
        "registers[5]           = 0x" << (unsigned int) registers[5] << "\n"
        "registers[6]           = 0x" << (unsigned int) registers[6] << "\n"
        "registers[7]           = 0x" << (unsigned int) registers[7] << "\n"
        "oamDMA                 = 0x" << (unsigned int) oamDMA << "\n"
        "ctrl.nametableBaseAddr = 0x" << (unsigned int) ctrl.nametableBaseAddr
        << "\n"
        "ctrl.ppuAddrInc        = " << std::dec << ctrl.ppuAddrInc << "\n"
        "ctrl.spritePatternAddr = 0x" << std::hex <<
        (unsigned int) ctrl.spritePatternAddr << "\n"
        "ctrl.bgPatternAddr     = 0x" << (unsigned int) ctrl.bgPatternAddr <<
        "\n"
        "ctrl.spriteHeight      = " << std::dec << ctrl.spriteHeight << "\n"
        "ctrl.outputColorOnEXT  = " << (bool) ctrl.outputColorOnEXT << "\n"
        "ctrl.nmiAtVblank       = " << (bool) ctrl.nmiAtVblank << "\n"
        "mask.grayscale         = " << (bool) mask.grayscale << "\n"
        "mask.showBGLeftCol     = " << (bool) mask.showBGLeftCol << "\n"
        "mask.showSpriteLeftCol = " << (bool) mask.showSpriteLeftCol << "\n"
        "mask.showBG            = " << (bool) mask.showBG << "\n"
        "mask.showSprites       = " << (bool) mask.showSprites << "\n"
        "mask.redEmphasis       = " << (bool) mask.redEmphasis << "\n"
        "mask.greenEmphasis     = " << (bool) mask.greenEmphasis << "\n"
        "mask.blueEmphasis      = " << (bool) mask.blueEmphasis << "\n"
        "status.spriteOverflow  = " << (bool) status.spriteOverflow << "\n"
        "status.spriteOverflow  = " << (bool) status.spriteOverflow << "\n"
        "status.spriteZeroHit   = " << (bool) status.spriteZeroHit << "\n"
        "status.inVblank        = " << (bool) status.inVblank << "\n"
        "ppuAddr                = " << std::hex << (unsigned int) ppuAddr <<
        "\n"
        "ppuDataBuffer          = " << (unsigned int) ppuDataBuffer << "\n"
        "writeLoAddr            = " << std::dec << (bool) writeLoAddr << "\n"
        "mirroring              = " << mirroring << "\n"
        "totalCycles            = " << totalCycles <<
        "\n------------------------\n" << std::hex <<
        "PPU Operation Fields\n------------------------\n"
        "nametableAddr          = 0x" << (unsigned int) op.nametableAddr << "\n"
        "nametableEntry         = 0x" << (unsigned int) op.nametableEntry <<
        "\n"
        "attributeAddr          = 0x" << (unsigned int) op.attributeAddr << "\n"
        "attributeEntry         = 0x" << (unsigned int) op.attributeEntry <<
        "\n"
        "patternEntryLo         = 0x" << (unsigned int) op.patternEntryLo <<
        "\n"
        "patternEntryHi         = 0x" << (unsigned int) op.patternEntryHi <<
        "\n"
        "spriteEntryLo          = 0x" << (unsigned int) op.spriteEntryLo << "\n"
        "spriteEntryHi          = 0x" << (unsigned int) op.spriteEntryHi << "\n"
        << std::dec <<
        "scanline               = " << op.scanline << "\n"
        "pixel                  = " << op.pixel << "\n"
        "attributeQuadrant      = " << op.attributeQuadrant << "\n"
        "oddFrame               = " << (bool) op.oddFrame << "\n"
        "cycle                  = " << op.cycle << "\n"
        "delayedFirstCycle      = " << (bool) op.delayedFirstCycle << "\n"
        "status                 = " << op.status <<
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
                ctrl.bgPatternAddr;
            op.patternEntryLo = readVRAM(addr, mmc);
            break;
        case PPUOp::FetchPatternEntryHi:
            addr = op.nametableEntry * 0x10 + (renderLine % 8) +
                ctrl.bgPatternAddr + 8;
            op.patternEntryHi = readVRAM(addr, mmc);
            break;
        case PPUOp::FetchSpriteEntryLo:
            break;
        case PPUOp::FetchSpriteEntryHi:
            break;
    }
}

void PPU::getPixel(MMC& mmc) {
    if (op.cycle % 2 == 1 || op.status != PPUOp::FetchPatternEntryHi) {
        return;
    }
    if (op.cycle > 240 && op.cycle < 321) {
        return;
    }
    if (op.scanline == 239 && op.cycle > 320) {
        return;
    }
    if (op.scanline == 261 && op.cycle < 321) {
        return;
    }

    unsigned int eightPixels = op.pixel + 8;
    for (; op.pixel < eightPixels; ++op.pixel) {
        uint8_t colorBits = getColorBits();
        op.paletteEntry = 0;
        if (op.patternEntryLo & (0x80 >> (op.pixel % 8))) {
            op.paletteEntry |= 1;
        }
        if (op.patternEntryHi & (0x80 >> (op.pixel % 8))) {
            op.paletteEntry |= 2;
        }
        if (colorBits & 1) {
            op.paletteEntry |= 4;
        }
        if (colorBits & 2) {
            op.paletteEntry |= 8;
        }
        if (mask.showBG) {
            op.paletteEntry = readVRAM(0x3f00 + op.paletteEntry, mmc);
        } else {
            op.paletteEntry = readVRAM(0x3f00, mmc);
        }
        setRGB();
    }
}

void PPU::updateFlags(MMC& mmc, bool mute) {
    // This function is called twice if the first cycle is not skipped
    // Could refactor to prevent that
    if (op.cycle == 1 && op.scanline == 241 && !status.inVblank) {
        uint8_t flags = readRegister(0x2002, mmc) | 0x80;
        writeRegister(0x2002, flags, mmc, mute);
    } else if (op.cycle == 1 && op.scanline == 261 && status.inVblank) {
        uint8_t flags = readRegister(0x2002, mmc) & 0x7f;
        writeRegister(0x2002, flags, mmc, mute);
    }
}

void PPU::prepNextCycle(MMC& mmc) {
    if (op.cycle % 2 == 0) {
        if ((op.scanline < 239 && (op.cycle < 241 || op.cycle > 320) &&
                op.cycle < 337) ||
                (op.scanline == 239 && op.cycle < 241) ||
                (op.scanline == 261 && op.cycle > 320 && op.cycle < 337)) {
            updateNametableAddr(mmc);
            updateAttributeAddr();
        }
        if (op.cycle < 337 && (op.scanline < 240 || op.scanline == 261)) {
            updateOpStatus();
        }
    }

    if (op.cycle == 240) {
        op.pixel = 0;
    }

    bool skipFirstCycle = op.oddFrame && (mask.showBG || mask.showSprites);
    if (op.scanline == 261 && op.cycle == 340) {
        op.scanline = 0;
        op.oddFrame = !op.oddFrame;
        op.cycle = 1;
        op.delayedFirstCycle = false;
    } else if (op.cycle == 340) {
        ++op.scanline;
        op.oddFrame = !op.oddFrame;
        op.cycle = 1;
        op.delayedFirstCycle = false;
    } else if (op.cycle == 1 && !skipFirstCycle && !op.delayedFirstCycle) {
        op.delayedFirstCycle = true;
    } else {
        ++op.cycle;
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

    if (op.nametableAddr == ctrl.nametableBaseAddr + 0x3c0) {
        op.nametableAddr = ctrl.nametableBaseAddr;
    }
}

void PPU::updateAttributeAddr() {
    if (op.status != PPUOp::FetchPatternEntryHi) {
        return;
    }
    if (op.nametableAddr % 2 == 1) {
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
            if (op.nametableAddr == ctrl.nametableBaseAddr) {
                op.attributeQuadrant = PPUOp::TopLeft;
                op.attributeAddr = ctrl.nametableBaseAddr + 0x3c0;
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
    if (addr == 0x4014) {
        return oamDMA;
    }
    if (addr == 0x2007) {
        if ((ppuAddr & 0x3fff) < 0x3f00) {
            registers[7] = ppuDataBuffer;
        } else {
            registers[7] = readVRAM(addr, mmc);
        }
        ppuDataBuffer = readVRAM(ppuAddr, mmc);
        ppuAddr += ctrl.ppuAddrInc;
    }
    addr &= 7;
    return registers[addr];
}

void PPU::writeRegister(uint16_t addr, uint8_t val, MMC& mmc, bool mute) {
    if (addr == 0x4014) {
        oamDMA = val;
    } else {
        uint16_t localAddr = addr & 7;
        registers[localAddr] = val;
    }

    switch (addr) {
        case 0x2000:
            updateCtrl(val);
            break;
        case 0x2001:
            updateMask(val);
            break;
        case 0x2002:
            updateStatus(val, false);
            break;
        case 0x2006:
            updatePPUAddr(val, mmc, mute);
            break;
        case 0x2007:
            writeVRAM(ppuAddr, val, mmc, mute);
            ppuAddr += ctrl.ppuAddrInc;
    }

    if (addr != 0x2002) {
        updateStatus(val, true);
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
    if (addr >= 0x3f00) {
        addr &= 0x3f1f;
        if (addr % 4 == 0) {
            addr = 0x3f00;
        }
        addr -= 0x1700;
    } else if (addr >= 0x2000) {
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

    if (addr < 0x2000) {
        return mmc.readCHR(addr);
    }
    addr -= 0x2000;
    return vram[addr];
}

void PPU::writeVRAM(uint16_t addr, uint8_t val, MMC& mmc, bool mute) {
    uint16_t localAddr = addr & 0x3fff;
    if (localAddr >= 0x3f00) {
        localAddr &= 0x3f1f;
        if (localAddr % 4 == 0) {
            localAddr = 0x3f00;
        }
        localAddr -= 0x1700;
    } else if (localAddr >= 0x2000) {
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

    if (localAddr < 0x2000) {
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

void PPU::updateCtrl(uint8_t val) {
    uint8_t currentFlags = val & 0xfc;
    switch (currentFlags) {
        case 0:
            ctrl.nametableBaseAddr = 0x2000;
            break;
        case 1:
            ctrl.nametableBaseAddr = 0x2400;
            break;
        case 2:
            ctrl.nametableBaseAddr = 0x2800;
            break;
        case 3:
            ctrl.nametableBaseAddr = 0x2c00;
    }

    currentFlags = (val & 4) >> 2;
    switch (currentFlags) {
        case 0:
            ctrl.ppuAddrInc = 1;
            break;
        case 1:
            ctrl.ppuAddrInc = 0x20;
    }

    currentFlags = (val & 8) >> 3;
    switch (currentFlags) {
        case 0:
            ctrl.spritePatternAddr = 0;
            break;
        case 1:
            ctrl.spritePatternAddr = 0x1000;
    }

    currentFlags = (val & 0x10) >> 4;
    switch (currentFlags) {
        case 0:
            ctrl.bgPatternAddr = 0;
            break;
        case 1:
            ctrl.bgPatternAddr = 0x1000;
    }

    currentFlags = (val & 0x20) >> 5;
    switch (currentFlags) {
        case 0:
            ctrl.spriteHeight = 8;
            break;
        case 1:
            ctrl.spriteHeight = 16;
    }

    currentFlags = (val & 0x40) >> 6;
    switch (currentFlags) {
        case 0:
            ctrl.outputColorOnEXT = false;
            break;
        case 1:
            ctrl.outputColorOnEXT = true;
    }

    currentFlags = (val & 0x80) >> 7;
    switch (currentFlags) {
        case 0:
            ctrl.nmiAtVblank = false;
            break;
        case 1:
            ctrl.nmiAtVblank = true;
    }
}

void PPU::updateMask(uint8_t val) {
    uint8_t currentFlags = val & 0xfc;
    currentFlags = val & 1;
    switch (currentFlags) {
        case 0:
            mask.grayscale = false;
            break;
        case 1:
            mask.grayscale = true;
    }

    currentFlags = (val & 2) >> 1;
    switch (currentFlags) {
        case 0:
            mask.showBGLeftCol = false;
            break;
        case 1:
            mask.showBGLeftCol = true;
    }

    currentFlags = (val & 4) >> 2;
    switch (currentFlags) {
        case 0:
            mask.showSpriteLeftCol = false;
            break;
        case 1:
            mask.showSpriteLeftCol = true;
    }

    currentFlags = (val & 8) >> 3;
    switch (currentFlags) {
        case 0:
            mask.showBG = false;
            break;
        case 1:
            mask.showBG = true;
    }

    currentFlags = (val & 0x10) >> 4;
    switch (currentFlags) {
        case 0:
            mask.showSprites = false;
            break;
        case 1:
            mask.showSprites = true;
    }

    currentFlags = (val & 0x20) >> 5;
    switch (currentFlags) {
        case 0:
            mask.redEmphasis = false;
            break;
        case 1:
            mask.redEmphasis = true;
    }

    currentFlags = (val & 0x40) >> 6;
    switch (currentFlags) {
        case 0:
            mask.greenEmphasis = false;
            break;
        case 1:
            mask.greenEmphasis = true;
    }

    currentFlags = (val & 0x80) >> 7;
    switch (currentFlags) {
        case 0:
            mask.blueEmphasis = false;
            break;
        case 1:
            mask.blueEmphasis = true;
    }
}

void PPU::updateStatus(uint8_t val, bool updatePrevWritten) {
    if (updatePrevWritten) {
        registers[2] &= 0xe0;
        registers[2] |= val & 0x1f;
    } else {
        uint8_t currentFlags = (val & 0x20) >> 5;
        switch (currentFlags) {
            case 0:
                status.spriteOverflow = false;
                break;
            case 1:
                status.spriteOverflow = true;
        }

        currentFlags = (val & 0x40) >> 6;
        switch (currentFlags) {
            case 0:
                status.spriteZeroHit = false;
                break;
            case 1:
                status.spriteZeroHit = true;
        }

        currentFlags = (val & 0x80) >> 7;
        switch (currentFlags) {
            case 0:
                status.inVblank = false;
                break;
            case 1:
                status.inVblank = true;
        }
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
    if (addr >= 0x2c00) {
        addr -= 0x800;
    } else if (addr >= 0x2800) {
        addr -= 0x400;
    } else if (addr >= 0x2400) {
        addr -= 0x400;
    }
    return addr;
}

uint16_t PPU::getVerticalMirrorAddr(uint16_t addr) const {
    if (addr >= 0x2c00) {
        addr -= 0x800;
    } else if (addr >= 0x2800) {
        addr -= 0x800;
    }
    return addr;
}

uint16_t PPU::getSingleScreenMirrorAddr(uint16_t addr) const {
    if (addr >= 0x2c00) {
        addr -= 0xc00;
    } else if (addr >= 0x2800) {
        addr -= 0x800;
    } else if (addr >= 0x2400) {
        addr -= 0x400;
    }
    return addr;
}

uint16_t PPU::getFourScreenMirrorAddr(uint16_t addr, MMC& mmc) const {
    // TODO: Implement CHR memory banks
    return getHorizontalMirrorAddr(addr);
}

unsigned int PPU::getRenderLine() const {
    if (op.cycle < 320) {
        return op.scanline;
    }
    if (op.scanline == 261) {
        return 0;
    } else {
        return op.scanline + 1;
    }
}

uint8_t PPU::getColorBits() const {
    unsigned int shiftNum = 0;
    switch (op.attributeQuadrant) {
        case PPUOp::TopRight:
            shiftNum = 2;
            break;
        case PPUOp::BottomLeft:
            shiftNum = 4;
            break;
        case PPUOp::BottomRight:
            shiftNum = 6;
    }
    return (op.attributeEntry >> shiftNum) & 3;
}

void PPU::setRGB() {
    unsigned int renderLine = getRenderLine();
    unsigned int blueIndex = (op.pixel + renderLine * 256) * 4;
    unsigned int greenIndex = blueIndex + 1;
    unsigned int redIndex = blueIndex + 2;
    uint8_t redVal = palette[op.paletteEntry].red;
    uint8_t greenVal = palette[op.paletteEntry].green;
    uint8_t blueVal = palette[op.paletteEntry].blue;
    frame[blueIndex + 3] = SDL_ALPHA_OPAQUE;

    if (frame[redIndex] != redVal || frame[greenIndex] != greenVal ||
            frame[blueIndex] != blueVal) {
        frame[redIndex] = redVal;
        frame[greenIndex] = greenVal;
        frame[blueIndex] = blueVal;
        op.changedFrame = true;
    }
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