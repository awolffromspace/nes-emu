#include "cpu-op.h"

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
        done(false) {

}

void CPUOp::clear(bool clearInterrupts) {
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
    oamDMATransfer = false;
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