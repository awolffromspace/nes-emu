#include "cpu.h"

CPU::CPU() :
        sp(0xfd),
        a(0),
        x(0),
        y(0),
        p(0x34),
        totalCycles(0),
        endOfProgram(false),
        haltAtBrk(false),
        mute(true) {
    // Initialize PC to the reset vector
    pc = (read(0xfffd) << 8) | read(0xfffc);
}

void CPU::clear() {
    sp = 0xfd;
    a = 0;
    x = 0;
    y = 0;
    p = 0x34;
    op.clear(true);
    ram.clear(mute);
    ppu.clear(mute);
    apu.clear(mute);
    io.clear(mute);
    mmc.clear(mute);
    pc = (read(0xfffd) << 8) | read(0xfffc);
    totalCycles = 0;
    endOfProgram = false;

    if (!mute) {
        std::cout << "CPU was cleared\n";
    }
}

void CPU::step(SDL_Renderer* renderer, SDL_Texture* texture) {
    if ((op.status & CPUOp::Done) || (totalCycles == 0)) {
        op.clear(false);

        // If any interrupts are flagged, start the interrupt prologue
        // Ignore IRQ if the Interrupt Disable flag is set
        if (((op.status & CPUOp::IRQ) && !(p & InterruptDisable)) ||
                (op.status & CPUOp::NMI) || (op.status & CPUOp::Reset)) {
            op.status |= CPUOp::InterruptPrologue;
        }

        // Fetch
        op.pc = pc;
        op.inst = read(pc);
        op.opcode = op.inst;
    }

    if (op.status & CPUOp::OAMDMA) {
        oamDMATransfer();
    } else if ((op.status & CPUOp::InterruptPrologue) &&
            (op.status & CPUOp::Reset)) {
        prepareReset();
    } else if ((op.status & CPUOp::InterruptPrologue) &&
            (op.status & CPUOp::NMI)) {
        prepareNMI();
    } else if ((op.status & CPUOp::InterruptPrologue) &&
            (op.status & CPUOp::IRQ)) {
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
        pc = (read(0xfffd) << 8) | read(0xfffc);
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
    std::bitset<10> binaryStatus(op.status);
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
        "dmaCycle    = " << op.dmaCycle << "\n"
        "status      = " << binaryStatus <<
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
            op.status |= CPUOp::Modify;
            op.status |= CPUOp::Write;
            break;
        case 4:
            op.status |= CPUOp::WriteUnmodified;
            pollInterrupts();
            break;
        case 5:
            op.status |= CPUOp::WriteModified;
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
                op.status |= CPUOp::Modify;
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
            if (!(op.status & CPUOp::Modify)) {
                op.status |= CPUOp::Modify;
            }
            op.status |= CPUOp::Write;
            break;
        case 5:
            op.status |= CPUOp::WriteUnmodified;
            pollInterrupts();
            break;
        case 6:
            op.status |= CPUOp::WriteModified;
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
                op.status |= CPUOp::Modify;
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
            if (!(op.status & CPUOp::Modify)) {
                op.status |= CPUOp::Modify;
            }
            op.status |= CPUOp::Write;
            break;
        case 5:
            op.status |= CPUOp::WriteUnmodified;
            pollInterrupts();
            break;
        case 6:
            op.status |= CPUOp::WriteModified;
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
            op.status |= CPUOp::Modify;
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
            op.status |= CPUOp::Modify;
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
            op.status |= CPUOp::Modify;
            op.status |= CPUOp::Write;
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
            op.status |= CPUOp::Modify;
            op.status |= CPUOp::Write;
            break;
        case 6:
            op.status |= CPUOp::WriteUnmodified;
            pollInterrupts();
            break;
        case 7:
            op.status |= CPUOp::WriteModified;
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
                op.status |= CPUOp::Modify;
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
            if (!(op.status & CPUOp::Modify)) {
                op.status |= CPUOp::Modify;
            }
            op.status |= CPUOp::Write;
            break;
        case 6:
            op.status |= CPUOp::WriteUnmodified;
            pollInterrupts();
            break;
        case 7:
            op.status |= CPUOp::WriteModified;
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
            op.status |= CPUOp::Modify;
            if (op.tempAddr == op.fixedAddr) {
                op.clearInterruptFlags();
                op.status |= CPUOp::Done;
            } else {
                op.tempAddr = op.fixedAddr;
                pollInterrupts();
            }
            break;
        case 3:
            op.status |= CPUOp::Done;
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
            op.status |= CPUOp::Modify;
            op.status |= CPUOp::Write;
            break;
        case 3:
            op.status |= CPUOp::WriteUnmodified;
            pollInterrupts();
            break;
        case 4:
            op.status |= CPUOp::WriteModified;
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
            op.status |= CPUOp::Modify;
            op.status |= CPUOp::Write;
            break;
        case 4:
            op.status |= CPUOp::WriteUnmodified;
            pollInterrupts();
            break;
        case 5:
            op.status |= CPUOp::WriteModified;
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
            op.status |= CPUOp::Modify;
            op.status |= CPUOp::Write;
            break;
        case 4:
            op.status |= CPUOp::WriteUnmodified;
            pollInterrupts();
            break;
        case 5:
            op.status |= CPUOp::WriteModified;
    }
}

// Operations
// These functions perform any opcode-specific execution that can't be easily
// done in the addressing mode functions

void CPU::adc() {
    op.instType = CPUOp::ReadInst;
    if (op.status & CPUOp::Modify) {
        uint16_t temp = a + op.val + (p & Carry);
        uint8_t pastA = a;
        a += op.val + (p & Carry);
        if (temp > 0xff) {
            p |= Carry;
        } else {
            p &= ~Carry;
        }
        if ((pastA ^ a) & (op.val ^ a) & Negative) {
            p |= Overflow;
        } else {
            p &= ~Overflow;
        }
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.status |= CPUOp::Done;
    }
}

void CPU::ahx() {
    op.status |= CPUOp::Done;
    printUnknownOp();
}

void CPU::alr() {
    op.status |= CPUOp::Done;
    printUnknownOp();
}

void CPU::andOp() {
    op.instType = CPUOp::ReadInst;
    if (op.status & CPUOp::Modify) {
        a &= op.val;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.status |= CPUOp::Done;
    }
}

void CPU::anc() {
    op.status |= CPUOp::Done;
    printUnknownOp();
}

void CPU::arr() {
    op.status |= CPUOp::Done;
    printUnknownOp();
}

void CPU::asl() {
    if (op.addrMode != CPUOp::Accumulator) {
        op.instType = CPUOp::RMWInst;
    }

    if ((op.status & CPUOp::Modify) && (op.addrMode == CPUOp::Accumulator)) {
        if (a & Negative) {
            p |= Carry;
        } else {
            p &= ~Carry;
        }
        a = a << 1;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.status |= CPUOp::Done;
    } else if (op.status & CPUOp::WriteModified) {
        write(op.tempAddr, op.val, mute);
        op.status |= CPUOp::Done;
    } else if (op.status & CPUOp::WriteUnmodified) {
        write(op.tempAddr, op.val, mute);
        if (op.val & Negative) {
            p |= Carry;
        } else {
            p &= ~Carry;
        }
        op.val = op.val << 1;
        updateZeroFlag(op.val);
        updateNegativeFlag(op.val);
    }
}

void CPU::axs() {
    op.status |= CPUOp::Done;
    printUnknownOp();
}

void CPU::bcc() {
    if ((op.cycle == 1) && (p & Carry)) {
        op.status |= CPUOp::Done;
    } else if ((op.status & CPUOp::Modify) && !(p & Carry)) {
        pc = op.tempAddr;
    }
}

void CPU::bcs() {
    if ((op.cycle == 1) && !(p & Carry)) {
        op.status |= CPUOp::Done;
    } else if ((op.status & CPUOp::Modify) && (p & Carry)) {
        pc = op.tempAddr;
    }
}

void CPU::beq() {
    if ((op.cycle == 1) && !(p & Zero)) {
        op.status |= CPUOp::Done;
    } else if ((op.status & CPUOp::Modify) && (p & Zero)) {
        pc = op.tempAddr;
    }
}

void CPU::bit() {
    op.instType = CPUOp::ReadInst;
    if (op.status & CPUOp::Modify) {
        uint8_t temp = a & op.val;
        if (op.val & Overflow) {
            p |= Overflow;
        } else {
            p &= ~Overflow;
        }
        updateZeroFlag(temp);
        updateNegativeFlag(op.val);
        op.status |= CPUOp::Done;
    }
}

void CPU::bmi() {
    if ((op.cycle == 1) && !(p & Negative)) {
        op.status |= CPUOp::Done;
    } else if ((op.status & CPUOp::Modify) && (p & Negative)) {
        pc = op.tempAddr;
    }
}

void CPU::bne() {
    if ((op.cycle == 1) && (p & Zero)) {
        op.status |= CPUOp::Done;
    } else if ((op.status & CPUOp::Modify) && !(p & Zero)) {
        pc = op.tempAddr;
    }
}

void CPU::bpl() {
    if ((op.cycle == 1) && (p & Negative)) {
        op.status |= CPUOp::Done;
    } else if ((op.status & CPUOp::Modify) && !(p & Negative)) {
        pc = op.tempAddr;
    }
}

void CPU::brk() {
    if (haltAtBrk) {
        endOfProgram = true;
    } else {
        op.status |= CPUOp::IRQ;
        op.status |= CPUOp::InterruptPrologue;
        prepareIRQ(true);
    }
}

void CPU::bvc() {
    if ((op.cycle == 1) && (p & Overflow)) {
        op.status |= CPUOp::Done;
    } else if ((op.status & CPUOp::Modify) && !(p & Overflow)) {
        pc = op.tempAddr;
    }
}

void CPU::bvs() {
    if ((op.cycle == 1) && !(p & Overflow)) {
        op.status |= CPUOp::Done;
    } else if ((op.status & CPUOp::Modify) && (p & Overflow)) {
        pc = op.tempAddr;
    }
}

void CPU::clc() {
    if (op.status & CPUOp::Modify) {
        p &= ~Carry;
        op.status |= CPUOp::Done;
    }
}

void CPU::cld() {
    if (op.status & CPUOp::Modify) {
        p &= ~DecimalMode;
        op.status |= CPUOp::Done;
    }
}

void CPU::cli() {
    if (op.status & CPUOp::Modify) {
        p &= ~InterruptDisable;
        op.status |= CPUOp::Done;
    }
}

void CPU::clv() {
    if (op.status & CPUOp::Modify) {
        p &= ~Overflow;
        op.status |= CPUOp::Done;
    }
}

void CPU::cmp() {
    op.instType = CPUOp::ReadInst;
    if (op.status & CPUOp::Modify) {
        uint8_t temp = a - op.val;
        if (a >= op.val) {
            p |= Carry;
        } else {
            p &= ~Carry;
        }
        updateZeroFlag(temp);
        updateNegativeFlag(temp);
        op.status |= CPUOp::Done;
    }
}

void CPU::cpx() {
    op.instType = CPUOp::ReadInst;
    if (op.status & CPUOp::Modify) {
        uint8_t temp = x - op.val;
        if (x >= op.val) {
            p |= Carry;
        } else {
            p &= ~Carry;
        }
        updateZeroFlag(temp);
        updateNegativeFlag(temp);
        op.status |= CPUOp::Done;
    }
}

void CPU::cpy() {
    op.instType = CPUOp::ReadInst;
    if (op.status & CPUOp::Modify) {
        uint8_t temp = y - op.val;
        if (y >= op.val) {
            p |= Carry;
        } else {
            p &= ~Carry;
        }
        updateZeroFlag(temp);
        updateNegativeFlag(temp);
        op.status |= CPUOp::Done;
    }
}

void CPU::dcp() {
    op.instType = CPUOp::RMWInst;
    if (op.status & CPUOp::WriteModified) {
        dec();
        cmp();
    } else if (op.status & CPUOp::WriteUnmodified) {
        dec();
    }
}

void CPU::dec() {
    op.instType = CPUOp::RMWInst;
    if (op.status & CPUOp::WriteModified) {
        write(op.tempAddr, op.val, mute);
        op.status |= CPUOp::Done;
    } else if (op.status & CPUOp::WriteUnmodified) {
        write(op.tempAddr, op.val, mute);
        --op.val;
        updateZeroFlag(op.val);
        updateNegativeFlag(op.val);
    }
}

void CPU::dex() {
    if (op.status & CPUOp::Modify) {
        --x;
        updateZeroFlag(x);
        updateNegativeFlag(x);
        op.status |= CPUOp::Done;
    }
}

void CPU::dey() {
    if (op.status & CPUOp::Modify) {
        --y;
        updateZeroFlag(y);
        updateNegativeFlag(y);
        op.status |= CPUOp::Done;
    }
}

void CPU::eor() {
    op.instType = CPUOp::ReadInst;
    if (op.status & CPUOp::Modify) {
        a ^= op.val;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.status |= CPUOp::Done;
    }
}

void CPU::inc() {
    op.instType = CPUOp::RMWInst;
    if (op.status & CPUOp::WriteModified) {
        write(op.tempAddr, op.val, mute);
        op.status |= CPUOp::Done;
    } else if (op.status & CPUOp::WriteUnmodified) {
        write(op.tempAddr, op.val, mute);
        ++op.val;
        updateZeroFlag(op.val);
        updateNegativeFlag(op.val);
    }
}

void CPU::inx() {
    if (op.status & CPUOp::Modify) {
        ++x;
        updateZeroFlag(x);
        updateNegativeFlag(x);
        op.status |= CPUOp::Done;
    }
}

void CPU::iny() {
    if (op.status & CPUOp::Modify) {
        ++y;
        updateZeroFlag(y);
        updateNegativeFlag(y);
        op.status |= CPUOp::Done;
    }
}

void CPU::isc() {
    op.instType = CPUOp::RMWInst;
    if (op.status & CPUOp::WriteModified) {
        inc();
        sbc();
    } else if (op.status & CPUOp::WriteUnmodified) {
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
                op.status |= CPUOp::Done;
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
                op.status |= CPUOp::Done;
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
            op.status |= CPUOp::Done;
    }
}

void CPU::las() {
    op.status |= CPUOp::Done;
    printUnknownOp();
}

void CPU::lax() {
    op.instType = CPUOp::ReadInst;
    if (op.status & CPUOp::Modify) {
        a = op.val;
        x = op.val;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.status |= CPUOp::Done;
    }
}

void CPU::lda() {
    op.instType = CPUOp::ReadInst;
    if (op.status & CPUOp::Modify) {
        a = op.val;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.status |= CPUOp::Done;
    }
}

void CPU::ldx() {
    op.instType = CPUOp::ReadInst;
    if (op.status & CPUOp::Modify) {
        x = op.val;
        updateZeroFlag(x);
        updateNegativeFlag(x);
        op.status |= CPUOp::Done;
    }
}

void CPU::ldy() {
    op.instType = CPUOp::ReadInst;
    if (op.status & CPUOp::Modify) {
        y = op.val;
        updateZeroFlag(y);
        updateNegativeFlag(y);
        op.status |= CPUOp::Done;
    }
}

void CPU::lsr() {
    if (op.addrMode != CPUOp::Accumulator) {
        op.instType = CPUOp::RMWInst;
    }

    if ((op.status & CPUOp::Modify) && (op.addrMode == CPUOp::Accumulator)) {
        if (a & Carry) {
            p |= Carry;
        } else {
            p &= ~Carry;
        }
        a = a >> 1;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.status |= CPUOp::Done;
    } else if (op.status & CPUOp::WriteModified) {
        write(op.tempAddr, op.val, mute);
        op.status |= CPUOp::Done;
    } else if (op.status & CPUOp::WriteUnmodified) {
        write(op.tempAddr, op.val, mute);
        if (op.val & Carry) {
            p |= Carry;
        } else {
            p &= ~Carry;
        }
        op.val = op.val >> 1;
        updateZeroFlag(op.val);
        updateNegativeFlag(op.val);
    }
}

void CPU::nop() {
    op.instType = CPUOp::ReadInst;
    if (op.status & CPUOp::Modify) {
        op.status |= CPUOp::Done;
    }
}

void CPU::ora() {
    op.instType = CPUOp::ReadInst;
    if (op.status & CPUOp::Modify) {
        a |= op.val;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.status |= CPUOp::Done;
    }
}

void CPU::pha() {
    switch (op.cycle) {
        case 1:
            pollInterrupts();
            break;
        case 2:
            ram.push(sp, a, mute);
            op.status |= CPUOp::Done;
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
            op.status |= CPUOp::Done;
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
            op.status |= CPUOp::Done;
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
            op.status |= CPUOp::Done;
    }
}

void CPU::rla() {
    op.instType = CPUOp::RMWInst;
    if (op.status & CPUOp::WriteModified) {
        rol();
        andOp();
    } else if (op.status & CPUOp::WriteUnmodified) {
        rol();
    }
}

void CPU::rol() {
    if (op.addrMode != CPUOp::Accumulator) {
        op.instType = CPUOp::RMWInst;
    }

    if ((op.status & CPUOp::Modify) && (op.addrMode == CPUOp::Accumulator)) {
        uint8_t temp = a << 1;
        if (p & Carry) {
            temp |= Carry;
        }
        if (a & Negative) {
            p |= Carry;
        } else {
            p &= ~Carry;
        }
        a = temp;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.status |= CPUOp::Done;
    } else if (op.status & CPUOp::WriteModified) {
        write(op.tempAddr, op.val, mute);
        op.status |= CPUOp::Done;
    } else if (op.status & CPUOp::WriteUnmodified) {
        write(op.tempAddr, op.val, mute);
        uint8_t temp = op.val << 1;
        if (p & Carry) {
            temp |= Carry;
        }
        if (op.val & Negative) {
            p |= Carry;
        } else {
            p &= ~Carry;
        }
        op.val = temp;
        updateZeroFlag(op.val);
        updateNegativeFlag(op.val);
    }
}

void CPU::ror() {
    if (op.addrMode != CPUOp::Accumulator) {
        op.instType = CPUOp::RMWInst;
    }

    if ((op.status & CPUOp::Modify) && (op.addrMode == CPUOp::Accumulator)) {
        uint8_t temp = a >> 1;
        if (p & Carry) {
            temp |= Negative;
        }
        if (a & Carry) {
            p |= Carry;
        } else {
            p &= ~Carry;
        }
        a = temp;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.status |= CPUOp::Done;
    } else if (op.status & CPUOp::WriteModified) {
        write(op.tempAddr, op.val, mute);
        op.status |= CPUOp::Done;
    } else if (op.status & CPUOp::WriteUnmodified) {
        write(op.tempAddr, op.val, mute);
        uint8_t temp = op.val >> 1;
        if (p & Carry) {
            temp |= Negative;
        }
        if (op.val & Carry) {
            p |= Carry;
        } else {
            p &= ~Carry;
        }
        op.val = temp;
        updateZeroFlag(op.val);
        updateNegativeFlag(op.val);
    }
}

void CPU::rra() {
    op.instType = CPUOp::RMWInst;
    if (op.status & CPUOp::WriteModified) {
        ror();
        adc();
    } else if (op.status & CPUOp::WriteUnmodified) {
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
            op.status |= CPUOp::Done;
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
            op.status |= CPUOp::Done;
    }
}

void CPU::sax() {
    op.instType = CPUOp::WriteInst;
    if (op.status & CPUOp::Write) {
        write(op.tempAddr, a & x, mute);
        op.status |= CPUOp::Done;
    }
}

void CPU::sbc() {
    op.instType = CPUOp::ReadInst;
    if (op.status & CPUOp::Modify) {
        op.val = 0xff - op.val;
        adc();
    }
}

void CPU::sec() {
    if (op.status & CPUOp::Modify) {
        p |= Carry;
        op.status |= CPUOp::Done;
    }
}

void CPU::sed() {
    if (op.status & CPUOp::Modify) {
        p |= DecimalMode;
        op.status |= CPUOp::Done;
    }
}

void CPU::sei() {
    if (op.status & CPUOp::Modify) {
        p |= InterruptDisable;
        op.status |= CPUOp::Done;
    }
}

void CPU::shx() {
    op.status |= CPUOp::Done;
    printUnknownOp();
}

void CPU::shy() {
    op.status |= CPUOp::Done;
    printUnknownOp();
}

void CPU::slo() {
    op.instType = CPUOp::RMWInst;
    if (op.status & CPUOp::WriteModified) {
        asl();
        ora();
    } else if (op.status & CPUOp::WriteUnmodified) {
        asl();
    }
}

void CPU::sre() {
    op.instType = CPUOp::RMWInst;
    if (op.status & CPUOp::WriteModified) {
        lsr();
        eor();
    } else if (op.status & CPUOp::WriteUnmodified) {
        lsr();
    }
}

void CPU::sta() {
    op.instType = CPUOp::WriteInst;
    if (op.status & CPUOp::Write) {
        write(op.tempAddr, a, mute);
        op.status |= CPUOp::Done;
    }
}

void CPU::stp() {
    op.status |= CPUOp::Done;
    printUnknownOp();
}

void CPU::stx() {
    op.instType = CPUOp::WriteInst;
    if (op.status & CPUOp::Write) {
        write(op.tempAddr, x, mute);
        op.status |= CPUOp::Done;
    }
}

void CPU::sty() {
    op.instType = CPUOp::WriteInst;
    if (op.status & CPUOp::Write) {
        write(op.tempAddr, y, mute);
        op.status |= CPUOp::Done;
    }
}

void CPU::tas() {
    op.status |= CPUOp::Done;
    printUnknownOp();
}

void CPU::tax() {
    if (op.status & CPUOp::Modify) {
        x = a;
        updateZeroFlag(x);
        updateNegativeFlag(x);
        op.status |= CPUOp::Done;
    }
}

void CPU::tay() {
    if (op.status & CPUOp::Modify) {
        y = a;
        updateZeroFlag(y);
        updateNegativeFlag(y);
        op.status |= CPUOp::Done;
    }
}

void CPU::tsx() {
    if (op.status & CPUOp::Modify) {
        x = sp;
        updateZeroFlag(x);
        updateNegativeFlag(x);
        op.status |= CPUOp::Done;
    }
}

void CPU::txa() {
    if (op.status & CPUOp::Modify) {
        a = x;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.status |= CPUOp::Done;
    }
}

void CPU::txs() {
    if (op.status & CPUOp::Modify) {
        sp = x;
        op.status |= CPUOp::Done;
    }
}

void CPU::tya() {
    if (op.status & CPUOp::Modify) {
        a = y;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.status |= CPUOp::Done;
    }
}

void CPU::xaa() {
    op.status |= CPUOp::Done;
    printUnknownOp();
}

// Read/Write Functions

uint8_t CPU::read(uint16_t addr) {
    if (addr < 0x2000) {
        return ram.read(addr);
    } else if (addr < 0x4000 || addr == 0x4014) {
        return ppu.readIO(addr, mmc, mute);
    } else if (addr < 0x4016) {
        return apu.readIO(addr);
    } else if (addr == 0x4017) {
        return apu.readIO(addr) | io.readIO(addr);
    } else if (addr < 0x4020) {
        return io.readIO(addr);
    } else {
        return mmc.readPRG(addr);
    }
}

void CPU::write(uint16_t addr, uint8_t val, bool mute) {
    if (addr < 0x2000) {
        ram.write(addr, val, mute);
    } else if (addr < 0x4000) {
        ppu.writeIO(addr, val, mmc, mute);
    } else if (addr == 0x4014) {
        ppu.writeIO(addr, val, mmc, mute);
        op.status |= CPUOp::OAMDMA;
    } else if (addr < 0x4016) {
        apu.writeIO(addr, val, mute);
    } else if (addr == 0x4017) {
        apu.writeIO(addr, val, mute);
        io.writeIO(addr, val, mute);
    } else if (addr < 0x4020) {
        io.writeIO(addr, val, mute);
    } else {
        mmc.writePRG(addr, val, mute);
    }
}

void CPU::oamDMATransfer() {
    if ((op.dmaCycle == 1) && (totalCycles % 2 == 1)) {
        --op.dmaCycle;
    } else if ((op.dmaCycle > 0) && (op.dmaCycle % 2 == 0)) {
        unsigned int oamAddr = op.dmaCycle / 2 - 1;
        uint16_t cpuBaseAddr = ppu.readIO(0x4014, mmc, mute) << 8;
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
        op.status |= CPUOp::NMI;
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
            op.status &= 0x90;
            temp = p | UnusedFlag;
            if (!isBrk) {
                temp &= ~Break;
            }
            ram.push(sp, temp, mute);
            break;
        case 5:
            op.tempAddr = read(0xfffe);
            p |= InterruptDisable;
            break;
        case 6:
            op.tempAddr |= read(0xffff) << 8;
            pc = op.tempAddr;
            op.status &= ~CPUOp::InterruptPrologue;
            op.clearInterruptFlags();
            op.status |= CPUOp::Done;
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
            op.status &= 0xa0;
            temp = p | UnusedFlag;
            temp = p & ~Break;
            ram.push(sp, temp, mute);
            break;
        case 5:
            op.tempAddr = read(0xfffa);
            p |= InterruptDisable;
            break;
        case 6:
            op.tempAddr |= read(0xfffb) << 8;
            pc = op.tempAddr;
            op.status &= ~CPUOp::InterruptPrologue;
            op.clearInterruptFlags();
            op.status |= CPUOp::Done;
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
            op.status &= 0xc0;
            --sp;
            break;
        case 5:
            op.tempAddr = read(0xfffc);
            p |= InterruptDisable;
            break;
        case 6:
            op.tempAddr |= read(0xfffd) << 8;
            pc = op.tempAddr;
            op.status &= ~CPUOp::InterruptPrologue;
            op.clearInterruptFlags();
            op.status |= CPUOp::Done;
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