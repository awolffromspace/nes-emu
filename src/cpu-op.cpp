#include "cpu-op.h"

// Public Member Functions

CPUOp::CPUOp() :
        inst(0),
        pc(0),
        opcode(0),
        operandLo(0),
        operandHi(0),
        val(0),
        tempAddr(0),
        fixedAddr(0),
        addrMode(0),
        instType(0),
        cycle(0),
        dmaCycle(0),
        modify(false),
        write(false),
        writeUnmodified(false),
        writeModified(false),
        irq(false),
        nmi(false),
        reset(false),
        interruptPrologue(false),
        oamDMATransfer(false),
        done(false) { }

void CPUOp::clear(bool clearInterrupts, bool clearDMA) {
    inst = 0;
    pc = 0;
    opcode = 0;
    operandLo = 0;
    operandHi = 0;
    val = 0;
    tempAddr = 0;
    fixedAddr = 0;
    addrMode = 0;
    instType = 0;
    cycle = 0;
    dmaCycle = 0;
    modify = false;
    write = false;
    writeUnmodified = false;
    writeModified = false;
    if (clearInterrupts) {
        irq = false;
        nmi = false;
        reset = false;
    }
    interruptPrologue = false;
    if (clearDMA) {
        oamDMATransfer = false;
    }
    done = false;
}

void CPUOp::clearStatusFlags(bool clearIRQ, bool clearNMI, bool clearReset,
        bool clearInterruptPrologue) {
    modify = false;
    write = false;
    writeUnmodified = false;
    writeModified = false;
    if (clearIRQ) {
        irq = false;
    }
    if (clearNMI) {
        nmi = false;
    }
    if (clearReset) {
        reset = false;
    }
    if (clearInterruptPrologue) {
        interruptPrologue = false;
    }
    oamDMATransfer = false;
    done = false;
}

void CPUOp::clearInterruptFlags() {
    irq = false;
    nmi = false;
    reset = false;
}

void CPUOp::clearDMA() {
    dmaCycle = 0;
    oamDMATransfer = false;
}

// When the CPU stores addresses into tempAddr and fixedAddr, it is done where tempAddr is formed as
// two 1-byte values with wraparound on both, and fixedAddr is formed as one 2-byte value with
// wraparound on only the entire 2-byte value. The high byte of the address represents the page, so
// crossing a page boundary means that an instruction, such as a jump or branch, has a target
// address with a different high byte or page

bool CPUOp::crossedPageBoundary() const {
    return tempAddr != fixedAddr;
}