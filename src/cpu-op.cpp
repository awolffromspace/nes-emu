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
        status(0) {

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
    if (clearInterrupts) {
        status = 0;
    } else {
        status &= ~(CPUOp::Modify | CPUOp::Write | CPUOp::WriteUnmodified |
            CPUOp::WriteModified | CPUOp::OAMDMA | CPUOp::Done);
    }
}

void CPUOp::clearInterruptFlags() {
    status &= ~(CPUOp::IRQ | CPUOp::NMI | CPUOp::Reset);
}

void CPUOp::clearDMA() {
    dmaCycle = 0;
    status &= ~CPUOp::OAMDMA;
}