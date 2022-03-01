#include "cpu.h"

CPU::CPU() :
        sp(0xff),
        a(0),
        x(0),
        y(0),
        p(0x20),
        totalCycles(0),
        endOfProgram(false),
        haltAtBrk(false),
        mute(true) {
    // Set the reset vector to 0x8000, the beginning of the PRG-ROM lower bank
    // This is where test programs will start at
    mem.write(0xfffd, 0x80, true);
    // Initialize PC to the reset vector
    pc = (mem.read(0xfffd) << 8) | mem.read(0xfffc);
}

void CPU::reset() {
    pc = 0x8000;
    sp = 0xff;
    a = 0;
    x = 0;
    y = 0;
    p = 0x20;
    op.reset();
    mem.reset(mute);
    totalCycles = 0;
    endOfProgram = false;

    if (!mute) {
        std::cout << "CPU was reset\n";
    }
}

void CPU::step() {
    if ((op.status & Op::Done) || (totalCycles == 0)) {
        op.reset();

        if ((op.status & Op::IRQ) && !(p & 0x4)) {
            op.status |= Op::InterruptPrologue;
        } else if ((op.status & Op::NMI) || (op.status & Op::Reset)) {
            op.status |= Op::InterruptPrologue;
        }

        // Fetch
        op.pc = pc;
        op.inst = mem.read(pc);
        op.opcode = op.inst;
    }

    if (op.status & Op::InterruptPrologue) {
        prepareInterrupt();
    } else {
        // Decode and execute
        (this->*addrModeArr[op.opcode])();
        (this->*opcodeArr[op.opcode])();
    }

    ++op.cycles;
    ++totalCycles;
}

void CPU::readInInst(std::string filename) {
    mem.readInInst(filename);
}

bool CPU::isEndOfProgram() {
    return endOfProgram;
}

bool CPU::compareState(struct CPUState& state) {
    if (state.pc != pc || state.sp != sp || state.a != a || state.x != x ||
            state.y != y || state.p != p || state.totalCycles != totalCycles) {
        return false;
    }
    return true;
}

void CPU::setHaltAtBrk(bool h) {
    haltAtBrk = h;
}

void CPU::setMute(bool m) {
    mute = m;
}

// Addressing Modes
// These functions prepare the operation functions as much as possible for
// execution

void CPU::abs() {
    switch (op.cycles) {
        case 0:
            op.addrMode = Op::Absolute;
            ++pc;
            break;
        case 1:
            op.operandLo = mem.read(pc);
            op.inst = (op.inst << 16) | (op.operandLo << 8);
            op.tempAddr = op.operandLo;
            ++pc;
            break;
        case 2:
            op.operandHi = mem.read(pc);
            op.inst |= op.operandHi;
            op.tempAddr |= op.operandHi << 8;
            ++pc;
            break;
        case 3:
            op.val = mem.read(op.tempAddr);
            op.status |= Op::Modify;
            op.status |= Op::Write;
            break;
        case 4:
            op.status |= Op::WriteUnmodified;
            break;
        case 5:
            op.status |= Op::WriteModified;
    }
}

void CPU::abx() {
    uint8_t temp = 0;
    uint16_t fixedAddr = 0;
    switch (op.cycles) {
        case 0:
            op.addrMode = Op::AbsoluteX;
            ++pc;
            break;
        case 1:
            op.operandLo = mem.read(pc);
            op.inst = (op.inst << 16) | (op.operandLo << 8);
            temp = op.operandLo + x;
            op.tempAddr = temp;
            ++pc;
            break;
        case 2:
            op.operandHi = mem.read(pc);
            op.inst |= op.operandHi;
            op.tempAddr |= op.operandHi << 8;
            ++pc;
            break;
        case 3:
            op.val = mem.read(op.tempAddr);
            fixedAddr = x;
            if (fixedAddr & 0x80) {
                fixedAddr |= 0xff00;
            }
            fixedAddr += (op.operandHi << 8) + op.operandLo;
            if (op.tempAddr == fixedAddr) {
                op.status |= Op::Modify;
            } else {
                op.tempAddr = fixedAddr;
            }
            break;
        case 4:
            op.val = mem.read(op.tempAddr);
            if (!(op.status & Op::Modify)) {
                op.status |= Op::Modify;
            }
            op.status |= Op::Write;
            break;
        case 5:
            op.status |= Op::WriteUnmodified;
            break;
        case 6:
            op.status |= Op::WriteModified;
    }
}

void CPU::aby() {
    uint8_t temp = 0;
    uint16_t fixedAddr = 0;
    switch (op.cycles) {
        case 0:
            op.addrMode = Op::AbsoluteY;
            ++pc;
            break;
        case 1:
            op.operandLo = mem.read(pc);
            op.inst = (op.inst << 16) | (op.operandLo << 8);
            temp = op.operandLo + y;
            op.tempAddr = temp;
            ++pc;
            break;
        case 2:
            op.operandHi = mem.read(pc);
            op.inst |= op.operandHi;
            op.tempAddr |= op.operandHi << 8;
            ++pc;
            break;
        case 3:
            op.val = mem.read(op.tempAddr);
            fixedAddr = y;
            if (fixedAddr & 0x80) {
                fixedAddr |= 0xff00;
            }
            fixedAddr += (op.operandHi << 8) + op.operandLo;
            if (op.tempAddr == fixedAddr) {
                op.status |= Op::Modify;
            } else {
                op.tempAddr = fixedAddr;
            }
            break;
        case 4:
            op.val = mem.read(op.tempAddr);
            if (!(op.status & Op::Modify)) {
                op.status |= Op::Modify;
            }
            op.status |= Op::Write;
    }
}

void CPU::acc() {
    switch (op.cycles) {
        case 0:
            op.addrMode = Op::Accumulator;
            ++pc;
            break;
        case 1:
            op.val = a;
            op.status |= Op::Modify;
    }
}

void CPU::imm() {
    switch (op.cycles) {
        case 0:
            op.addrMode = Op::Immediate;
            ++pc;
            break;
        case 1:
            op.operandLo = mem.read(pc);
            op.inst = (op.inst << 8) | op.operandLo;
            op.val = op.operandLo;
            op.status |= Op::Modify;
            ++pc;
    }
}

void CPU::imp() {
    switch (op.cycles) {
        case 0:
            op.addrMode = Op::Implied;
            ++pc;
            break;
        case 1:
            op.status |= Op::Modify;
            op.status |= Op::Write;
    }
}

void CPU::idr() {
    uint8_t temp = 0;
    switch (op.cycles) {
        case 0:
            op.addrMode = Op::Indirect;
            ++pc;
            break;
        case 1:
            op.operandLo = mem.read(pc);
            op.inst = (op.inst << 16) | (op.operandLo << 8);
            ++pc;
            break;
        case 2:
            op.operandHi = mem.read(pc);
            op.inst |= op.operandHi;
            ++pc;
            break;
        case 3:
            op.tempAddr = mem.read((op.operandHi << 8) | op.operandLo);
            break;
        case 4:
            temp = op.operandLo + 1;
            op.tempAddr |= mem.read((op.operandHi << 8) | temp) << 8;
            op.val = mem.read(op.tempAddr);
    }
}

void CPU::idx() {
    uint8_t temp = 0;
    switch (op.cycles) {
        case 0:
            op.addrMode = Op::IndirectX;
            ++pc;
            break;
        case 1:
            op.operandLo = mem.read(pc);
            op.inst = (op.inst << 8) | op.operandLo;
            ++pc;
            break;
        case 3:
            temp = op.operandLo + x;
            op.tempAddr = mem.read(temp);
            break;
        case 4:
            temp = op.operandLo + x + 1;
            op.tempAddr |= mem.read(temp) << 8;
            break;
        case 5:
            op.val = mem.read(op.tempAddr);
            op.status |= Op::Modify;
            op.status |= Op::Write;
            break;
        case 6:
            op.status |= Op::WriteUnmodified;
            break;
        case 7:
            op.status |= Op::WriteModified;
    }
}

void CPU::idy() {
    uint8_t temp = 0;
    uint16_t fixedAddr = 0;
    switch (op.cycles) {
        case 0:
            op.addrMode = Op::IndirectY;
            ++pc;
            break;
        case 1:
            op.operandLo = mem.read(pc);
            op.inst = (op.inst << 8) | op.operandLo;
            ++pc;
            break;
        case 2:
            temp = mem.read(op.operandLo) + y;
            op.tempAddr = temp;
            break;
        case 3:
            temp = op.operandLo + 1;
            op.tempAddr |= mem.read(temp) << 8;
            break;
        case 4:
            op.val = mem.read(op.tempAddr);
            temp = op.operandLo + 1;
            fixedAddr = y;
            if (fixedAddr & 0x80) {
                fixedAddr |= 0xff00;
            }
            fixedAddr += (mem.read(temp) << 8) + mem.read(op.operandLo);
            if (op.tempAddr == fixedAddr) {
                op.status |= Op::Modify;
            } else {
                op.tempAddr = fixedAddr;
            }
            break;
        case 5:
            op.val = mem.read(op.tempAddr);
            if (!(op.status & Op::Modify)) {
                op.status |= Op::Modify;
            }
            op.status |= Op::Write;
            break;
        case 6:
            op.status |= Op::WriteUnmodified;
            break;
        case 7:
            op.status |= Op::WriteModified;
    }
}

void CPU::rel() {
    uint8_t temp = 0;
    uint16_t fixedAddr = 0;
    switch (op.cycles) {
        case 0:
            op.addrMode = Op::Relative;
            ++pc;
            break;
        case 1:
            op.operandLo = mem.read(pc);
            op.inst = (op.inst << 8) | op.operandLo;
            ++pc;
            break;
        case 2:
            temp = (pc & 0xff) + op.operandLo;
            op.tempAddr = (pc & 0xff00) | temp;
            fixedAddr = op.operandLo;
            if (fixedAddr & 0x80) {
                fixedAddr |= 0xff00;
            }
            fixedAddr += pc;
            op.status |= Op::Modify;
            if (op.tempAddr == fixedAddr) {
                op.status |= Op::Done;
            } else {
                op.tempAddr = fixedAddr;
            }
            break;
        case 3:
            op.status |= Op::Done;
    }
}

void CPU::zpg() {
    switch (op.cycles) {
        case 0:
            op.addrMode = Op::ZeroPage;
            ++pc;
            break;
        case 1:
            op.operandLo = mem.read(pc);
            op.tempAddr = op.operandLo;
            ++pc;
            break;
        case 2:
            op.val = mem.read(op.tempAddr);
            op.status |= Op::Modify;
            op.status |= Op::Write;
            break;
        case 3:
            op.status |= Op::WriteUnmodified;
            break;
        case 4:
            op.status |= Op::WriteModified;
    }
}

void CPU::zpx() {
    uint8_t temp = 0;
    switch (op.cycles) {
        case 0:
            op.addrMode = Op::ZeroPageX;
            ++pc;
            break;
        case 1:
            op.operandLo = mem.read(pc);
            temp = op.operandLo + x;
            op.tempAddr = temp;
            ++pc;
            break;
        case 3:
            op.val = mem.read(op.tempAddr);
            op.status |= Op::Modify;
            op.status |= Op::Write;
            break;
        case 4:
            op.status |= Op::WriteUnmodified;
            break;
        case 5:
            op.status |= Op::WriteModified;
    }
}

void CPU::zpy() {
    uint8_t temp = 0;
    switch (op.cycles) {
        case 0:
            op.addrMode = Op::ZeroPageY;
            ++pc;
            break;
        case 1:
            op.operandLo = mem.read(pc);
            temp = op.operandLo + y;
            op.tempAddr = temp;
            ++pc;
            break;
        case 3:
            op.val = mem.read(op.tempAddr);
            op.status |= Op::Modify;
            op.status |= Op::Write;
    }
}

// Operations
// These functions perform any opcode-specific execution that can't be easily
// done in the addressing mode functions

void CPU::adc() {
    if (op.status & Op::Modify) {
        uint16_t temp = a + op.val + (p & 1);
        uint8_t pastA = a;
        a += op.val + (p & 1);
        if (temp > 0xff) {
            p |= 1;
        } else {
            p &= 0xfe;
        }
        if ((pastA ^ a) & (op.val ^ a) & 0x80) {
            p |= 0x40;
        } else {
            p &= 0xbf;
        }
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.status |= Op::Done;
    }
}

void CPU::ahx() {
    printUnknownOp();
}

void CPU::alr() {
    printUnknownOp();
}

void CPU::andOp() {
    if (op.status & Op::Modify) {
        a &= op.val;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.status |= Op::Done;
    }
}

void CPU::anc() {
    printUnknownOp();
}

void CPU::arr() {
    printUnknownOp();
}

void CPU::asl() {
    if (op.status & Op::WriteUnmodified) {
        mem.write(op.tempAddr, op.val, mute);
        if (op.val & 0x80) {
            p |= 1;
        } else {
            p &= 0xfe;
        }
        op.val = op.val << 1;
        updateZeroFlag(op.val);
        updateNegativeFlag(op.val);
    } else if (op.status & Op::WriteModified) {
        mem.write(op.tempAddr, op.val, mute);
        op.status |= Op::Done;
    }
}

void CPU::axs() {
    printUnknownOp();
}

void CPU::bcc() {
    if (op.status & Op::Modify) {
        if (!(p & 1)) {
            pc = op.tempAddr;
        } else {
            op.status |= Op::Done;
        }
    }
}

void CPU::bcs() {
    if (op.status & Op::Modify) {
        if (p & 1) {
            pc = op.tempAddr;
        } else {
            op.status |= Op::Done;
        }
    }
}

void CPU::beq() {
    if (op.status & Op::Modify) {
        if (p & 2) {
            pc = op.tempAddr;
        } else {
            op.status |= Op::Done;
        }
    }
}

void CPU::bit() {
    if (op.status & Op::Modify) {
        uint8_t temp = a & op.val;
        if (op.val & 0x40) {
            p |= 0x40;
        } else {
            p &= 0xbf;
        }
        updateZeroFlag(temp);
        updateNegativeFlag(op.val);
        op.status |= Op::Done;
    }
}

void CPU::bmi() {
    if (op.status & Op::Modify) {
        if (p & 0x80) {
            pc = op.tempAddr;
        } else {
            op.status |= Op::Done;
        }
    }
}

void CPU::bne() {
    if (op.status & Op::Modify) {
        if (!(p & 2)) {
            pc = op.tempAddr;
        } else {
            op.status |= Op::Done;
        }
    }
}

void CPU::bpl() {
    if (op.status & Op::Modify) {
        if (!(p & 0x80)) {
            pc = op.tempAddr;
        } else {
            op.status |= Op::Done;
        }
    }
}

void CPU::brk() {
    p |= 0x10;
    if (haltAtBrk) {
        endOfProgram = true;
        --op.cycles;
        --totalCycles;
    } else {
        op.status |= Op::IRQ;
        op.status |= Op::InterruptPrologue;
        prepareInterrupt();
    }
}

void CPU::bvc() {
    if (op.status & Op::Modify) {
        if (!(p & 0x40)) {
            pc = op.tempAddr;
        } else {
            op.status |= Op::Done;
        }
    }
}

void CPU::bvs() {
    if (op.status & Op::Modify) {
        if (p & 0x40) {
            pc = op.tempAddr;
        } else {
            op.status |= Op::Done;
        }
    }
}

void CPU::clc() {
    if (op.status & Op::Modify) {
        p &= 0xfe;
        op.status |= Op::Done;
    }
}

void CPU::cld() {
    if (op.status & Op::Modify) {
        p &= 0xf7;
        op.status |= Op::Done;
    }
}

void CPU::cli() {
    if (op.status & Op::Modify) {
        p &= 0xfb;
        op.status |= Op::Done;
    }
}

void CPU::clv() {
    if (op.status & Op::Modify) {
        p &= 0xbf;
        op.status |= Op::Done;
    }
}

void CPU::cmp() {
    if (op.status & Op::Modify) {
        uint8_t temp = a - op.val;
        if (a >= op.val) {
            p |= 1;
        } else {
            p &= 0xfe;
        }
        updateZeroFlag(temp);
        updateNegativeFlag(temp);
        op.status |= Op::Done;
    }
}

void CPU::cpx() {
    if (op.status & Op::Modify) {
        uint8_t temp = x - op.val;
        if (x >= op.val) {
            p |= 1;
        } else {
            p &= 0xfe;
        }
        updateZeroFlag(temp);
        updateNegativeFlag(temp);
        op.status |= Op::Done;
    }
}

void CPU::cpy() {
    if (op.status & Op::Modify) {
        uint8_t temp = y - op.val;
        if (y >= op.val) {
            p |= 1;
        } else {
            p &= 0xfe;
        }
        updateZeroFlag(temp);
        updateNegativeFlag(temp);
        op.status |= Op::Done;
    }
}

void CPU::dcp() {
    printUnknownOp();
}

void CPU::dec() {
    if (op.status & Op::WriteUnmodified) {
        mem.write(op.tempAddr, op.val, mute);
        --op.val;
        updateZeroFlag(op.val);
        updateNegativeFlag(op.val);
    } else if (op.status & Op::WriteModified) {
        mem.write(op.tempAddr, op.val, mute);
        op.status |= Op::Done;
    }
}

void CPU::dex() {
    if (op.status & Op::Modify) {
        --x;
        updateZeroFlag(x);
        updateNegativeFlag(x);
        op.status |= Op::Done;
    }
}

void CPU::dey() {
    if (op.status & Op::Modify) {
        --y;
        updateZeroFlag(y);
        updateNegativeFlag(y);
        op.status |= Op::Done;
    }
}

void CPU::eor() {
    if (op.status & Op::Modify) {
        a ^= op.val;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.status |= Op::Done;
    }
}

void CPU::inc() {
    if (op.status & Op::WriteUnmodified) {
        mem.write(op.tempAddr, op.val, mute);
        ++op.val;
        updateZeroFlag(op.val);
        updateNegativeFlag(op.val);
    } else if (op.status & Op::WriteModified) {
        mem.write(op.tempAddr, op.val, mute);
        op.status |= Op::Done;
    }
}

void CPU::inx() {
    if (op.status & Op::Modify) {
        ++x;
        updateZeroFlag(x);
        updateNegativeFlag(x);
        op.status |= Op::Done;
    }
}

void CPU::iny() {
    if (op.status & Op::Modify) {
        ++y;
        updateZeroFlag(y);
        updateNegativeFlag(y);
        op.status |= Op::Done;
    }
}

void CPU::isc() {
    printUnknownOp();
}

void CPU::jmp() {
    switch (op.cycles) {
        case 2:
            if (op.addrMode == Op::Absolute) {
                pc = op.tempAddr;
                op.status |= Op::Done;
            }
            break;
        case 4:
            if (op.addrMode == Op::Indirect) {
                pc = op.tempAddr;
                op.status |= Op::Done;
            }
    }
}

void CPU::jsr() {
    uint8_t temp = 0;
    switch (op.cycles) {
        case 2:
            --pc;
            break;
        case 3:
            temp = (pc & 0xff00) >> 8;
            mem.push(sp, temp, mute);
            break;
        case 4:
            temp = pc & 0xff;
            mem.push(sp, temp, mute);
            break;
        case 5:
            pc = op.tempAddr;
            op.status |= Op::Done;
    }
}

void CPU::las() {
    printUnknownOp();
}

void CPU::lax() {
    printUnknownOp();
}

void CPU::lda() {
    if (op.status & Op::Modify) {
        a = op.val;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.status |= Op::Done;
    }
}

void CPU::ldx() {
    if (op.status & Op::Modify) {
        x = op.val;
        updateZeroFlag(x);
        updateNegativeFlag(x);
        op.status |= Op::Done;
    }
}

void CPU::ldy() {
    if (op.status & Op::Modify) {
        y = op.val;
        updateZeroFlag(y);
        updateNegativeFlag(y);
        op.status |= Op::Done;
    }
}

void CPU::lsr() {
    if (op.status & Op::WriteUnmodified) {
        mem.write(op.tempAddr, op.val, mute);
        if (op.val & 1) {
            p |= 1;
        } else {
            p &= 0xfe;
        }
        op.val = op.val >> 1;
        updateZeroFlag(op.val);
        updateNegativeFlag(op.val);
    } else if (op.status & Op::WriteModified) {
        mem.write(op.tempAddr, op.val, mute);
        op.status |= Op::Done;
    }
}

void CPU::nop() {
    if (op.status & Op::Modify) {
        op.status |= Op::Done;
    }
}

void CPU::ora() {
    if (op.status & Op::Modify) {
        a |= op.val;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.status |= Op::Done;
    }
}

void CPU::pha() {
    if (op.cycles == 2) {
        mem.push(sp, a, mute);
        op.status |= Op::Done;
    }
}

void CPU::php() {
    if (op.cycles == 2) {
        mem.push(sp, p, mute);
        op.status |= Op::Done;
    }
}

void CPU::pla() {
    switch (op.cycles) {
        case 2:
            op.val = mem.pull(sp, mute);
            break;
        case 3:
            a = op.val;
            op.status |= Op::Done;
    }
}

void CPU::plp() {
    switch (op.cycles) {
        case 2:
            op.val = mem.pull(sp, mute);
            break;
        case 3:
            p = op.val;
            op.status |= Op::Done;
    }
}

void CPU::rla() {
    printUnknownOp();
}

void CPU::rol() {
    if (op.status & Op::WriteUnmodified) {
        mem.write(op.tempAddr, op.val, mute);
        uint8_t temp = op.val << 1;
        if (p & 1) {
            temp |= 1;
        }
        if (op.val & 0x80) {
            p |= 1;
        } else {
            p &= 0xfe;
        }
        op.val = temp;
        updateZeroFlag(op.val);
        updateNegativeFlag(op.val);
    } else if (op.status & Op::WriteModified) {
        mem.write(op.tempAddr, op.val, mute);
        op.status |= Op::Done;
    }
}

void CPU::ror() {
    if (op.status & Op::WriteUnmodified) {
        mem.write(op.tempAddr, op.val, mute);
        uint8_t temp = op.val >> 1;
        if (p & 1) {
            temp |= 0x80;
        }
        if (op.val & 1) {
            p |= 1;
        } else {
            p &= 0xfe;
        }
        op.val = temp;
        updateZeroFlag(op.val);
        updateNegativeFlag(op.val);
    } else if (op.status & Op::WriteModified) {
        mem.write(op.tempAddr, op.val, mute);
        op.status |= Op::Done;
    }
}

void CPU::rra() {
    printUnknownOp();
}

void CPU::rti() {
    printUnknownOp();
}

void CPU::rts() {
    switch (op.cycles) {
        case 3:
            op.tempAddr = mem.pull(sp, mute);
            break;
        case 4:
            op.tempAddr |= mem.pull(sp, mute) << 8;
            break;
        case 5:
            pc = op.tempAddr + 1;
            op.status |= Op::Done;
    }
}

void CPU::sax() {
    printUnknownOp();
}

void CPU::sbc() {
    if (op.status & Op::Modify) {
        op.val = 0xff - op.val;
        adc();
    }
}

void CPU::sec() {
    if (op.status & Op::Modify) {
        p |= 1;
        op.status |= Op::Done;
    }
}

void CPU::sed() {
    if (op.status & Op::Modify) {
        p |= 8;
        op.status |= Op::Done;
    }
}

void CPU::sei() {
    if (op.status & Op::Modify) {
        p |= 4;
        op.status |= Op::Done;
    }
}

void CPU::shx() {
    printUnknownOp();
}

void CPU::shy() {
    printUnknownOp();
}

void CPU::slo() {
    printUnknownOp();
}

void CPU::sre() {
    printUnknownOp();
}

void CPU::sta() {
    if (op.status & Op::Write) {
        mem.write(op.tempAddr, a, mute);
        op.status |= Op::Done;
    }
}

void CPU::stp() {
    printUnknownOp();
}

void CPU::stx() {
    if (op.status & Op::Write) {
        mem.write(op.tempAddr, x, mute);
        op.status |= Op::Done;
    }
}

void CPU::sty() {
    if (op.status & Op::Write) {
        mem.write(op.tempAddr, y, mute);
        op.status |= Op::Done;
    }
}

void CPU::tas() {
    printUnknownOp();
}

void CPU::tax() {
    if (op.status & Op::Modify) {
        x = a;
        updateZeroFlag(x);
        updateNegativeFlag(x);
        op.status |= Op::Done;
    }
}

void CPU::tay() {
    if (op.status & Op::Modify) {
        y = a;
        updateZeroFlag(y);
        updateNegativeFlag(y);
        op.status |= Op::Done;
    }
}

void CPU::tsx() {
    if (op.status & Op::Modify) {
        x = sp;
        updateZeroFlag(x);
        updateNegativeFlag(x);
        op.status |= Op::Done;
    }
}

void CPU::txa() {
    if (op.status & Op::Modify) {
        a = x;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.status |= Op::Done;
    }
}

void CPU::txs() {
    if (op.status & Op::Modify) {
        sp = x;
        op.status |= Op::Done;
    }
}

void CPU::tya() {
    if (op.status & Op::Modify) {
        a = y;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.status |= Op::Done;
    }
}

void CPU::xaa() {
    printUnknownOp();
}

void CPU::prepareInterrupt() {
    uint8_t temp = 0;
    switch (op.cycles) {
        case 0:
            op.inst = 0;
            op.opcode = 0;
            break;
        case 2:
            if (op.status & Op::Reset) {
                --sp;
            } else {
                temp = (pc & 0xff00) >> 8;
                mem.push(sp, temp, mute);
            }
            break;
        case 3:
            if (op.status & Op::Reset) {
                --sp;
            } else {
                temp = pc & 0xff;
                mem.push(sp, temp, mute);
            }
            break;
        case 4:
            if (op.status & Op::Reset) {
                op.status &= 0xc0;
                --sp;
            } else if (op.status & Op::NMI) {
                op.status &= 0xa0;
                mem.push(sp, p, mute);
            } else if (op.status & Op::IRQ) {
                op.status &= 0x90;
                mem.push(sp, p, mute);
            }
            break;
        case 5:
            if (op.status & Op::Reset) {
                op.tempAddr = mem.read(0xfffc);
            } else if (op.status & Op::NMI) {
                op.tempAddr = mem.read(0xfffa);
            } else if (op.status & Op::IRQ) {
                op.tempAddr = mem.read(0xfffe);
            }
            p |= 0x4;
            break;
        case 6:
            if (op.status & Op::Reset) {
                op.tempAddr |= mem.read(0xfffd) << 8;
            } else if (op.status & Op::NMI) {
                op.tempAddr |= mem.read(0xfffb) << 8;
            } else if (op.status & Op::IRQ) {
                op.tempAddr |= mem.read(0xffff) << 8;
            }
            pc = op.tempAddr;
            op.status |= Op::Done;
    }
}

// Processor Status Updates

void CPU::updateZeroFlag(uint8_t result) {
    if (result == 0) {
        p |= 2;
    } else {
        p &= 0xfd;
    }
}

void CPU::updateNegativeFlag(uint8_t result) {
    p &= 0x7f;
    p |= result & 0x80;
}

// Print Functions

void CPU::print(bool isCycleDone) {
    unsigned int inc = 0;
    std::string time;
    std::bitset<8> binaryP(p);
    std::bitset<8> binaryStatus(op.status);
    if (isCycleDone) {
        time = "After";
    } else {
        time = "Before";
        ++inc;
        std::cout << "--------------------------------------------------\n";
    }
    std::cout << "Cycle " << totalCycles + inc << ": " << time
        << std::hex << "\n----------------------\n"
        "CPU Fields\n----------------------\n"
        "pc          = 0x" << (unsigned int) pc << "\n"
        "sp          = 0x" << (unsigned int) sp << "\n"
        "a           = 0x" << (unsigned int) a << "\n"
        "x           = 0x" << (unsigned int) x << "\n"
        "y           = 0x" << (unsigned int) y << "\n"
        "p           = " << binaryP << "\n"
        "totalCycles = " << std::dec << (unsigned int) totalCycles <<
        "\n----------------------\n" << std::hex <<
        "Operation Fields\n----------------------\n"
        "inst        = 0x" << (unsigned int) op.inst << "\n"
        "pc          = 0x" << (unsigned int) op.pc << "\n"
        "opcode      = 0x" << (unsigned int) op.opcode << "\n"
        "operandLo   = 0x" << (unsigned int) op.operandLo << "\n"
        "operandHi   = 0x" << (unsigned int) op.operandHi << "\n"
        "val         = 0x" << (unsigned int) op.val << "\n"
        "tempAddr    = 0x" << (unsigned int) op.tempAddr << "\n"
        << std::dec <<
        "addrMode    = " << (unsigned int) op.addrMode << "\n"
        "cycles      = " << (unsigned int) op.cycles << "\n"
        "status      = " << binaryStatus <<
        "\n--------------------------------------------------\n";
}

void CPU::printUnknownOp() {
    if (op.cycles == 2) {
        op.status |= Op::Done;
        std::cout << std::hex << "NOTE: Opcode 0x" << (unsigned int) op.opcode
            << " has not been implemented yet"
            "\n--------------------------------------------------\n" <<
            std::dec;
    }
}