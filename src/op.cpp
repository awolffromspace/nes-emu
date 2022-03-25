#include "op.h"

Op::Op() :
        inst(0),
        pc(0),
        opcode(0),
        operandLo(0),
        operandHi(0),
        val(0),
        tempAddr(0),
        addrMode(0),
        cycles(0),
        status(0) {

}

void Op::clear() {
    inst = 0;
    pc = 0;
    opcode = 0;
    operandLo = 0;
    operandHi = 0;
    val = 0;
    tempAddr = 0;
    addrMode = 0;
    cycles = 0;
    status = 0;
}