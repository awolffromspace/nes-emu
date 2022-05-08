#include "cpu.h"

CPU::CPU() :
        sp(0xfd),
        a(0),
        x(0),
        y(0),
        p(UnusedFlag | Break | InterruptDisable),
        totalCycles(0),
        endOfProgram(false),
        haltAtBrk(false),
        mute(true) {
    // Initialize PC to the reset vector
    pc = (read(UPPER_RESET_ADDR) << 8) | read(LOWER_RESET_ADDR);
}

void CPU::clear() {
    sp = 0xfd;
    a = 0;
    x = 0;
    y = 0;
    p = UnusedFlag | Break | InterruptDisable;
    op.clear(true);
    ram.clear(mute);
    ppu.clear(mute);
    apu.clear(mute);
    io.clear(mute);
    mmc.clear(mute);
    pc = (read(UPPER_RESET_ADDR) << 8) | read(LOWER_RESET_ADDR);
    totalCycles = 0;
    endOfProgram = false;

    if (!mute) {
        std::cout << "CPU was cleared\n";
    }
}

void CPU::step(SDL_Renderer* renderer, SDL_Texture* texture) {
    if (op.done || totalCycles == 0) {
        op.clear(false);

        // If any interrupts are flagged, start the interrupt prologue
        // Ignore IRQ if the Interrupt Disable flag is set
        if ((op.irq && !areInterruptsDisabled()) || op.nmi || op.reset) {
            op.interruptPrologue = true;
        }

        // Fetch
        op.pc = pc;
        op.inst = read(pc);
        op.opcode = op.inst;
    }

    if (op.oamDMATransfer) {
        oamDMATransfer();
    } else if (op.interruptPrologue && op.reset) {
        prepareReset();
    } else if (op.interruptPrologue && op.nmi) {
        prepareNMI();
    } else if (op.interruptPrologue && op.irq) {
        prepareIRQ(false);
    } else {
        // Decode and execute
        (this->*addrModeArr[op.opcode])();
        (this->*opcodeArr[op.opcode])();
    }

    for (unsigned int i = 0; i < 3; ++i) {
        ppu.step(mmc, renderer, texture, mute);
    }

    ++op.cycle;
    ++totalCycles;
}

// Miscellaneous Functions

void CPU::readInInst(std::string& filename) {
    mmc.readInInst(filename);
}

void CPU::readInINES(std::string& filename) {
    mmc.readInINES(filename, ppu);
    if (filename == "nestest.nes") {
        pc = 0xc000;
    } else {
        pc = (read(UPPER_RESET_ADDR) << 8) | read(LOWER_RESET_ADDR);
    }
}

bool CPU::compareState(struct CPU::State& state) const {
    if (state.pc != pc || state.sp != sp || state.a != a || state.x != x ||
            state.y != y || (state.p & 0xcf) != (p & 0xcf) ||
            state.totalCycles != totalCycles) {
        return false;
    }
    return true;
}

uint32_t CPU::getFutureInst() {
    uint8_t operandLo = 0, operandHi = 0;
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

unsigned int CPU::getTotalCycles() const {
    return totalCycles;
}

bool CPU::isEndOfProgram() const {
    return endOfProgram;
}

bool CPU::isHaltAtBrk() const {
    return haltAtBrk;
}

void CPU::setHaltAtBrk(bool h) {
    haltAtBrk = h;
}

void CPU::setMute(bool m) {
    mute = m;
}

unsigned int CPU::getOpCycles() const {
    return op.cycle;
}

uint8_t CPU::getValidOpResult() const {
    return ram.read(2);
}

uint8_t CPU::getInvalidOpResult() const {
    return ram.read(3);
}

// Print Functions

void CPU::print(bool isCycleDone) const {
    if (mute) {
        return;
    }

    unsigned int inc = 0;
    std::string time;
    std::bitset<8> binaryP(p);
    if (isCycleDone) {
        time = "After";
    } else {
        time = "Before";
        ++inc;
        std::cout << "--------------------------------------------------\n";
    }
    std::cout << "Cycle " << totalCycles + inc << ": " << time
        << std::hex << "\n------------------------\n"
        "CPU Fields\n------------------------\n"
        "pc          = 0x" << (unsigned int) pc << "\n"
        "sp          = 0x" << (unsigned int) sp << "\n"
        "a           = 0x" << (unsigned int) a << "\n"
        "x           = 0x" << (unsigned int) x << "\n"
        "y           = 0x" << (unsigned int) y << "\n"
        "p           = " << binaryP << "\n"
        "totalCycles = " << std::dec << totalCycles <<
        "\n------------------------\n" << std::hex <<
        "CPU Operation Fields\n------------------------\n"
        "inst        = 0x" << (unsigned int) op.inst << "\n"
        "pc          = 0x" << (unsigned int) op.pc << "\n"
        "opcode      = 0x" << (unsigned int) op.opcode << "\n"
        "operandLo   = 0x" << (unsigned int) op.operandLo << "\n"
        "operandHi   = 0x" << (unsigned int) op.operandHi << "\n"
        "val         = 0x" << (unsigned int) op.val << "\n"
        "tempAddr    = 0x" << (unsigned int) op.tempAddr << "\n"
        "fixedAddr   = 0x" << (unsigned int) op.fixedAddr << "\n"
        << std::dec <<
        "addrMode    = " << (unsigned int) op.addrMode << "\n"
        "instType    = " << (unsigned int) op.instType << "\n"
        "cycle       = " << op.cycle << "\n"
        "dmaCycle    = " << op.dmaCycle <<
        "\n--------------------------------------------------\n";
}

void CPU::printUnknownOp() const {
    if (op.cycle == 2) {
        if (!mute) {
            std::cout << std::hex << "NOTE: Opcode 0x" <<
                (unsigned int) op.opcode <<
                " has not been implemented yet"
                "\n--------------------------------------------------\n" <<
                std::dec;
        }
    }
}

void CPU::printStateInst(uint32_t inst) const {
    std::cout << std::hex << (unsigned int) pc << "  " << (unsigned int) inst <<
        "  A:" << (unsigned int) a << " X:" << (unsigned int) x << " Y:" <<
        (unsigned int) y << " P:" << (unsigned int) p << " SP:" <<
        (unsigned int) sp << " CYC:" << std::dec << totalCycles << "\n";
}

// Addressing Modes
// These functions prepare the operation functions as much as possible for
// execution

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
            if (op.instType == CPUOp::ReadInst ||
                    op.instType == CPUOp::WriteInst) {
                pollInterrupts();
            }
            break;
        case 3:
            if (op.instType == CPUOp::ReadInst ||
                    op.instType == CPUOp::RMWInst) {
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

void CPU::abx() {
    uint8_t temp = 0;
    switch (op.cycle) {
        case 0:
            op.addrMode = CPUOp::AbsoluteX;
            ++pc;
            break;
        case 1:
            op.operandLo = read(pc);
            op.inst = (op.inst << 16) | (op.operandLo << 8);
            temp = op.operandLo + x;
            op.tempAddr = temp;
            ++pc;
            break;
        case 2:
            op.operandHi = read(pc);
            op.inst |= op.operandHi;
            op.tempAddr |= op.operandHi << 8;
            op.fixedAddr = x;
            op.fixedAddr += (op.operandHi << 8) + op.operandLo;
            if (op.instType == CPUOp::ReadInst && op.tempAddr == op.fixedAddr) {
                pollInterrupts();
            }
            ++pc;
            break;
        case 3:
            if (op.instType == CPUOp::ReadInst ||
                    op.instType == CPUOp::RMWInst) {
                op.val = read(op.tempAddr);
            }
            if (op.tempAddr == op.fixedAddr) {
                op.modify = true;
            } else {
                op.tempAddr = op.fixedAddr;
                pollInterrupts();
            }
            break;
        case 4:
            if (op.instType == CPUOp::ReadInst ||
                    op.instType == CPUOp::RMWInst) {
                op.val = read(op.tempAddr);
            }
            if (!op.modify) {
                op.modify = true;
            }
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
    uint8_t temp = 0;
    switch (op.cycle) {
        case 0:
            op.addrMode = CPUOp::AbsoluteY;
            ++pc;
            break;
        case 1:
            op.operandLo = read(pc);
            op.inst = (op.inst << 16) | (op.operandLo << 8);
            temp = op.operandLo + y;
            op.tempAddr = temp;
            ++pc;
            break;
        case 2:
            op.operandHi = read(pc);
            op.inst |= op.operandHi;
            op.tempAddr |= op.operandHi << 8;
            op.fixedAddr = y;
            op.fixedAddr += (op.operandHi << 8) + op.operandLo;
            if (op.instType == CPUOp::ReadInst && op.tempAddr == op.fixedAddr) {
                pollInterrupts();
            }
            ++pc;
            break;
        case 3:
            if (op.instType == CPUOp::ReadInst ||
                    op.instType == CPUOp::RMWInst) {
                op.val = read(op.tempAddr);
            }
            if (op.tempAddr == op.fixedAddr) {
                op.modify = true;
            } else {
                op.tempAddr = op.fixedAddr;
                pollInterrupts();
            }
            break;
        case 4:
            if (op.instType == CPUOp::ReadInst ||
                    op.instType == CPUOp::RMWInst) {
                op.val = read(op.tempAddr);
            }
            if (!op.modify) {
                op.modify = true;
            }
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
            pollInterrupts();
            ++pc;
            break;
        case 1:
            op.modify = true;
            op.write = true;
    }
}

void CPU::idr() {
    uint8_t temp = 0;
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
    uint8_t temp = 0;
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
            temp = op.operandLo + x;
            op.tempAddr = read(temp);
            break;
        case 4:
            temp = op.operandLo + x + 1;
            op.tempAddr |= read(temp) << 8;
            if (op.instType == CPUOp::ReadInst ||
                    op.instType == CPUOp::WriteInst) {
                pollInterrupts();
            }
            break;
        case 5:
            if (op.instType == CPUOp::ReadInst ||
                    op.instType == CPUOp::RMWInst) {
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
    uint8_t temp = 0;
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
            temp = read(op.operandLo) + y;
            op.tempAddr = temp;
            break;
        case 3:
            temp = op.operandLo + 1;
            op.tempAddr |= read(temp) << 8;
            op.fixedAddr = y;
            op.fixedAddr += (read(temp) << 8) + read(op.operandLo);
            if (op.instType == CPUOp::ReadInst && op.tempAddr == op.fixedAddr) {
                pollInterrupts();
            }
            break;
        case 4:
            if (op.instType == CPUOp::ReadInst ||
                    op.instType == CPUOp::RMWInst) {
                op.val = read(op.tempAddr);
            }
            if (op.tempAddr == op.fixedAddr) {
                op.modify = true;
            } else {
                op.tempAddr = op.fixedAddr;
                pollInterrupts();
            }
            break;
        case 5:
            if (op.instType == CPUOp::ReadInst ||
                    op.instType == CPUOp::RMWInst) {
                op.val = read(op.tempAddr);
            }
            if (!op.modify) {
                op.modify = true;
            }
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
    uint8_t temp = 0;
    switch (op.cycle) {
        case 0:
            op.addrMode = CPUOp::Relative;
            pollInterrupts();
            ++pc;
            break;
        case 1:
            op.operandLo = read(pc);
            op.inst = (op.inst << 8) | op.operandLo;
            ++pc;
            break;
        case 2:
            temp = (pc & 0xff) + op.operandLo;
            op.tempAddr = (pc & 0xff00) | temp;
            op.fixedAddr = op.operandLo;
            if (op.fixedAddr & Negative) {
                op.fixedAddr |= 0xff00;
            }
            op.fixedAddr += pc;
            op.modify = true;
            if (op.tempAddr == op.fixedAddr) {
                op.clearInterruptFlags();
                op.done = true;
            } else {
                op.tempAddr = op.fixedAddr;
                pollInterrupts();
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
            if (op.instType == CPUOp::ReadInst ||
                    op.instType == CPUOp::WriteInst) {
                pollInterrupts();
            }
            ++pc;
            break;
        case 2:
            if (op.instType == CPUOp::ReadInst ||
                    op.instType == CPUOp::RMWInst) {
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
    uint8_t temp = 0;
    switch (op.cycle) {
        case 0:
            op.addrMode = CPUOp::ZeroPageX;
            ++pc;
            break;
        case 1:
            op.operandLo = read(pc);
            op.inst = (op.inst << 8) | op.operandLo;
            temp = op.operandLo + x;
            op.tempAddr = temp;
            ++pc;
            break;
        case 2:
            if (op.instType == CPUOp::ReadInst ||
                    op.instType == CPUOp::WriteInst) {
                pollInterrupts();
            }
            break;
        case 3:
            if (op.instType == CPUOp::ReadInst ||
                    op.instType == CPUOp::RMWInst) {
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
    uint8_t temp = 0;
    switch (op.cycle) {
        case 0:
            op.addrMode = CPUOp::ZeroPageY;
            ++pc;
            break;
        case 1:
            op.operandLo = read(pc);
            op.inst = (op.inst << 8) | op.operandLo;
            temp = op.operandLo + y;
            op.tempAddr = temp;
            ++pc;
            break;
        case 2:
            if (op.instType == CPUOp::ReadInst ||
                    op.instType == CPUOp::WriteInst) {
                pollInterrupts();
            }
            break;
        case 3:
            if (op.instType == CPUOp::ReadInst ||
                    op.instType == CPUOp::RMWInst) {
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
// These functions perform any opcode-specific execution that can't be easily
// done in the addressing mode functions

void CPU::adc() {
    op.instType = CPUOp::ReadInst;
    if (op.modify) {
        uint16_t temp = a + op.val + (p & Carry);
        uint8_t pastA = a;
        a += op.val + (p & Carry);
        setCarryFlag(temp > 0xff);
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

    if (op.modify && op.addrMode == CPUOp::Accumulator) {
        setCarryFlag(a & Negative);
        a = a << 1;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.done = true;
    } else if (op.writeModified) {
        write(op.tempAddr, op.val, mute);
        op.done = true;
    } else if (op.writeUnmodified) {
        write(op.tempAddr, op.val, mute);
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
    if (op.cycle == 1 && isCarry()) {
        op.done = true;
    } else if (op.modify && !isCarry()) {
        pc = op.tempAddr;
    }
}

void CPU::bcs() {
    if (op.cycle == 1 && !isCarry()) {
        op.done = true;
    } else if (op.modify && isCarry()) {
        pc = op.tempAddr;
    }
}

void CPU::beq() {
    if (op.cycle == 1 && !isZero()) {
        op.done = true;
    } else if (op.modify && isZero()) {
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
    if (op.cycle == 1 && !isNegative()) {
        op.done = true;
    } else if (op.modify && isNegative()) {
        pc = op.tempAddr;
    }
}

void CPU::bne() {
    if (op.cycle == 1 && isZero()) {
        op.done = true;
    } else if (op.modify && !isZero()) {
        pc = op.tempAddr;
    }
}

void CPU::bpl() {
    if (op.cycle == 1 && isNegative()) {
        op.done = true;
    } else if (op.modify && !isNegative()) {
        pc = op.tempAddr;
    }
}

void CPU::brk() {
    if (haltAtBrk) {
        endOfProgram = true;
    } else {
        op.irq = true;
        op.interruptPrologue = true;
        prepareIRQ(true);
    }
}

void CPU::bvc() {
    if (op.cycle == 1 && isOverflow()) {
        op.done = true;
    } else if (op.modify && !isOverflow()) {
        pc = op.tempAddr;
    }
}

void CPU::bvs() {
    if (op.cycle == 1 && !isOverflow()) {
        op.done = true;
    } else if (op.modify && isOverflow()) {
        pc = op.tempAddr;
    }
}

void CPU::clc() {
    if (op.modify) {
        setCarryFlag(false);
        op.done = true;
    }
}

void CPU::cld() {
    if (op.modify) {
        setDecimalMode(false);
        op.done = true;
    }
}

void CPU::cli() {
    if (op.modify) {
        setInterruptDisable(false);
        op.done = true;
    }
}

void CPU::clv() {
    if (op.modify) {
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
        write(op.tempAddr, op.val, mute);
        op.done = true;
    } else if (op.writeUnmodified) {
        write(op.tempAddr, op.val, mute);
        --op.val;
        updateZeroFlag(op.val);
        updateNegativeFlag(op.val);
    }
}

void CPU::dex() {
    if (op.modify) {
        --x;
        updateZeroFlag(x);
        updateNegativeFlag(x);
        op.done = true;
    }
}

void CPU::dey() {
    if (op.modify) {
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
        write(op.tempAddr, op.val, mute);
        op.done = true;
    } else if (op.writeUnmodified) {
        write(op.tempAddr, op.val, mute);
        ++op.val;
        updateZeroFlag(op.val);
        updateNegativeFlag(op.val);
    }
}

void CPU::inx() {
    if (op.modify) {
        ++x;
        updateZeroFlag(x);
        updateNegativeFlag(x);
        op.done = true;
    }
}

void CPU::iny() {
    if (op.modify) {
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
    switch (op.cycle) {
        case 1:
            if (op.addrMode == CPUOp::Absolute) {
                pollInterrupts();
            }
            break;
        case 2:
            if (op.addrMode == CPUOp::Absolute) {
                pc = op.tempAddr;
                op.done = true;
            }
            break;
        case 3:
            if (op.addrMode == CPUOp::Indirect) {
                pollInterrupts();
            }
            break;
        case 4:
            if (op.addrMode == CPUOp::Indirect) {
                pc = op.tempAddr;
                op.done = true;
            }
    }
}

void CPU::jsr() {
    uint8_t temp = 0;
    switch (op.cycle) {
        case 2:
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

    if (op.modify && op.addrMode == CPUOp::Accumulator) {
        setCarryFlag(a & Carry);
        a = a >> 1;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.done = true;
    } else if (op.writeModified) {
        write(op.tempAddr, op.val, mute);
        op.done = true;
    } else if (op.writeUnmodified) {
        write(op.tempAddr, op.val, mute);
        setCarryFlag(op.val & Carry);
        op.val = op.val >> 1;
        updateZeroFlag(op.val);
        updateNegativeFlag(op.val);
    }
}

void CPU::nop() {
    op.instType = CPUOp::ReadInst;
    if (op.modify) {
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
    uint8_t temp = 0;
    switch (op.cycle) {
        case 1:
            pollInterrupts();
            break;
        case 2:
            temp = p | Break | UnusedFlag;
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
    } else if (op.writeModified) {
        write(op.tempAddr, op.val, mute);
        op.done = true;
    } else if (op.writeUnmodified) {
        write(op.tempAddr, op.val, mute);
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
    } else if (op.writeModified) {
        write(op.tempAddr, op.val, mute);
        op.done = true;
    } else if (op.writeUnmodified) {
        write(op.tempAddr, op.val, mute);
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
        write(op.tempAddr, a & x, mute);
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
    if (op.modify) {
        setCarryFlag(true);
        op.done = true;
    }
}

void CPU::sed() {
    if (op.modify) {
        setDecimalMode(true);
        op.done = true;
    }
}

void CPU::sei() {
    if (op.modify) {
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
        write(op.tempAddr, a, mute);
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
        write(op.tempAddr, x, mute);
        op.done = true;
    }
}

void CPU::sty() {
    op.instType = CPUOp::WriteInst;
    if (op.write) {
        write(op.tempAddr, y, mute);
        op.done = true;
    }
}

void CPU::tas() {
    op.done = true;
    printUnknownOp();
}

void CPU::tax() {
    if (op.modify) {
        x = a;
        updateZeroFlag(x);
        updateNegativeFlag(x);
        op.done = true;
    }
}

void CPU::tay() {
    if (op.modify) {
        y = a;
        updateZeroFlag(y);
        updateNegativeFlag(y);
        op.done = true;
    }
}

void CPU::tsx() {
    if (op.modify) {
        x = sp;
        updateZeroFlag(x);
        updateNegativeFlag(x);
        op.done = true;
    }
}

void CPU::txa() {
    if (op.modify) {
        a = x;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.done = true;
    }
}

void CPU::txs() {
    if (op.modify) {
        sp = x;
        op.done = true;
    }
}

void CPU::tya() {
    if (op.modify) {
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

// Read/Write Functions

uint8_t CPU::read(uint16_t addr) {
    if (addr < PPUCTRL) {
        return ram.read(addr);
    } else if (addr < SQ1_VOL || addr == OAMDMA) {
        return ppu.readIO(addr, mmc, mute);
    } else if (addr < JOY1) {
        return apu.readIO(addr);
    } else if (addr == JOY2) {
        return apu.readIO(addr) | io.readIO(addr);
    } else if (addr < SAVE_WORK_RAM_START) {
        return io.readIO(addr);
    } else {
        return mmc.readPRG(addr);
    }
}

void CPU::write(uint16_t addr, uint8_t val, bool mute) {
    if (addr < PPUCTRL) {
        ram.write(addr, val, mute);
    } else if (addr < SQ1_VOL) {
        ppu.writeIO(addr, val, mmc, mute);
    } else if (addr == OAMDMA) {
        ppu.writeIO(addr, val, mmc, mute);
        op.oamDMATransfer = true;
    } else if (addr < JOY1) {
        apu.writeIO(addr, val, mute);
    } else if (addr == JOY2) {
        apu.writeIO(addr, val, mute);
        io.writeIO(addr, val, mute);
    } else if (addr < SAVE_WORK_RAM_START) {
        io.writeIO(addr, val, mute);
    } else {
        mmc.writePRG(addr, val, mute);
    }
}

void CPU::oamDMATransfer() {
    if (op.dmaCycle == 1 && totalCycles % 2 == 1) {
        --op.dmaCycle;
    } else if (op.dmaCycle > 0 && op.dmaCycle % 2 == 0) {
        unsigned int oamAddr = op.dmaCycle / 2 - 1;
        uint16_t cpuBaseAddr = ppu.readIO(OAMDMA, mmc, mute) << 8;
        uint16_t cpuAddr = cpuBaseAddr + oamAddr;
        uint8_t cpuData = read(cpuAddr);
        ppu.writeOAM(oamAddr, cpuData);
    }

    ++op.dmaCycle;

    if (op.dmaCycle == 514) {
        op.clearDMA();
    }
}

// Interrupt Functions

void CPU::pollInterrupts() {
    op.clearInterruptFlags();
    if (ppu.isNMIActive(mmc, mute)) {
        op.nmi = true;
    }
}

void CPU::prepareIRQ(bool isBrk) {
    uint8_t temp = 0;
    switch (op.cycle) {
        case 0:
            op.inst = 0;
            op.opcode = 0;
            break;
        case 1:
            ++pc;
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
            op.clearStatusFlags(false, true, true, false);
            temp = p | UnusedFlag;
            if (!isBrk) {
                temp &= ~Break;
            }
            ram.push(sp, temp, mute);
            break;
        case 5:
            op.tempAddr = read(LOWER_IRQ_ADDR);
            setInterruptDisable(true);
            break;
        case 6:
            op.tempAddr |= read(UPPER_IRQ_ADDR) << 8;
            pc = op.tempAddr;
            op.interruptPrologue = false;
            op.clearInterruptFlags();
            op.done = true;
    }
}

void CPU::prepareNMI() {
    uint8_t temp = 0;
    switch (op.cycle) {
        case 0:
            op.inst = 0;
            op.opcode = 0;
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
            op.clearStatusFlags(true, false, true, false);
            temp = p | UnusedFlag;
            temp = p & ~Break;
            ram.push(sp, temp, mute);
            break;
        case 5:
            op.tempAddr = read(LOWER_NMI_ADDR);
            setInterruptDisable(true);
            break;
        case 6:
            op.tempAddr |= read(UPPER_NMI_ADDR) << 8;
            pc = op.tempAddr;
            op.interruptPrologue = false;
            op.clearInterruptFlags();
            op.done = true;
    }
}

void CPU::prepareReset() {
    switch (op.cycle) {
        case 0:
            op.inst = 0;
            op.opcode = 0;
            break;
        case 2:
            --sp;
            break;
        case 3:
            --sp;
            break;
        case 4:
            op.clearStatusFlags(true, true, false, false);
            --sp;
            break;
        case 5:
            op.tempAddr = read(LOWER_RESET_ADDR);
            setInterruptDisable(true);
            break;
        case 6:
            op.tempAddr |= read(UPPER_RESET_ADDR) << 8;
            pc = op.tempAddr;
            op.interruptPrologue = false;
            op.clearInterruptFlags();
            op.done = true;
    }
}

// Processor Status Updates

void CPU::updateZeroFlag(uint8_t result) {
    if (result == 0) {
        p |= Zero;
    } else {
        p &= ~Zero;
    }
}

void CPU::updateNegativeFlag(uint8_t result) {
    p &= ~Negative;
    p |= result & Negative;
}

// Processor Status Getters

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

// Processor Status Setters

void CPU::setCarryFlag(bool val) {
    if (val) {
        p |= Carry;
    } else {
        p &= ~Carry;
    }
}

void CPU::setZeroFlag(bool val) {
    if (val) {
        p |= Zero;
    } else {
        p &= ~Zero;
    }
}

void CPU::setInterruptDisable(bool val) {
    if (val) {
        p |= InterruptDisable;
    } else {
        p &= ~InterruptDisable;
    }
}

void CPU::setDecimalMode(bool val) {
    if (val) {
        p |= DecimalMode;
    } else {
        p &= ~DecimalMode;
    }
}

void CPU::setOverflowFlag(bool val) {
    if (val) {
        p |= Overflow;
    } else {
        p &= ~Overflow;
    }
}

void CPU::setNegativeFlag(bool val) {
    if (val) {
        p |= Negative;
    } else {
        p &= ~Negative;
    }
}