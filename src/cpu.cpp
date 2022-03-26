#include "cpu.h"

CPU::CPU() :
        sp(0xff),
        a(0),
        x(0),
        y(0),
        p(0x30),
        totalCycles(0),
        endOfProgram(false),
        haltAtBrk(false),
        mute(true) {
    // Initialize PC to the reset vector
    pc = (read(0xfffd) << 8) | read(0xfffc);
}

void CPU::clear() {
    sp = 0xff;
    a = 0;
    x = 0;
    y = 0;
    p = 0x30;
    op.clear();
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

void CPU::step() {
    if ((op.status & Op::Done) || (totalCycles == 0)) {
        op.clear();

        // Polling for interrupts
        // If any interrupts are flagged, start the interrupt prologue
        // Ignore IRQ if the Interrupt Disable flag is set
        // This technically should be performed on the second-to-last cycle, but
        // doing it here right after the instruction ended seems to accomplish
        // the same result without having to hard code when each instruction's
        // second-to-last cycle is
        // TODO: When other devices (e.g., PPU) are added to the emulator, need
        // to add interrupt latency
        if (((op.status & Op::IRQ) && !(p & InterruptDisable)) ||
                (op.status & Op::NMI) || (op.status & Op::Reset)) {
            op.status |= Op::InterruptPrologue;
        }

        // Fetch
        op.pc = pc;
        op.inst = read(pc);
        op.opcode = op.inst;
    }

    if ((op.status & Op::InterruptPrologue) && (op.status & Op::Reset)) {
        prepareReset();
    } else if ((op.status & Op::InterruptPrologue) && (op.status & Op::NMI)) {
        prepareNMI();
    } else if ((op.status & Op::InterruptPrologue) && (op.status & Op::IRQ)) {
        prepareIRQ(false);
    } else {
        // Decode and execute
        (this->*addrModeArr[op.opcode])();
        (this->*opcodeArr[op.opcode])();
    }

    ++op.cycles;
    ++totalCycles;
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
            break;
        case 3:
            op.val = read(op.tempAddr);
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
            ++pc;
            break;
        case 3:
            op.val = read(op.tempAddr);
            fixedAddr = x;
            fixedAddr += (op.operandHi << 8) + op.operandLo;
            if (op.tempAddr == fixedAddr) {
                op.status |= Op::Modify;
            } else {
                op.tempAddr = fixedAddr;
            }
            break;
        case 4:
            op.val = read(op.tempAddr);
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
            ++pc;
            break;
        case 3:
            op.val = read(op.tempAddr);
            fixedAddr = y;
            fixedAddr += (op.operandHi << 8) + op.operandLo;
            if (op.tempAddr == fixedAddr) {
                op.status |= Op::Modify;
            } else {
                op.tempAddr = fixedAddr;
            }
            break;
        case 4:
            op.val = read(op.tempAddr);
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
            op.operandLo = read(pc);
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
            break;
        case 4:
            temp = op.operandLo + 1;
            op.tempAddr |= read((op.operandHi << 8) | temp) << 8;
            op.val = read(op.tempAddr);
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
            break;
        case 5:
            op.val = read(op.tempAddr);
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
            break;
        case 4:
            op.val = read(op.tempAddr);
            temp = op.operandLo + 1;
            fixedAddr = y;
            fixedAddr += (read(temp) << 8) + read(op.operandLo);
            if (op.tempAddr == fixedAddr) {
                op.status |= Op::Modify;
            } else {
                op.tempAddr = fixedAddr;
            }
            break;
        case 5:
            op.val = read(op.tempAddr);
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
            op.operandLo = read(pc);
            op.inst = (op.inst << 8) | op.operandLo;
            ++pc;
            break;
        case 2:
            temp = (pc & 0xff) + op.operandLo;
            op.tempAddr = (pc & 0xff00) | temp;
            fixedAddr = op.operandLo;
            if (fixedAddr & Negative) {
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
            op.operandLo = read(pc);
            op.inst = (op.inst << 8) | op.operandLo;
            op.tempAddr = op.operandLo;
            ++pc;
            break;
        case 2:
            op.val = read(op.tempAddr);
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
            op.operandLo = read(pc);
            op.inst = (op.inst << 8) | op.operandLo;
            temp = op.operandLo + x;
            op.tempAddr = temp;
            ++pc;
            break;
        case 3:
            op.val = read(op.tempAddr);
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
            op.operandLo = read(pc);
            op.inst = (op.inst << 8) | op.operandLo;
            temp = op.operandLo + y;
            op.tempAddr = temp;
            ++pc;
            break;
        case 3:
            op.val = read(op.tempAddr);
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

// Operations
// These functions perform any opcode-specific execution that can't be easily
// done in the addressing mode functions

void CPU::adc() {
    if (op.status & Op::Modify) {
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
    if ((op.status & Op::Modify) && (op.addrMode == Op::Accumulator)) {
        if (a & Negative) {
            p |= Carry;
        } else {
            p &= ~Carry;
        }
        a = a << 1;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.status |= Op::Done;
    } else if (op.status & Op::WriteModified) {
        write(op.tempAddr, op.val, mute);
        op.status |= Op::Done;
    } else if (op.status & Op::WriteUnmodified) {
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
    printUnknownOp();
}

void CPU::bcc() {
    if ((op.cycles == 1) && (p & Carry)) {
        op.status |= Op::Done;
    } else if ((op.status & Op::Modify) && !(p & Carry)) {
        pc = op.tempAddr;
    }
}

void CPU::bcs() {
    if ((op.cycles == 1) && !(p & Carry)) {
        op.status |= Op::Done;
    } else if ((op.status & Op::Modify) && (p & Carry)) {
        pc = op.tempAddr;
    }
}

void CPU::beq() {
    if ((op.cycles == 1) && !(p & Zero)) {
        op.status |= Op::Done;
    } else if ((op.status & Op::Modify) && (p & Zero)) {
        pc = op.tempAddr;
    }
}

void CPU::bit() {
    if (op.status & Op::Modify) {
        uint8_t temp = a & op.val;
        if (op.val & Overflow) {
            p |= Overflow;
        } else {
            p &= ~Overflow;
        }
        updateZeroFlag(temp);
        updateNegativeFlag(op.val);
        op.status |= Op::Done;
    }
}

void CPU::bmi() {
    if ((op.cycles == 1) && !(p & Negative)) {
        op.status |= Op::Done;
    } else if ((op.status & Op::Modify) && (p & Negative)) {
        pc = op.tempAddr;
    }
}

void CPU::bne() {
    if ((op.cycles == 1) && (p & Zero)) {
        op.status |= Op::Done;
    } else if ((op.status & Op::Modify) && !(p & Zero)) {
        pc = op.tempAddr;
    }
}

void CPU::bpl() {
    if ((op.cycles == 1) && (p & Negative)) {
        op.status |= Op::Done;
    } else if ((op.status & Op::Modify) && !(p & Negative)) {
        pc = op.tempAddr;
    }
}

void CPU::brk() {
    if (haltAtBrk) {
        endOfProgram = true;
    } else {
        op.status |= Op::IRQ;
        op.status |= Op::InterruptPrologue;
        prepareIRQ(true);
    }
}

void CPU::bvc() {
    if ((op.cycles == 1) && (p & Overflow)) {
        op.status |= Op::Done;
    } else if ((op.status & Op::Modify) && !(p & Overflow)) {
        pc = op.tempAddr;
    }
}

void CPU::bvs() {
    if ((op.cycles == 1) && !(p & Overflow)) {
        op.status |= Op::Done;
    } else if ((op.status & Op::Modify) && (p & Overflow)) {
        pc = op.tempAddr;
    }
}

void CPU::clc() {
    if (op.status & Op::Modify) {
        p &= ~Carry;
        op.status |= Op::Done;
    }
}

void CPU::cld() {
    if (op.status & Op::Modify) {
        p &= ~DecimalMode;
        op.status |= Op::Done;
    }
}

void CPU::cli() {
    if (op.status & Op::Modify) {
        p &= ~InterruptDisable;
        op.status |= Op::Done;
    }
}

void CPU::clv() {
    if (op.status & Op::Modify) {
        p &= ~Overflow;
        op.status |= Op::Done;
    }
}

void CPU::cmp() {
    if (op.status & Op::Modify) {
        uint8_t temp = a - op.val;
        if (a >= op.val) {
            p |= Carry;
        } else {
            p &= ~Carry;
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
            p |= Carry;
        } else {
            p &= ~Carry;
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
            p |= Carry;
        } else {
            p &= ~Carry;
        }
        updateZeroFlag(temp);
        updateNegativeFlag(temp);
        op.status |= Op::Done;
    }
}

void CPU::dcp() {
    if (op.status & Op::WriteModified) {
        dec();
        cmp();
    } else if (op.status & Op::WriteUnmodified) {
        dec();
    }
}

void CPU::dec() {
    if (op.status & Op::WriteModified) {
        write(op.tempAddr, op.val, mute);
        op.status |= Op::Done;
    } else if (op.status & Op::WriteUnmodified) {
        write(op.tempAddr, op.val, mute);
        --op.val;
        updateZeroFlag(op.val);
        updateNegativeFlag(op.val);
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
    if (op.status & Op::WriteModified) {
        write(op.tempAddr, op.val, mute);
        op.status |= Op::Done;
    } else if (op.status & Op::WriteUnmodified) {
        write(op.tempAddr, op.val, mute);
        ++op.val;
        updateZeroFlag(op.val);
        updateNegativeFlag(op.val);
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
    if (op.status & Op::WriteModified) {
        inc();
        sbc();
    } else if (op.status & Op::WriteUnmodified) {
        inc();
    }
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
            ram.push(sp, temp, mute);
            break;
        case 4:
            temp = pc & 0xff;
            ram.push(sp, temp, mute);
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
    if (op.status & Op::Modify) {
        a = op.val;
        x = op.val;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.status |= Op::Done;
    }
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
    if ((op.status & Op::Modify) && (op.addrMode == Op::Accumulator)) {
        if (a & Carry) {
            p |= Carry;
        } else {
            p &= ~Carry;
        }
        a = a >> 1;
        updateZeroFlag(a);
        updateNegativeFlag(a);
        op.status |= Op::Done;
    } else if (op.status & Op::WriteModified) {
        write(op.tempAddr, op.val, mute);
        op.status |= Op::Done;
    } else if (op.status & Op::WriteUnmodified) {
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
        ram.push(sp, a, mute);
        op.status |= Op::Done;
    }
}

void CPU::php() {
    if (op.cycles == 2) {
        uint8_t temp = p | Break | UnusedFlag;
        ram.push(sp, temp, mute);
        op.status |= Op::Done;
    }
}

void CPU::pla() {
    switch (op.cycles) {
        case 2:
            op.val = ram.pull(sp, mute);
            break;
        case 3:
            a = op.val;
            updateZeroFlag(a);
            updateNegativeFlag(a);
            op.status |= Op::Done;
    }
}

void CPU::plp() {
    switch (op.cycles) {
        case 2:
            op.val = ram.pull(sp, mute);
            break;
        case 3:
            p = op.val | Break | UnusedFlag;
            op.status |= Op::Done;
    }
}

void CPU::rla() {
    if (op.status & Op::WriteModified) {
        rol();
        andOp();
    } else if (op.status & Op::WriteUnmodified) {
        rol();
    }
}

void CPU::rol() {
    if ((op.status & Op::Modify) && (op.addrMode == Op::Accumulator)) {
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
        op.status |= Op::Done;
    } else if (op.status & Op::WriteModified) {
        write(op.tempAddr, op.val, mute);
        op.status |= Op::Done;
    } else if (op.status & Op::WriteUnmodified) {
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
    if ((op.status & Op::Modify) && (op.addrMode == Op::Accumulator)) {
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
        op.status |= Op::Done;
    } else if (op.status & Op::WriteModified) {
        write(op.tempAddr, op.val, mute);
        op.status |= Op::Done;
    } else if (op.status & Op::WriteUnmodified) {
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
    if (op.status & Op::WriteModified) {
        ror();
        adc();
    } else if (op.status & Op::WriteUnmodified) {
        ror();
    }
}

void CPU::rti() {
    switch (op.cycles) {
        case 3:
            p = ram.pull(sp, mute) | Break | UnusedFlag;
            break;
        case 4:
            op.tempAddr |= ram.pull(sp, mute);
            break;
        case 5:
            op.tempAddr |= ram.pull(sp, mute) << 8;
            pc = op.tempAddr;
            op.status |= Op::Done;
    }
}

void CPU::rts() {
    switch (op.cycles) {
        case 3:
            op.tempAddr = ram.pull(sp, mute);
            break;
        case 4:
            op.tempAddr |= ram.pull(sp, mute) << 8;
            break;
        case 5:
            pc = op.tempAddr + 1;
            op.status |= Op::Done;
    }
}

void CPU::sax() {
    if (op.status & Op::Write) {
        write(op.tempAddr, a & x, mute);
        op.status |= Op::Done;
    }
}

void CPU::sbc() {
    if (op.status & Op::Modify) {
        op.val = 0xff - op.val;
        adc();
    }
}

void CPU::sec() {
    if (op.status & Op::Modify) {
        p |= Carry;
        op.status |= Op::Done;
    }
}

void CPU::sed() {
    if (op.status & Op::Modify) {
        p |= DecimalMode;
        op.status |= Op::Done;
    }
}

void CPU::sei() {
    if (op.status & Op::Modify) {
        p |= InterruptDisable;
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
    if (op.status & Op::WriteModified) {
        asl();
        ora();
    } else if (op.status & Op::WriteUnmodified) {
        asl();
    }
}

void CPU::sre() {
    if (op.status & Op::WriteModified) {
        lsr();
        eor();
    } else if (op.status & Op::WriteUnmodified) {
        lsr();
    }
}

void CPU::sta() {
    if (op.status & Op::Write) {
        write(op.tempAddr, a, mute);
        op.status |= Op::Done;
    }
}

void CPU::stp() {
    printUnknownOp();
}

void CPU::stx() {
    if (op.status & Op::Write) {
        write(op.tempAddr, x, mute);
        op.status |= Op::Done;
    }
}

void CPU::sty() {
    if (op.status & Op::Write) {
        write(op.tempAddr, y, mute);
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

// Read/Write Functions

uint8_t CPU::read(uint16_t addr) {
    if (addr < 0x2000) {
        return ram.read(addr);
    } else if (addr < 0x4000 || addr == 0x4014) {
        return ppu.readRegister(addr);
    } else if (addr < 0x4016) {
        return apu.readRegister(addr);
    } else if (addr == 0x4017) {
        return apu.readRegister(addr) | io.readRegister(addr);
    } else if (addr < 0x4020) {
        return io.readRegister(addr);
    } else {
        return mmc.read(addr);
    }
}

void CPU::write(uint16_t addr, uint8_t val, bool mute) {
    if (addr < 0x2000) {
        ram.write(addr, val, mute);
    } else if (addr < 0x4000 || addr == 0x4014) {
        ppu.writeRegister(addr, val, mute);
    } else if (addr < 0x4016) {
        apu.writeRegister(addr, val, mute);
    } else if (addr == 0x4017) {
        apu.writeRegister(addr, val, mute);
        io.writeRegister(addr, val, mute);
    } else if (addr < 0x4020) {
        io.writeRegister(addr, val, mute);
    } else {
        mmc.write(addr, val, mute);
    }
}

// Interrupt Prologue Functions

void CPU::prepareIRQ(bool isBrk) {
    uint8_t temp = 0;
    switch (op.cycles) {
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
            op.status |= Op::Done;
    }
}

void CPU::prepareNMI() {
    uint8_t temp = 0;
    switch (op.cycles) {
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
            op.status |= Op::Done;
    }
}

void CPU::prepareReset() {
    switch (op.cycles) {
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
            op.status |= Op::Done;
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

// Miscellaneous Functions

void CPU::readInInst(std::string& filename) {
    mmc.readInInst(filename);
}

void CPU::readInINES(std::string& filename) {
    mmc.readInINES(filename);
    pc = 0xc000;
    p |= 0x4;
    sp = 0xfd;
}

bool CPU::compareState(struct CPUState& state) {
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
            operandHi = read(pc + 0x1);
            inst |= operandHi;
            break;
        case 2:
            operandLo = read(pc);
            inst = (inst << 16) | (operandLo << 8);
            operandHi = read(pc + 0x1);
            inst |= operandHi;
            break;
        case 3:
            operandLo = read(pc);
            inst = (inst << 16) | (operandLo << 8);
            operandHi = read(pc + 0x1);
            inst |= operandHi;
            break;
        case 5:
            operandLo = read(pc);
            inst = (inst << 8) | operandLo;
            break;
        case 7:
            operandLo = read(pc);
            inst = (inst << 16) | (operandLo << 8);
            operandHi = read(pc + 0x1);
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

unsigned int CPU::getTotalCycles() {
    return totalCycles;
}

bool CPU::isEndOfProgram() {
    return endOfProgram;
}

bool CPU::isHaltAtBrk() {
    return haltAtBrk;
}

void CPU::setHaltAtBrk(bool h) {
    haltAtBrk = h;
}

void CPU::setMute(bool m) {
    mute = m;
}

unsigned int CPU::getOpCycles() {
    return op.cycles;
}

// Print Functions

void CPU::print(bool isCycleDone) {
    if (mute) {
        return;
    }

    unsigned int inc = 0;
    std::string time;
    std::bitset<8> binaryP(p);
    std::bitset<9> binaryStatus(op.status);
    if (isCycleDone) {
        time = "After";
    } else {
        time = "Before";
        ++inc;
        std::cout << "--------------------------------------------------\n";
    }
    std::cout << "Cycle " << totalCycles + inc << ": " << time
        << std::hex << "\n-----------------------\n"
        "CPU Fields\n-----------------------\n"
        "pc          = 0x" << (unsigned int) pc << "\n"
        "sp          = 0x" << (unsigned int) sp << "\n"
        "a           = 0x" << (unsigned int) a << "\n"
        "x           = 0x" << (unsigned int) x << "\n"
        "y           = 0x" << (unsigned int) y << "\n"
        "p           = " << binaryP << "\n"
        "totalCycles = " << std::dec << (unsigned int) totalCycles <<
        "\n-----------------------\n" << std::hex <<
        "Operation Fields\n-----------------------\n"
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
        if (!mute) {
            std::cout << std::hex << "NOTE: Opcode 0x" <<
                (unsigned int) op.opcode <<
                " has not been implemented yet"
                "\n--------------------------------------------------\n" <<
                std::dec;
        }
    }
}

void CPU::printStateInst(uint32_t inst) {
    std::cout << std::hex << (unsigned int) pc << "  " << (unsigned int) inst <<
        "  A:" << (unsigned int) a << " X:" << (unsigned int) x << " Y:" <<
        (unsigned int) y << " P:" << (unsigned int) p << " SP:" <<
        (unsigned int) sp << " CYC:" << std::dec << totalCycles << "\n";
}