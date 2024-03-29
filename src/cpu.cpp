#include "cpu.h"

// Public Member Functions

CPU::CPU() :
        pc(0),
        sp(0xfd),
        a(0),
        x(0),
        y(0),
        p(UnusedFlag | Break | InterruptDisable),
        totalCycles(0),
        endOfProgram(false),
        haltAtBrk(false),
        mute(true) { }

void CPU::clear() {
    pc = 0;
    sp = 0xfd;
    a = 0;
    x = 0;
    y = 0;
    p = UnusedFlag | Break | InterruptDisable;
    op.clear(true, true);
    ram.clear();
    ppu.clear();
    apu.clear();
    io.clear();
    mmc.clear();
    totalCycles = 0;
    endOfProgram = false;
}

// Executes exactly one CPU cycle

void CPU::step(SDL_Renderer* renderer, SDL_Texture* texture) {
    // This if statement performs the 6502's pipelined fetch
    if (op.done || totalCycles == 0) {
        // Clear previous operation to set up the next operation. However, this doesn't clear
        // interrupt or OAM DMA transfer statuses because they are triggered in the previous
        // operation and must remain for the next operation in order for the interrupt or OAM DMA
        // transfer to occur as the next operation
        op.clear(false, false);
        op.pc = pc;

        // If any interrupts are flagged, start the interrupt prologue. Ignore IRQs if the Interrupt
        // Disable flag is set. Note that the BRK instruction bypasses this by setting
        // interruptPrologue in its function
        if ((op.irq && !areInterruptsDisabled()) || op.nmi || op.reset) {
            op.interruptPrologue = true;
        // Otherwise, read in the first byte of the instruction and start it as the next operation
        } else {
            op.inst = read(pc);
            op.opcode = op.inst;
        }
    }

    // Priority list of what to do next if there are multiple options
    if (op.oamDMATransfer) {
        oamDMATransfer();
    } else if (op.interruptPrologue && op.reset) {
        prepareReset();
    } else if (op.interruptPrologue && op.nmi) {
        prepareNMI();
    } else if (op.interruptPrologue && op.irq) {
        prepareIRQ();
    } else {
        // Use the opcode to look up in the addressing mode and opcode arrays to get the two
        // relevant functions
        (this->*addrModeArr[op.opcode])();
        (this->*opcodeArr[op.opcode])();
    }

    // PPU executes 3 cycles for every CPU cycle
    for (unsigned int i = 0; i < 3; ++i) {
        ppu.step(mmc, renderer, texture, mute);
    }

    ++op.cycle;
    ++totalCycles;
}

void CPU::readInInst(const std::string& filename) {
    const uint16_t lowerResetAddr = 0xfffc;
    const uint16_t upperResetAddr = 0xfffd;
    // Addresses $4020 - $ffff belong in the cartridge, so pass it off to the MMC
    mmc.readInInst(filename);
    pc = (read(upperResetAddr) << 8) | read(lowerResetAddr);
}

void CPU::readInINES(const std::string& filename) {
    // Addresses $4020 - $ffff belong in the cartridge, so pass it off to the MMC
    mmc.readInINES(filename);
    if (filename == "test/nestest/nestest.nes") {
        // Use this start PC for an automated run of nestest.nes
        pc = 0xc000;
    } else {
        const uint16_t lowerResetAddr = 0xfffc;
        const uint16_t upperResetAddr = 0xfffd;
        // In all other cases, initialize PC to the reset vector
        pc = (read(upperResetAddr) << 8) | read(lowerResetAddr);
    }
}

void CPU::updateButton(const SDL_Event& event) {
    io.updateButton(event);
}

// Compares the current registers and total cycles with a given CPU state

bool CPU::compareState(const struct CPU::State& state) const {
    // Ignore bits 4 and 5 of the P register because they're always set
    if (state.pc != pc || state.sp != sp || state.a != a || state.x != x || state.y != y ||
            (state.p & 0xcf) != (p & 0xcf) || state.totalCycles != totalCycles) {
        return false;
    }
    return true;
}

// Used for comparing instructions with the nestest.log. Each line in the log is during the first
// cycle of each instruction when the operands are usually unknown, so this function is specifically
// for grabbing the operands in advance to ensure that they match with the log

uint32_t CPU::getFutureInst() {
    uint8_t operandLo, operandHi;
    uint32_t inst = op.inst;
    switch (op.addrMode) {
        case 1:
            operandLo = read(pc);
            inst = (inst << 16) | (operandLo << 8);
            operandHi = read(pc + 1);
            inst |= operandHi;
            break;
        case 2:
            operandLo = read(pc);
            inst = (inst << 16) | (operandLo << 8);
            operandHi = read(pc + 1);
            inst |= operandHi;
            break;
        case 3:
            operandLo = read(pc);
            inst = (inst << 16) | (operandLo << 8);
            operandHi = read(pc + 1);
            inst |= operandHi;
            break;
        case 5:
            operandLo = read(pc);
            inst = (inst << 8) | operandLo;
            break;
        case 7:
            operandLo = read(pc);
            inst = (inst << 16) | (operandLo << 8);
            operandHi = read(pc + 1);
            inst |= operandHi;
            break;
        case 8:
            operandLo = read(pc);
            inst = (inst << 8) | operandLo;
            break;
        case 9:
            operandLo = read(pc);
            inst = (inst << 8) | operandLo;
            break;
        case 10:
            operandLo = read(pc);
            inst = (inst << 8) | operandLo;
            break;
        case 11:
            operandLo = read(pc);
            inst = (inst << 8) | operandLo;
            break;
        case 12:
            operandLo = read(pc);
            inst = (inst << 8) | operandLo;
            break;
        case 13:
            operandLo = read(pc);
            inst = (inst << 8) | operandLo;
    }
    return inst;
}

uint16_t CPU::getPC() const {
    return pc;
}

unsigned int CPU::getTotalCycles() const {
    return totalCycles;
}

bool CPU::isEndOfProgram() const {
    return endOfProgram;
}

bool CPU::isHaltAtBrk() const {
    return haltAtBrk;
}

unsigned int CPU::getOpCycles() const {
    return op.cycle;
}

uint8_t CPU::readRAM(const uint16_t addr) const {
    return ram.read(addr);
}

unsigned int CPU::getTotalPPUCycles() const {
    return ppu.getTotalCycles();
}

uint8_t CPU::readPRG(const uint16_t addr) const {
    return mmc.readPRG(addr);
}

void CPU::setHaltAtBrk(const bool h) {
    haltAtBrk = h;
}

void CPU::setMute(const bool m) {
    mute = m;
}

void CPU::clearTotalPPUCycles() {
    ppu.clearTotalCycles();
}

void CPU::print(const bool isCycleDone) const {
    if (mute) {
        return;
    }

    unsigned int inc = 0;
    std::string time;
    const std::bitset<8> binaryP(p);
    if (isCycleDone) {
        time = "After";
    } else {
        time = "Before";
        ++inc;
        std::cout << "--------------------------------------------------\n";
    }
    std::cout << "CPU Cycle " << totalCycles + inc << ": " << time << std::hex <<
        "\n----------------------------\nCPU Fields\n----------------------------\n"
        "pc                = 0x" << (unsigned int) pc << "\n"
        "sp                = 0x" << (unsigned int) sp << "\n"
        "a                 = 0x" << (unsigned int) a << "\n"
        "x                 = 0x" << (unsigned int) x << "\n"
        "y                 = 0x" << (unsigned int) y << "\n"
        "p                 = " << binaryP << "\n"
        "totalCycles       = " << std::dec << totalCycles << std::hex <<
        "\n----------------------------\nCPU Operation Fields\n----------------------------\n"
        "inst              = 0x" << (unsigned int) op.inst << "\n"
        "pc                = 0x" << (unsigned int) op.pc << "\n"
        "opcode            = 0x" << (unsigned int) op.opcode << "\n"
        "operandLo         = 0x" << (unsigned int) op.operandLo << "\n"
        "operandHi         = 0x" << (unsigned int) op.operandHi << "\n"
        "val               = 0x" << (unsigned int) op.val << "\n"
        "tempAddr          = 0x" << (unsigned int) op.tempAddr << "\n"
        "fixedAddr         = 0x" << (unsigned int) op.fixedAddr << "\n"
        "addrMode          = " << std::dec << op.addrMode << "\n"
        "instType          = " << op.instType << "\n"
        "cycle             = " << op.cycle << "\n"
        "dmaCycle          = " << op.dmaCycle << "\n"
        "modify            = " << op.modify << "\n"
        "write             = " << op.write << "\n"
        "writeUnmodified   = " << op.writeUnmodified << "\n"
        "writeModified     = " << op.writeModified << "\n"
        "irq               = " << op.irq << "\n"
        "brk               = " << op.brk << "\n"
        "nmi               = " << op.nmi << "\n"
        "reset             = " << op.reset << "\n"
        "interruptPrologue = " << op.interruptPrologue << "\n"
        "oamDMATransfer    = " << op.oamDMATransfer << "\n"
        "done              = " << op.done <<
        "\n--------------------------------------------------\n";
}

void CPU::printUnknownOp() const {
    if (op.cycle == 2) {
        std::cerr << "NOTE: Opcode 0x" << std::hex << (unsigned int) op.opcode << " has not been "
            "implemented yet\n--------------------------------------------------\n" << std::dec;
    }
}

// Prints out the current state in the style of nestest.log

void CPU::printStateInst(const uint32_t inst) const {
    std::cout << std::hex << (unsigned int) pc << "  " << (unsigned int) inst << "  A:" <<
        (unsigned int) a << " X:" << (unsigned int) x << " Y:" << (unsigned int) y << " P:" <<
        (unsigned int) p << " SP:" << (unsigned int) sp << " CYC:" << std::dec << totalCycles <<
        "\n";
}

void CPU::printPPU() const {
    ppu.print(true);
}

// Private Member Functions

// Addressing Modes
// These functions prepare the operation functions as much as possible for execution

void CPU::abs() {
    switch (op.cycle) {
        case 0:
            op.addrMode = CPUOp::Absolute;
            ++pc;
            break;
        case 1:
            op.operandLo = read(pc);
            op.inst = (op.inst << 16) | (op.operandLo << 8);
            op.tempAddr = op.operandLo;
            ++pc;
            break;
        case 2:
            op.operandHi = read(pc);
            op.inst |= op.operandHi;
            op.tempAddr |= op.operandHi << 8;
            ++pc;
            if (op.instType == CPUOp::ReadInst || op.instType == CPUOp::WriteInst) {
                pollInterrupts();
            }
            break;
        case 3:
            if (op.instType == CPUOp::ReadInst || op.instType == CPUOp::RMWInst) {
                op.val = read(op.tempAddr);
            }
            op.modify = true;
            op.write = true;
            break;
        case 4:
            op.writeUnmodified = true;
            if (op.instType == CPUOp::RMWInst) {
                pollInterrupts();
            }
            break;
        case 5:
            op.writeModified = true;
    }
}

void CPU::abx() {
    uint8_t temp;
    switch (op.cycle) {
        case 0:
            op.addrMode = CPUOp::AbsoluteX;
            ++pc;
            break;
        case 1:
            op.operandLo = read(pc);
            op.inst = (op.inst << 16) | (op.operandLo << 8);
            // Add X to the low byte of the address in a 1-byte variable to match wraparound
            // behavior (i.e., without crossing the page boundary)
            temp = op.operandLo + x;
            op.tempAddr = temp;
            ++pc;
            break;
        case 2:
            op.operandHi = read(pc);
            op.inst |= op.operandHi;
            op.tempAddr |= op.operandHi << 8;
            // The proper address with the high byte fixed (i.e., can cross the page boundary)
            op.fixedAddr = x;
            op.fixedAddr += (op.operandHi << 8) + op.operandLo;
            if ((op.instType == CPUOp::ReadInst || op.instType == CPUOp::WriteInst) &&
                    !op.crossedPageBoundary()) {
                pollInterrupts();
            }
            ++pc;
            break;
        case 3:
            if (op.instType == CPUOp::ReadInst || op.instType == CPUOp::RMWInst) {
                op.val = read(op.tempAddr);
            }
            // If the page boundary was crossed, use the address with the fixed high byte and delay
            // the operation by one cycle (i.e., don't set modify to true until next cycle)
            if (op.crossedPageBoundary()) {
                op.tempAddr = op.fixedAddr;
                if (op.instType == CPUOp::ReadInst || op.instType == CPUOp::WriteInst) {
                    pollInterrupts();
                }
            } else {
                op.modify = true;
            }
            break;
        case 4:
            if (op.instType == CPUOp::ReadInst || op.instType == CPUOp::RMWInst) {
                op.val = read(op.tempAddr);
            }
            op.modify = true;
            op.write = true;
            break;
        case 5:
            op.writeUnmodified = true;
            pollInterrupts();
            break;
        case 6:
            op.writeModified = true;
    }
}

void CPU::aby() {
    uint8_t temp;
    switch (op.cycle) {
        case 0:
            op.addrMode = CPUOp::AbsoluteY;
            ++pc;
            break;
        case 1:
            op.operandLo = read(pc);
            op.inst = (op.inst << 16) | (op.operandLo << 8);
            // Add Y to the low byte of the address in a 1-byte variable to match wraparound
            // behavior (i.e., without crossing the page boundary)
            temp = op.operandLo + y;
            op.tempAddr = temp;
            ++pc;
            break;
        case 2:
            op.operandHi = read(pc);
            op.inst |= op.operandHi;
            op.tempAddr |= op.operandHi << 8;
            // The proper address with the high byte fixed (i.e., can cross the page boundary)
            op.fixedAddr = y;
            op.fixedAddr += (op.operandHi << 8) + op.operandLo;
            if ((op.instType == CPUOp::ReadInst || op.instType == CPUOp::WriteInst) &&
                    !op.crossedPageBoundary()) {
                pollInterrupts();
            }
            ++pc;
            break;
        case 3:
            if (op.instType == CPUOp::ReadInst || op.instType == CPUOp::RMWInst) {
                op.val = read(op.tempAddr);
            }
            // If the page boundary was crossed, use the address with the fixed high byte and delay
            // the operation by one cycle (i.e., don't set modify to true until next cycle)
            if (op.crossedPageBoundary()) {
                op.tempAddr = op.fixedAddr;
                if (op.instType == CPUOp::ReadInst || op.instType == CPUOp::WriteInst) {
                    pollInterrupts();
                }
            } else {
                op.modify = true;
            }
            break;
        case 4:
            if (op.instType == CPUOp::ReadInst || op.instType == CPUOp::RMWInst) {
                op.val = read(op.tempAddr);
            }
            op.modify = true;
            op.write = true;
            break;
        case 5:
            op.writeUnmodified = true;
            pollInterrupts();
            break;
        case 6:
            op.writeModified = true;
    }
}

void CPU::acc() {
    switch (op.cycle) {
        case 0:
            op.addrMode = CPUOp::Accumulator;
            pollInterrupts();
            ++pc;
            break;
        case 1:
            op.val = a;
            op.modify = true;
    }
}

void CPU::imm() {
    switch (op.cycle) {
        case 0:
            op.addrMode = CPUOp::Immediate;
            pollInterrupts();
            ++pc;
            break;
        case 1:
            op.operandLo = read(pc);
            op.inst = (op.inst << 8) | op.operandLo;
            op.val = op.operandLo;
            op.modify = true;
            ++pc;
    }
}

void CPU::imp() {
    switch (op.cycle) {
        case 0:
            op.addrMode = CPUOp::Implied;
            ++pc;
            // Polling for interrupts isn't done here because some implied instructions are 2 cycles
            // long and others are longer. At this point, the operation function hasn't been called
            // once yet for the current operation, so polling for interrupts can't be selectively
            // done for certain instructions. Instead, polling is done in the operation functions
            break;
        case 1:
            op.modify = true;
            op.write = true;
    }
}

void CPU::idr() {
    uint8_t temp;
    switch (op.cycle) {
        case 0:
            op.addrMode = CPUOp::Indirect;
            ++pc;
            break;
        case 1:
            op.operandLo = read(pc);
            op.inst = (op.inst << 16) | (op.operandLo << 8);
            ++pc;
            break;
        case 2:
            op.operandHi = read(pc);
            op.inst |= op.operandHi;
            ++pc;
            break;
        case 3:
            op.tempAddr = read((op.operandHi << 8) | op.operandLo);
            pollInterrupts();
            break;
        case 4:
            temp = op.operandLo + 1;
            op.tempAddr |= read((op.operandHi << 8) | temp) << 8;
            op.val = read(op.tempAddr);
    }
}

void CPU::idx() {
    uint8_t temp;
    switch (op.cycle) {
        case 0:
            op.addrMode = CPUOp::IndirectX;
            ++pc;
            break;
        case 1:
            op.operandLo = read(pc);
            op.inst = (op.inst << 8) | op.operandLo;
            ++pc;
            break;
        case 3:
            // Add X to the low byte of the address in a 1-byte variable to match wraparound
            // behavior (i.e., without crossing the page boundary and escaping the zero page)
            temp = op.operandLo + x;
            op.tempAddr = read(temp);
            break;
        case 4:
            // Add X + 1 to the low byte of the address in a 1-byte variable to match wraparound
            // behavior (i.e., without crossing the page boundary and escaping the zero page)
            temp = op.operandLo + x + 1;
            op.tempAddr |= read(temp) << 8;
            if (op.instType == CPUOp::ReadInst || op.instType == CPUOp::WriteInst) {
                pollInterrupts();
            }
            break;
        case 5:
            if (op.instType == CPUOp::ReadInst || op.instType == CPUOp::RMWInst) {
                op.val = read(op.tempAddr);
            }
            op.modify = true;
            op.write = true;
            break;
        case 6:
            op.writeUnmodified = true;
            pollInterrupts();
            break;
        case 7:
            op.writeModified = true;
    }
}

void CPU::idy() {
    uint8_t temp;
    switch (op.cycle) {
        case 0:
            op.addrMode = CPUOp::IndirectY;
            ++pc;
            break;
        case 1:
            op.operandLo = read(pc);
            op.inst = (op.inst << 8) | op.operandLo;
            ++pc;
            break;
        case 2:
            // Add Y to the low byte of the address in a 1-byte variable to match wraparound
            // behavior (i.e., without crossing the page boundary)
            temp = read(op.operandLo) + y;
            op.tempAddr = temp;
            break;
        case 3:
            // Add 1 to the low byte of the address in a 1-byte variable to match wraparound
            // behavior (i.e., without crossing the page boundary and escaping the zero page)
            temp = op.operandLo + 1;
            op.tempAddr |= read(temp) << 8;
            // The proper address with the high byte fixed (i.e., can cross the page boundary)
            op.fixedAddr = y;
            op.fixedAddr += (read(temp) << 8) + read(op.operandLo);
            if ((op.instType == CPUOp::ReadInst || op.instType == CPUOp::WriteInst) &&
                    !op.crossedPageBoundary()) {
                pollInterrupts();
            }
            break;
        case 4:
            if (op.instType == CPUOp::ReadInst || op.instType == CPUOp::RMWInst) {
                op.val = read(op.tempAddr);
            }
            // If the page boundary was crossed, use the address with the fixed high byte and delay
            // the operation by one cycle (i.e., don't set modify to true until next cycle)
            if (op.crossedPageBoundary()) {
                op.tempAddr = op.fixedAddr;
                if (op.instType == CPUOp::ReadInst || op.instType == CPUOp::WriteInst) {
                    pollInterrupts();
                }
            } else {
                op.modify = true;
            }
            break;
        case 5:
            if (op.instType == CPUOp::ReadInst || op.instType == CPUOp::RMWInst) {
                op.val = read(op.tempAddr);
            }
            op.modify = true;
            op.write = true;
            break;
        case 6:
            op.writeUnmodified = true;
            pollInterrupts();
            break;
        case 7:
            op.writeModified = true;
    }
}

void CPU::rel() {
    uint8_t temp;
    switch (op.cycle) {
        case 0:
            op.addrMode = CPUOp::Relative;
            ++pc;
            break;
        case 1:
            op.operandLo = read(pc);
            op.inst = (op.inst << 8) | op.operandLo;
            ++pc;
            break;
        case 2:
            // Add the low byte of the PC to the low byte of the address in a 1-byte variable to
            // match wraparound behavior (i.e., without crossing the page boundary)
            temp = (pc & 0xff) + op.operandLo;
            op.tempAddr = (pc & 0xff00) | temp;
            // The proper address with the high byte fixed (i.e., can cross the page boundary)
            op.fixedAddr = op.operandLo;
            if (op.fixedAddr & Negative) {
                op.fixedAddr |= 0xff00;
            }
            op.fixedAddr += pc;
            op.modify = true;
            // If the page boundary was crossed, use the address with the fixed high byte and delay
            // the operation by one cycle (i.e., don't set done to true until next cycle)
            if (op.crossedPageBoundary()) {
                op.tempAddr = op.fixedAddr;
                pollInterrupts();
            } else {
                op.done = true;
            }
            break;
        case 3:
            op.done = true;
    }
}

void CPU::zpg() {
    switch (op.cycle) {
        case 0:
            op.addrMode = CPUOp::ZeroPage;
            ++pc;
            break;
        case 1:
            op.operandLo = read(pc);
            op.inst = (op.inst << 8) | op.operandLo;
            op.tempAddr = op.operandLo;
            if (op.instType == CPUOp::ReadInst || op.instType == CPUOp::WriteInst) {
                pollInterrupts();
            }
            ++pc;
            break;
        case 2:
            if (op.instType == CPUOp::ReadInst || op.instType == CPUOp::RMWInst) {
                op.val = read(op.tempAddr);
            }
            op.modify = true;
            op.write = true;
            break;
        case 3:
            op.writeUnmodified = true;
            pollInterrupts();
            break;
        case 4:
            op.writeModified = true;
    }
}

void CPU::zpx() {
    uint8_t temp;
    switch (op.cycle) {
        case 0:
            op.addrMode = CPUOp::ZeroPageX;
            ++pc;
            break;
        case 1:
            op.operandLo = read(pc);
            op.inst = (op.inst << 8) | op.operandLo;
            // Add X to the low byte of the address in a 1-byte variable to match wraparound
            // behavior (i.e., without crossing the page boundary)
            temp = op.operandLo + x;
            op.tempAddr = temp;
            ++pc;
            break;
        case 2:
            if (op.instType == CPUOp::ReadInst || op.instType == CPUOp::WriteInst) {
                pollInterrupts();
            }
            break;
        case 3:
            if (op.instType == CPUOp::ReadInst || op.instType == CPUOp::RMWInst) {
                op.val = read(op.tempAddr);
            }
            op.modify = true;
            op.write = true;
            break;
        case 4:
            op.writeUnmodified = true;
            pollInterrupts();
            break;
        case 5:
            op.writeModified = true;
    }
}

void CPU::zpy() {
    uint8_t temp;
    switch (op.cycle) {
        case 0:
            op.addrMode = CPUOp::ZeroPageY;
            ++pc;
            break;
        case 1:
            op.operandLo = read(pc);
            op.inst = (op.inst << 8) | op.operandLo;
            // Add Y to the low byte of the address in a 1-byte variable to match wraparound
            // behavior (i.e., without crossing the page boundary)
            temp = op.operandLo + y;
            op.tempAddr = temp;
            ++pc;
            break;
        case 2:
            if (op.instType == CPUOp::ReadInst || op.instType == CPUOp::WriteInst) {
                pollInterrupts();
            }
            break;
        case 3:
            if (op.instType == CPUOp::ReadInst || op.instType == CPUOp::RMWInst) {
                op.val = read(op.tempAddr);
            }
            op.modify = true;
            op.write = true;
            break;
        case 4:
            op.writeUnmodified = true;
            pollInterrupts();
            break;
        case 5:
            op.writeModified = true;
    }
}

// Operations
// These functions perform any opcode-specific execution that can't be easily done in the addressing
// mode functions

void CPU::adc() {
    op.instType = CPUOp::ReadInst;
    if (op.modify) {
        // Add in a 2-byte variable to see if the result becomes greater than 1 byte
        uint16_t temp = a + op.val + (p & Carry);
        uint8_t pastA = a;
        a += op.val + (p & Carry);
        // If the result is greater than 1 byte, set the carry flag
        setCarryFlag(temp > 0xff);
        // If two positive values resulted in a negative value, set the overflow flag
        setOverflowFlag((pastA ^ a) & (op.val ^ a) & Negative);
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.done = true;
    }
}

void CPU::ahx() {
    op.done = true;
    printUnknownOp();
}

void CPU::alr() {
    op.done = true;
    printUnknownOp();
}

void CPU::andOp() {
    op.instType = CPUOp::ReadInst;
    if (op.modify) {
        a &= op.val;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.done = true;
    }
}

void CPU::anc() {
    op.done = true;
    printUnknownOp();
}

void CPU::arr() {
    op.done = true;
    printUnknownOp();
}

void CPU::asl() {
    if (op.addrMode != CPUOp::Accumulator) {
        op.instType = CPUOp::RMWInst;
    }

    // If the addressing mode is accumulator, then this operation only operates on the accumulator
    // and doesn't read or write
    if (op.modify && op.addrMode == CPUOp::Accumulator) {
        setCarryFlag(a & Negative);
        a = a << 1;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.done = true;
    // Otherwise, this is a read-modify-write operation
    } else if (op.writeModified) {
        write(op.tempAddr, op.val);
        op.done = true;
    } else if (op.writeUnmodified) {
        write(op.tempAddr, op.val);
        setCarryFlag(op.val & Negative);
        op.val = op.val << 1;
        updateZeroFlag(op.val);
        updateNegativeFlag(op.val);
    }
}

void CPU::axs() {
    op.done = true;
    printUnknownOp();
}

void CPU::bcc() {
    if (isCarry()) {
        // Branch not taken
        if (op.cycle == 0) {
            pollInterrupts();
        } else if (op.cycle == 1) {
            op.done = true;
        }
    } else if (op.modify && !isCarry()) {
        // Branch taken
        // Polling for interrupts is only done when the page boundary is crossed, which is checked
        // for in the relative addressing mode function
        pc = op.tempAddr;
    }
}

void CPU::bcs() {
    if (!isCarry()) {
        // Branch not taken
        if (op.cycle == 0) {
            pollInterrupts();
        } else if (op.cycle == 1) {
            op.done = true;
        }
    } else if (op.modify && isCarry()) {
        // Branch taken
        // Polling for interrupts is only done when the page boundary is crossed, which is checked
        // for in the relative addressing mode function
        pc = op.tempAddr;
    }
}

void CPU::beq() {
    if (!isZero()) {
        // Branch not taken
        if (op.cycle == 0) {
            pollInterrupts();
        } else if (op.cycle == 1) {
            op.done = true;
        }
    } else if (op.modify && isZero()) {
        // Branch taken
        // Polling for interrupts is only done when the page boundary is crossed, which is checked
        // for in the relative addressing mode function
        pc = op.tempAddr;
    }
}

void CPU::bit() {
    op.instType = CPUOp::ReadInst;
    if (op.modify) {
        uint8_t temp = a & op.val;
        setOverflowFlag(op.val & Overflow);
        updateZeroFlag(temp);
        updateNegativeFlag(op.val);
        op.done = true;
    }
}

void CPU::bmi() {
    if (!isNegative()) {
        // Branch not taken
        if (op.cycle == 0) {
            pollInterrupts();
        } else if (op.cycle == 1) {
            op.done = true;
        }
    } else if (op.modify && isNegative()) {
        // Branch taken
        // Polling for interrupts is only done when the page boundary is crossed, which is checked
        // for in the relative addressing mode function
        pc = op.tempAddr;
    }
}

void CPU::bne() {
    if (isZero()) {
        // Branch not taken
        if (op.cycle == 0) {
            pollInterrupts();
        } else if (op.cycle == 1) {
            op.done = true;
        }
    } else if (op.modify && !isZero()) {
        // Branch taken
        // Polling for interrupts is only done when the page boundary is crossed, which is checked
        // for in the relative addressing mode function
        pc = op.tempAddr;
    }
}

void CPU::bpl() {
    if (isNegative()) {
        // Branch not taken
        if (op.cycle == 0) {
            pollInterrupts();
        } else if (op.cycle == 1) {
            op.done = true;
        }
    } else if (op.modify && !isNegative()) {
        // Branch taken
        // Polling for interrupts is only done when the page boundary is crossed, which is checked
        // for in the relative addressing mode function
        pc = op.tempAddr;
    }
}

void CPU::brk() {
    if (haltAtBrk) {
        endOfProgram = true;
    } else if (op.cycle == 0) {
        op.irq = true;
        op.brk = true;
        // Start the interrupt prologue regardless if the Interrupt Disable flag is set
        op.interruptPrologue = true;
    }
}

void CPU::bvc() {
    if (isOverflow()) {
        // Branch not taken
        if (op.cycle == 0) {
            pollInterrupts();
        } else if (op.cycle == 1) {
            op.done = true;
        }
    } else if (op.modify && !isOverflow()) {
        // Branch taken
        // Polling for interrupts is only done when the page boundary is crossed, which is checked
        // for in the relative addressing mode function
        pc = op.tempAddr;
    }
}

void CPU::bvs() {
    if (!isOverflow()) {
        // Branch not taken
        if (op.cycle == 0) {
            pollInterrupts();
        } else if (op.cycle == 1) {
            op.done = true;
        }
    } else if (op.modify && isOverflow()) {
        // Branch taken
        // Polling for interrupts is only done when the page boundary is crossed, which is checked
        // for in the relative addressing mode function
        pc = op.tempAddr;
    }
}

void CPU::clc() {
    if (op.cycle == 0) {
        pollInterrupts();
    } else if (op.modify) {
        setCarryFlag(false);
        op.done = true;
    }
}

void CPU::cld() {
    if (op.cycle == 0) {
        pollInterrupts();
    } else if (op.modify) {
        setDecimalMode(false);
        op.done = true;
    }
}

void CPU::cli() {
    if (op.cycle == 0) {
        pollInterrupts();
    } else if (op.modify) {
        setInterruptDisable(false);
        op.done = true;
    }
}

void CPU::clv() {
    if (op.cycle == 0) {
        pollInterrupts();
    } else if (op.modify) {
        setOverflowFlag(false);
        op.done = true;
    }
}

void CPU::cmp() {
    op.instType = CPUOp::ReadInst;
    if (op.modify) {
        uint8_t temp = a - op.val;
        setCarryFlag(a >= op.val);
        updateZeroFlag(temp);
        updateNegativeFlag(temp);
        op.done = true;
    }
}

void CPU::cpx() {
    op.instType = CPUOp::ReadInst;
    if (op.modify) {
        uint8_t temp = x - op.val;
        setCarryFlag(x >= op.val);
        updateZeroFlag(temp);
        updateNegativeFlag(temp);
        op.done = true;
    }
}

void CPU::cpy() {
    op.instType = CPUOp::ReadInst;
    if (op.modify) {
        uint8_t temp = y - op.val;
        setCarryFlag(y >= op.val);
        updateZeroFlag(temp);
        updateNegativeFlag(temp);
        op.done = true;
    }
}

void CPU::dcp() {
    op.instType = CPUOp::RMWInst;
    if (op.writeModified) {
        dec();
        cmp();
    } else if (op.writeUnmodified) {
        dec();
    }
}

void CPU::dec() {
    op.instType = CPUOp::RMWInst;
    if (op.writeModified) {
        write(op.tempAddr, op.val);
        op.done = true;
    } else if (op.writeUnmodified) {
        write(op.tempAddr, op.val);
        --op.val;
        updateZeroFlag(op.val);
        updateNegativeFlag(op.val);
    }
}

void CPU::dex() {
    if (op.cycle == 0) {
        pollInterrupts();
    } else if (op.modify) {
        --x;
        updateZeroFlag(x);
        updateNegativeFlag(x);
        op.done = true;
    }
}

void CPU::dey() {
    if (op.cycle == 0) {
        pollInterrupts();
    } else if (op.modify) {
        --y;
        updateZeroFlag(y);
        updateNegativeFlag(y);
        op.done = true;
    }
}

void CPU::eor() {
    op.instType = CPUOp::ReadInst;
    if (op.modify) {
        a ^= op.val;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.done = true;
    }
}

void CPU::inc() {
    op.instType = CPUOp::RMWInst;
    if (op.writeModified) {
        write(op.tempAddr, op.val);
        op.done = true;
    } else if (op.writeUnmodified) {
        write(op.tempAddr, op.val);
        ++op.val;
        updateZeroFlag(op.val);
        updateNegativeFlag(op.val);
    }
}

void CPU::inx() {
    if (op.cycle == 0) {
        pollInterrupts();
    } else if (op.modify) {
        ++x;
        updateZeroFlag(x);
        updateNegativeFlag(x);
        op.done = true;
    }
}

void CPU::iny() {
    if (op.cycle == 0) {
        pollInterrupts();
    } else if (op.modify) {
        ++y;
        updateZeroFlag(y);
        updateNegativeFlag(y);
        op.done = true;
    }
}

void CPU::isc() {
    op.instType = CPUOp::RMWInst;
    if (op.writeModified) {
        inc();
        sbc();
    } else if (op.writeUnmodified) {
        inc();
    }
}

void CPU::jmp() {
    if (op.addrMode == CPUOp::Absolute) {
        if (op.cycle == 1) {
            pollInterrupts();
        } else if (op.cycle == 2) {
            pc = op.tempAddr;
            op.done = true;
        }
    } else {
        // Indirect addressing mode
        if (op.cycle == 4) {
            pc = op.tempAddr;
            op.done = true;
        }
    }
}

void CPU::jsr() {
    uint8_t temp;
    switch (op.cycle) {
        case 2:
            // Absolute addressing mode increments the PC three times by default, but JSR only does
            // it two times
            --pc;
            break;
        case 3:
            temp = (pc & 0xff00) >> 8;
            ram.push(sp, temp, mute);
            break;
        case 4:
            temp = pc & 0xff;
            ram.push(sp, temp, mute);
            pollInterrupts();
            break;
        case 5:
            pc = op.tempAddr;
            op.done = true;
    }
}

void CPU::las() {
    op.done = true;
    printUnknownOp();
}

void CPU::lax() {
    op.instType = CPUOp::ReadInst;
    if (op.modify) {
        a = op.val;
        x = op.val;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.done = true;
    }
}

void CPU::lda() {
    op.instType = CPUOp::ReadInst;
    if (op.modify) {
        a = op.val;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.done = true;
    }
}

void CPU::ldx() {
    op.instType = CPUOp::ReadInst;
    if (op.modify) {
        x = op.val;
        updateZeroFlag(x);
        updateNegativeFlag(x);
        op.done = true;
    }
}

void CPU::ldy() {
    op.instType = CPUOp::ReadInst;
    if (op.modify) {
        y = op.val;
        updateZeroFlag(y);
        updateNegativeFlag(y);
        op.done = true;
    }
}

void CPU::lsr() {
    if (op.addrMode != CPUOp::Accumulator) {
        op.instType = CPUOp::RMWInst;
    }

    // If the addressing mode is accumulator, then this operation only operates on the accumulator
    // and doesn't read or write
    if (op.modify && op.addrMode == CPUOp::Accumulator) {
        setCarryFlag(a & Carry);
        a = a >> 1;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.done = true;
    // Otherwise, this is a read-modify-write operation
    } else if (op.writeModified) {
        write(op.tempAddr, op.val);
        op.done = true;
    } else if (op.writeUnmodified) {
        write(op.tempAddr, op.val);
        setCarryFlag(op.val & Carry);
        op.val = op.val >> 1;
        updateZeroFlag(op.val);
        updateNegativeFlag(op.val);
    }
}

void CPU::nop() {
    op.instType = CPUOp::ReadInst;
    if (op.cycle == 0 && op.addrMode == CPUOp::Implied) {
        pollInterrupts();
    } else if (op.modify) {
        op.done = true;
    }
}

void CPU::ora() {
    op.instType = CPUOp::ReadInst;
    if (op.modify) {
        a |= op.val;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.done = true;
    }
}

void CPU::pha() {
    switch (op.cycle) {
        case 1:
            pollInterrupts();
            break;
        case 2:
            ram.push(sp, a, mute);
            op.done = true;
    }
}

void CPU::php() {
    uint8_t temp;
    switch (op.cycle) {
        case 1:
            pollInterrupts();
            break;
        case 2:
            temp = p;
            ram.push(sp, temp, mute);
            op.done = true;
    }
}

void CPU::pla() {
    switch (op.cycle) {
        case 2:
            op.val = ram.pull(sp, mute);
            pollInterrupts();
            break;
        case 3:
            a = op.val;
            updateZeroFlag(a);
            updateNegativeFlag(a);
            op.done = true;
    }
}

void CPU::plp() {
    switch (op.cycle) {
        case 2:
            op.val = ram.pull(sp, mute);
            pollInterrupts();
            break;
        case 3:
            p = op.val | Break | UnusedFlag;
            op.done = true;
    }
}

void CPU::rla() {
    op.instType = CPUOp::RMWInst;
    if (op.writeModified) {
        rol();
        andOp();
    } else if (op.writeUnmodified) {
        rol();
    }
}

void CPU::rol() {
    if (op.addrMode != CPUOp::Accumulator) {
        op.instType = CPUOp::RMWInst;
    }

    // If the addressing mode is accumulator, then this operation only operates on the accumulator
    // and doesn't read or write
    if (op.modify && op.addrMode == CPUOp::Accumulator) {
        uint8_t temp = a << 1;
        if (isCarry()) {
            temp |= Carry;
        }
        setCarryFlag(a & Negative);
        a = temp;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.done = true;
    // Otherwise, this is a read-modify-write operation
    } else if (op.writeModified) {
        write(op.tempAddr, op.val);
        op.done = true;
    } else if (op.writeUnmodified) {
        write(op.tempAddr, op.val);
        uint8_t temp = op.val << 1;
        if (isCarry()) {
            temp |= Carry;
        }
        setCarryFlag(op.val & Negative);
        op.val = temp;
        updateZeroFlag(op.val);
        updateNegativeFlag(op.val);
    }
}

void CPU::ror() {
    if (op.addrMode != CPUOp::Accumulator) {
        op.instType = CPUOp::RMWInst;
    }

    // If the addressing mode is accumulator, then this operation only operates on the accumulator
    // and doesn't read or write
    if (op.modify && op.addrMode == CPUOp::Accumulator) {
        uint8_t temp = a >> 1;
        if (isCarry()) {
            temp |= Negative;
        }
        setCarryFlag(a & Carry);
        a = temp;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.done = true;
    // Otherwise, this is a read-modify-write operation
    } else if (op.writeModified) {
        write(op.tempAddr, op.val);
        op.done = true;
    } else if (op.writeUnmodified) {
        write(op.tempAddr, op.val);
        uint8_t temp = op.val >> 1;
        if (isCarry()) {
            temp |= Negative;
        }
        setCarryFlag(op.val & Carry);
        op.val = temp;
        updateZeroFlag(op.val);
        updateNegativeFlag(op.val);
    }
}

void CPU::rra() {
    op.instType = CPUOp::RMWInst;
    if (op.writeModified) {
        ror();
        adc();
    } else if (op.writeUnmodified) {
        ror();
    }
}

void CPU::rti() {
    switch (op.cycle) {
        case 3:
            p = ram.pull(sp, mute) | Break | UnusedFlag;
            break;
        case 4:
            op.tempAddr |= ram.pull(sp, mute);
            pollInterrupts();
            break;
        case 5:
            op.tempAddr |= ram.pull(sp, mute) << 8;
            pc = op.tempAddr;
            op.done = true;
    }
}

void CPU::rts() {
    switch (op.cycle) {
        case 3:
            op.tempAddr = ram.pull(sp, mute);
            break;
        case 4:
            op.tempAddr |= ram.pull(sp, mute) << 8;
            pollInterrupts();
            break;
        case 5:
            pc = op.tempAddr + 1;
            op.done = true;
    }
}

void CPU::sax() {
    op.instType = CPUOp::WriteInst;
    if (op.write) {
        write(op.tempAddr, a & x);
        op.done = true;
    }
}

void CPU::sbc() {
    op.instType = CPUOp::ReadInst;
    if (op.modify) {
        op.val = 0xff - op.val;
        adc();
    }
}

void CPU::sec() {
    if (op.cycle == 0) {
        pollInterrupts();
    } else if (op.modify) {
        setCarryFlag(true);
        op.done = true;
    }
}

void CPU::sed() {
    if (op.cycle == 0) {
        pollInterrupts();
    } else if (op.modify) {
        setDecimalMode(true);
        op.done = true;
    }
}

void CPU::sei() {
    if (op.cycle == 0) {
        pollInterrupts();
    } else if (op.modify) {
        setInterruptDisable(true);
        op.done = true;
    }
}

void CPU::shx() {
    op.done = true;
    printUnknownOp();
}

void CPU::shy() {
    op.done = true;
    printUnknownOp();
}

void CPU::slo() {
    op.instType = CPUOp::RMWInst;
    if (op.writeModified) {
        asl();
        ora();
    } else if (op.writeUnmodified) {
        asl();
    }
}

void CPU::sre() {
    op.instType = CPUOp::RMWInst;
    if (op.writeModified) {
        lsr();
        eor();
    } else if (op.writeUnmodified) {
        lsr();
    }
}

void CPU::sta() {
    op.instType = CPUOp::WriteInst;
    if (op.write) {
        write(op.tempAddr, a);
        op.done = true;
    }
}

void CPU::stp() {
    op.done = true;
    printUnknownOp();
}

void CPU::stx() {
    op.instType = CPUOp::WriteInst;
    if (op.write) {
        write(op.tempAddr, x);
        op.done = true;
    }
}

void CPU::sty() {
    op.instType = CPUOp::WriteInst;
    if (op.write) {
        write(op.tempAddr, y);
        op.done = true;
    }
}

void CPU::tas() {
    op.done = true;
    printUnknownOp();
}

void CPU::tax() {
    if (op.cycle == 0) {
        pollInterrupts();
    } else if (op.modify) {
        x = a;
        updateZeroFlag(x);
        updateNegativeFlag(x);
        op.done = true;
    }
}

void CPU::tay() {
    if (op.cycle == 0) {
        pollInterrupts();
    } else if (op.modify) {
        y = a;
        updateZeroFlag(y);
        updateNegativeFlag(y);
        op.done = true;
    }
}

void CPU::tsx() {
    if (op.cycle == 0) {
        pollInterrupts();
    } else if (op.modify) {
        x = sp;
        updateZeroFlag(x);
        updateNegativeFlag(x);
        op.done = true;
    }
}

void CPU::txa() {
    if (op.cycle == 0) {
        pollInterrupts();
    } else if (op.modify) {
        a = x;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.done = true;
    }
}

void CPU::txs() {
    if (op.cycle == 0) {
        pollInterrupts();
    } else if (op.modify) {
        sp = x;
        op.done = true;
    }
}

void CPU::tya() {
    if (op.cycle == 0) {
        pollInterrupts();
    } else if (op.modify) {
        a = y;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.done = true;
    }
}

void CPU::xaa() {
    op.done = true;
    printUnknownOp();
}

// Passes the read to the component that is responsible for the address range in the CPU memory map

uint8_t CPU::read(const uint16_t addr) {
    const uint16_t ppuCtrl = 0x2000;
    const uint16_t sq1Vol = 0x4000;
    const uint16_t oamDMAAddr = 0x4014;
    const uint16_t joy1 = 0x4016;
    const uint16_t joy2 = 0x4017;
    const uint16_t prgRAMStart = 0x4020;
    if (addr < ppuCtrl) {
        return ram.read(addr);
    } else if (addr < sq1Vol || addr == oamDMAAddr) {
        return ppu.readRegister(addr, mmc);
    } else if (addr < joy1) {
        return apu.readRegister(addr);
    } else if (addr <= joy2) {
        return io.readRegister(addr);
    } else if (addr >= prgRAMStart) {
        return mmc.readPRG(addr);
    }
    // Only reached for disabled APU and I/O registers $4018 - $401f
    return 0;
}

// Passes the write to the component that is responsible for the address range in the CPU memory map

void CPU::write(const uint16_t addr, const uint8_t val) {
    const uint16_t ppuCtrl = 0x2000;
    const uint16_t sq1Vol = 0x4000;
    const uint16_t oamDMAAddr = 0x4014;
    const uint16_t joy1 = 0x4016;
    const uint16_t joy2 = 0x4017;
    const uint16_t prgRAMStart = 0x4020;
    if (addr < ppuCtrl) {
        ram.write(addr, val);
    } else if (addr < sq1Vol || addr == oamDMAAddr) {
        ppu.writeRegister(addr, val, mmc, mute);
        if (addr == oamDMAAddr) {
            // Start the OAM DMA transfer
            op.oamDMATransfer = true;
        }
    } else if (addr < joy1) {
        apu.writeRegister(addr, val);
    } else if (addr <= joy2) {
        if (addr == joy2) {
            // This register is shared between the APU and I/O, so write the value to both to ensure
            // that they're equal
            apu.writeRegister(addr, val);
        }
        io.writeRegister(addr, val);
    } else if (addr >= prgRAMStart)  {
        mmc.writePRG(addr, val, totalCycles);
    }

    if (!mute) {
        std::cout << std::hex << "0x" << (unsigned int) val << " has been written to the address 0x"
            << (unsigned int) addr << "\n--------------------------------------------------\n" <<
            std::dec;
    }
}

// Performs an OAM DMA transfer to the PPU:
// https://www.nesdev.org/wiki/PPU_registers#OAM_DMA_($4014)_%3E_write

void CPU::oamDMATransfer() {
    // OAM DMA transfer takes 1 extra cycle if the CPU cycle is odd after cycle 0 of the transfer
    if (op.dmaCycle == 1 && totalCycles % 2) {
        --op.dmaCycle;
    // Cycle 0 is a wait state cycle. Perform both a read and a write every other cycle. While the
    // read and write should technically be done on separate cycles, it arguably doesn't matter
    // because the CPU is suspended and can't write to the addresses that the transfer is reading
    // from. However, there could be games that ignore best practice and perform OAM DMA transfers
    // while the PPU is rendering and evaluating sprites, which would behave differently depending
    // on the exact cycles that the writes are performed on. Could refactor to be more accurate, but
    // it'll increase code complexity
    } else if (op.dmaCycle > 0 && op.dmaCycle % 2 == 0) {
        const uint16_t oamDMAAddr = 0x4014;
        const uint16_t cpuBaseAddr = ppu.readRegister(oamDMAAddr, mmc) << 8;
        const unsigned int oamAddr = op.dmaCycle / 2 - 1;
        const uint16_t cpuAddr = cpuBaseAddr + oamAddr;
        const uint8_t cpuData = read(cpuAddr);
        ppu.writeOAM(oamAddr, cpuData);
    }

    ++op.dmaCycle;

    if (op.dmaCycle == 514) {
        op.clearDMA();
        op.done = true;
    }
}

// Asks the PPU if an NMI should be executed. This function is called on the second-to-last cycle of
// every instruction, except for branch instructions:
// https://www.nesdev.org/wiki/CPU_interrupts#Branch_instructions_and_interrupts

void CPU::pollInterrupts() {
    if (ppu.isNMIActive(mmc, mute)) {
        op.nmi = true;
    }
}

// Performs the interrupt prologue for maskable interrupts, which involves pushing the PC and P
// registers to the stack and setting the Interrupt Disable flag

void CPU::prepareIRQ() {
    uint8_t temp;
    const uint16_t lowerIRQAddr = 0xfffe;
    const uint16_t upperIRQAddr = 0xffff;
    switch (op.cycle) {
        case 1:
            // Increment the PC again because the BRK instruction is 2 bytes long
            if (op.brk) {
                ++pc;
            }
            break;
        case 2:
            temp = (pc & 0xff00) >> 8;
            ram.push(sp, temp, mute);
            break;
        case 3:
            temp = pc & 0xff;
            ram.push(sp, temp, mute);
            break;
        case 4:
            // This cycle is when the CPU decides which interrupt vector to take:
            // https://www.nesdev.org/wiki/CPU_interrupts#Interrupt_hijacking
            pollInterrupts();
            if (op.reset) {
                prepareReset();
                return;
            } else if (op.nmi) {
                prepareNMI();
                return;
            }
            temp = p;
            // When pushing the P register to the stack, only set the Break flag if the IRQ was
            // triggered by the BRK instruction
            if (!op.brk) {
                temp &= ~Break;
            }
            ram.push(sp, temp, mute);
            break;
        case 5:
            op.tempAddr = read(lowerIRQAddr);
            setInterruptDisable(true);
            break;
        case 6:
            op.tempAddr |= read(upperIRQAddr) << 8;
            pc = op.tempAddr;
            op.clearInterruptFlags();
            op.done = true;
    }
}

// Performs the interrupt prologue for non-maskable interrupts, which involves pushing the PC and P
// registers to the stack and setting the Interrupt Disable flag

void CPU::prepareNMI() {
    uint8_t temp;
    const uint16_t lowerNMIAddr = 0xfffa;
    const uint16_t upperNMIAddr = 0xfffb;
    switch (op.cycle) {
        case 2:
            temp = (pc & 0xff00) >> 8;
            ram.push(sp, temp, mute);
            break;
        case 3:
            temp = pc & 0xff;
            ram.push(sp, temp, mute);
            break;
        case 4:
            // This cycle is when the CPU decides which interrupt vector to take:
            // https://www.nesdev.org/wiki/CPU_interrupts#Interrupt_hijacking
            pollInterrupts();
            if (op.reset) {
                prepareReset();
                return;
            }
            temp = p;
            // The Break flag is still set if the NMI interrupts a BRK instruction
            if (!op.brk) {
                temp &= ~Break;
            }
            ram.push(sp, temp, mute);
            break;
        case 5:
            op.tempAddr = read(lowerNMIAddr);
            setInterruptDisable(true);
            break;
        case 6:
            op.tempAddr |= read(upperNMIAddr) << 8;
            pc = op.tempAddr;
            op.clearInterruptFlags();
            op.done = true;
    }
}

// Performs the interrupt prologue for reset interrupts, which involves attempting to push the PC
// and P registers to the stack and setting the Interrupt Disable flag. Writes are surpressed, so
// the values never get pushed to the stack. However, the SP register still gets decremented

void CPU::prepareReset() {
    const uint16_t lowerResetAddr = 0xfffc;
    const uint16_t upperResetAddr = 0xfffd;
    switch (op.cycle) {
        case 2:
            --sp;
            break;
        case 3:
            --sp;
            break;
        case 4:
            --sp;
            break;
        case 5:
            op.tempAddr = read(lowerResetAddr);
            setInterruptDisable(true);
            break;
        case 6:
            op.tempAddr |= read(upperResetAddr) << 8;
            pc = op.tempAddr;
            op.clearInterruptFlags();
            op.done = true;
    }
}

// Sets the Zero flag if the result is zero. Otherwise, clear it

void CPU::updateZeroFlag(const uint8_t result) {
    if (result == 0) {
        p |= Zero;
    } else {
        p &= ~Zero;
    }
}

// Sets the Negative flag if the result is negative. Otherwise, clear it

void CPU::updateNegativeFlag(const uint8_t result) {
    p &= ~Negative;
    p |= result & Negative;
}

bool CPU::isCarry() const {
    return p & Carry;
}

bool CPU::isZero() const {
    return p & Zero;
}

bool CPU::areInterruptsDisabled() const {
    return p & InterruptDisable;
}

bool CPU::isOverflow() const {
    return p & Overflow;
}

bool CPU::isNegative() const {
    return p & Negative;
}

void CPU::setCarryFlag(const bool val) {
    if (val) {
        p |= Carry;
    } else {
        p &= ~Carry;
    }
}

void CPU::setZeroFlag(const bool val) {
    if (val) {
        p |= Zero;
    } else {
        p &= ~Zero;
    }
}

void CPU::setInterruptDisable(const bool val) {
    if (val) {
        p |= InterruptDisable;
    } else {
        p &= ~InterruptDisable;
    }
}

void CPU::setDecimalMode(const bool val) {
    if (val) {
        p |= DecimalMode;
    } else {
        p &= ~DecimalMode;
    }
}

void CPU::setOverflowFlag(const bool val) {
    if (val) {
        p |= Overflow;
    } else {
        p &= ~Overflow;
    }
}

void CPU::setNegativeFlag(const bool val) {
    if (val) {
        p |= Negative;
    } else {
        p &= ~Negative;
    }
}