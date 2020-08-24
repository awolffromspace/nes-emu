#include "cpu.h"

CPU::CPU()
	: PC(0x8000),
	  SP(0xff),
	  A(0),
	  X(0),
	  Y(0),
	  P(0x20),
	  totalCycles(0) {

}

CPU::CPU(uint8_t data[])
	: PC(0x8000),
	  SP(0xff),
	  A(0),
	  X(0),
	  Y(0),
	  P(0x20),
	  mem(data),
	  totalCycles(0) {

}

void CPU::step() {
	print(false);
	// Fetch
	if ((op.status & Op::Done) || (totalCycles == 0)) {
		op.reset();
		op.PC = PC;
		op.inst = mem.read(PC);
		op.opcode = op.inst;
	}
	// Decode and execute
	(*this.*addrModeArr[op.opcode])();
	(*this.*opcodeArr[op.opcode])();
	++op.cycles;
	++totalCycles;
	print(true);
}

// Addressing Modes
// These functions prepare the operation functions as much as possible for
// execution

void CPU::ABS() {
	switch (op.cycles) {
		case 0:
			op.addrMode = Op::Absolute;
			++PC;
			break;
		case 1:
			op.operandLo = mem.read(PC);
			op.inst = (op.inst << 16) | (op.operandLo << 8);
			op.tempAddr = op.operandLo;
			++PC;
			break;
		case 2:
			op.operandHi = mem.read(PC);
			op.inst |= op.operandHi;
			op.tempAddr |= op.operandHi << 8;
			++PC;
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

void CPU::ABX() {
	uint8_t temp = 0;
	uint16_t fixedAddr = 0;
	switch (op.cycles) {
		case 0:
			op.addrMode = Op::AbsoluteX;
			++PC;
			break;
		case 1:
			op.operandLo = mem.read(PC);
			op.inst = (op.inst << 16) | (op.operandLo << 8);
			temp = op.operandLo + X;
			op.tempAddr = temp;
			++PC;
			break;
		case 2:
			op.operandHi = mem.read(PC);
			op.inst |= op.operandHi;
			op.tempAddr |= op.operandHi << 8;
			++PC;
			break;
		case 3:
			op.val = mem.read(op.tempAddr);
			fixedAddr = X;
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

void CPU::ABY() {
	uint8_t temp = 0;
	uint16_t fixedAddr = 0;
	switch (op.cycles) {
		case 0:
			op.addrMode = Op::AbsoluteY;
			++PC;
			break;
		case 1:
			op.operandLo = mem.read(PC);
			op.inst = (op.inst << 16) | (op.operandLo << 8);
			temp = op.operandLo + Y;
			op.tempAddr = temp;
			++PC;
			break;
		case 2:
			op.operandHi = mem.read(PC);
			op.inst |= op.operandHi;
			op.tempAddr |= op.operandHi << 8;
			++PC;
			break;
		case 3:
			op.val = mem.read(op.tempAddr);
			fixedAddr = Y;
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

void CPU::ACC() {
	switch (op.cycles) {
		case 0:
			op.addrMode = Op::Accumulator;
			++PC;
			break;
		case 1:
			op.val = A;
			op.status |= Op::Modify;
	}
}

void CPU::IMM() {
	switch (op.cycles) {
		case 0:
			op.addrMode = Op::Immediate;
			++PC;
			break;
		case 1:
			op.operandLo = mem.read(PC);
			op.inst = (op.inst << 8) | op.operandLo;
			op.val = op.operandLo;
			op.status |= Op::Modify;
			++PC;
	}
}

void CPU::IMP() {
	switch (op.cycles) {
		case 0:
			op.addrMode = Op::Implied;
			++PC;
			break;
		case 1:
			op.status |= Op::Modify;
			op.status |= Op::Write;
	}
}

void CPU::IDR() {
	uint8_t temp = 0;
	switch (op.cycles) {
		case 0:
			op.addrMode = Op::Indirect;
			++PC;
			break;
		case 1:
			op.operandLo = mem.read(PC);
			op.inst = (op.inst << 16) | (op.operandLo << 8);
			++PC;
			break;
		case 2:
			op.operandHi = mem.read(PC);
			op.inst |= op.operandHi;
			++PC;
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

void CPU::IDX() {
	uint8_t temp = 0;
	switch (op.cycles) {
		case 0:
			op.addrMode = Op::IndirectX;
			++PC;
			break;
		case 1:
			op.operandLo = mem.read(PC);
			op.inst = (op.inst << 8) | op.operandLo;
			++PC;
			break;
		case 3:
			temp = op.operandLo + X;
			op.tempAddr = mem.read(temp);
			break;
		case 4:
			temp = op.operandLo + X + 1;
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

void CPU::IDY() {
	uint8_t temp = 0;
	uint16_t fixedAddr = 0;
	switch (op.cycles) {
		case 0:
			op.addrMode = Op::IndirectY;
			++PC;
			break;
		case 1:
			op.operandLo = mem.read(PC);
			op.inst = (op.inst << 8) | op.operandLo;
			++PC;
			break;
		case 2:
			temp = mem.read(op.operandLo) + Y;
			op.tempAddr = temp;
			break;
		case 3:
			temp = op.operandLo + 1;
			op.tempAddr |= mem.read(temp) << 8;
			break;
		case 4:
			op.val = mem.read(op.tempAddr);
			temp = op.operandLo + 1;
			fixedAddr = Y;
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

void CPU::REL() {
	uint8_t temp = 0;
	uint16_t fixedAddr = 0;
	switch (op.cycles) {
		case 0:
			op.addrMode = Op::Relative;
			++PC;
			break;
		case 1:
			op.operandLo = mem.read(PC);
			op.inst = (op.inst << 8) | op.operandLo;
			++PC;
			break;
		case 2:
			temp = (PC & 0xff) + op.operandLo;
			op.tempAddr = (PC & 0xff00) | temp;
			fixedAddr = op.operandLo;
			if (fixedAddr & 0x80) {
				fixedAddr |= 0xff00;
			}
			fixedAddr += PC;
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

void CPU::ZPG() {
	switch (op.cycles) {
		case 0:
			op.addrMode = Op::ZeroPage;
			++PC;
			break;
		case 1:
			op.operandLo = mem.read(PC);
			op.tempAddr = op.operandLo;
			++PC;
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

void CPU::ZPX() {
	uint8_t temp = 0;
	switch (op.cycles) {
		case 0:
			op.addrMode = Op::ZeroPageX;
			++PC;
			break;
		case 1:
			op.operandLo = mem.read(PC);
			temp = op.operandLo + X;
			op.tempAddr = temp;
			++PC;
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

void CPU::ZPY() {
	uint8_t temp = 0;
	switch (op.cycles) {
		case 0:
			op.addrMode = Op::ZeroPageY;
			++PC;
			break;
		case 1:
			op.operandLo = mem.read(PC);
			temp = op.operandLo + Y;
			op.tempAddr = temp;
			++PC;
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

void CPU::ADC() {
	if (op.status & Op::Modify) {
		uint16_t temp = A + op.val + (P & 1);
		uint8_t pastA = A;
		A += op.val + (P & 1);
		if (temp > 0xff) {
			P |= 1;
		} else {
			P &= 0xfe;
		}
		if ((pastA ^ A) & (op.val ^ A) & 0x80) {
			P |= 0x40;
		} else {
			P &= 0xbf;
		}
		updateZeroFlag(A);
		updateNegativeFlag(A);
		op.status |= Op::Done;
	}
}

void CPU::AHX() {
	printUnknownOp();
}

void CPU::ALR() {
	printUnknownOp();
}

void CPU::AND() {
	if (op.status & Op::Modify) {
		A &= op.val;
		updateZeroFlag(A);
		updateNegativeFlag(A);
		op.status |= Op::Done;
	}
}

void CPU::ANC() {
	printUnknownOp();
}

void CPU::ARR() {
	printUnknownOp();
}

void CPU::ASL() {
	if (op.status & Op::WriteUnmodified) {
		mem.write(op.tempAddr, op.val);
		if (op.val & 0x80) {
			P |= 1;
		} else {
			P &= 0xfe;
		}
		op.val = op.val << 1;
		updateZeroFlag(op.val);
		updateNegativeFlag(op.val);
	} else if (op.status & Op::WriteModified) {
		mem.write(op.tempAddr, op.val);
		op.status |= Op::Done;
	}
}

void CPU::AXS() {
	printUnknownOp();
}

void CPU::BCC() {
	if (op.status & Op::Modify) {
		if (!(P & 1)) {
			PC = op.tempAddr;
		} else {
			op.status |= Op::Done;
		}
	}
}

void CPU::BCS() {
	if (op.status & Op::Modify) {
		if (P & 1) {
			PC = op.tempAddr;
		} else {
			op.status |= Op::Done;
		}
	}
}

void CPU::BEQ() {
	if (op.status & Op::Modify) {
		if (P & 2) {
			PC = op.tempAddr;
		} else {
			op.status |= Op::Done;
		}
	}
}

void CPU::BIT() {
	if (op.status & Op::Modify) {
		uint8_t temp = A & op.val;
		if (op.val & 0x40) {
			P |= 0x40;
		} else {
			P &= 0xbf;
		}
		updateZeroFlag(temp);
		updateNegativeFlag(op.val);
		op.status |= Op::Done;
	}
}

void CPU::BMI() {
	if (op.status & Op::Modify) {
		if (P & 0x80) {
			PC = op.tempAddr;
		} else {
			op.status |= Op::Done;
		}
	}
}

void CPU::BNE() {
	if (op.status & Op::Modify) {
		if (!(P & 2)) {
			PC = op.tempAddr;
		} else {
			op.status |= Op::Done;
		}
	}
}

void CPU::BPL() {
	if (op.status & Op::Modify) {
		if (!(P & 0x80)) {
			PC = op.tempAddr;
		} else {
			op.status |= Op::Done;
		}
	}
}

void CPU::BRK() {
	// Since the emulator is currently designed to run a list of machine 
	// language instructions rather than a ROM file, there needs to be a way to
	// halt the program. BRK is the most suitable instruction for this purpose
	std::cout << "End of the program" << std::endl;
	exit(0);
}

void CPU::BVC() {
	if (op.status & Op::Modify) {
		if (!(P & 0x40)) {
			PC = op.tempAddr;
		} else {
			op.status |= Op::Done;
		}
	}
}

void CPU::BVS() {
	if (op.status & Op::Modify) {
		if (P & 0x40) {
			PC = op.tempAddr;
		} else {
			op.status |= Op::Done;
		}
	}
}

void CPU::CLC() {
	if (op.status & Op::Modify) {
		P &= 0xfe;
		op.status |= Op::Done;
	}
}

void CPU::CLD() {
	if (op.status & Op::Modify) {
		P &= 0xf7;
		op.status |= Op::Done;
	}
}

void CPU::CLI() {
	if (op.status & Op::Modify) {
		P &= 0xfb;
		op.status |= Op::Done;
	}
}

void CPU::CLV() {
	if (op.status & Op::Modify) {
		P &= 0xbf;
		op.status |= Op::Done;
	}
}

void CPU::CMP() {
	if (op.status & Op::Modify) {
		uint8_t temp = A - op.val;
		if (A >= op.val) {
			P |= 1;
		} else {
			P &= 0xfe;
		}
		updateZeroFlag(temp);
		updateNegativeFlag(temp);
		op.status |= Op::Done;
	}
}

void CPU::CPX() {
	if (op.status & Op::Modify) {
		uint8_t temp = X - op.val;
		if (X >= op.val) {
			P |= 1;
		} else {
			P &= 0xfe;
		}
		updateZeroFlag(temp);
		updateNegativeFlag(temp);
		op.status |= Op::Done;
	}
}

void CPU::CPY() {
	if (op.status & Op::Modify) {
		uint8_t temp = Y - op.val;
		if (Y >= op.val) {
			P |= 1;
		} else {
			P &= 0xfe;
		}
		updateZeroFlag(temp);
		updateNegativeFlag(temp);
		op.status |= Op::Done;
	}
}

void CPU::DCP() {
	printUnknownOp();
}

void CPU::DEC() {
	if (op.status & Op::WriteUnmodified) {
		mem.write(op.tempAddr, op.val);
		--op.val;
		updateZeroFlag(op.val);
		updateNegativeFlag(op.val);
	} else if (op.status & Op::WriteModified) {
		mem.write(op.tempAddr, op.val);
		op.status |= Op::Done;
	}
}

void CPU::DEX() {
	if (op.status & Op::Modify) {
		--X;
		updateZeroFlag(X);
		updateNegativeFlag(X);
		op.status |= Op::Done;
	}
}

void CPU::DEY() {
	if (op.status & Op::Modify) {
		--Y;
		updateZeroFlag(Y);
		updateNegativeFlag(Y);
		op.status |= Op::Done;
	}
}

void CPU::EOR() {
	if (op.status & Op::Modify) {
		A ^= op.val;
		updateZeroFlag(A);
		updateNegativeFlag(A);
		op.status |= Op::Done;
	}
}

void CPU::INC() {
	if (op.status & Op::WriteUnmodified) {
		mem.write(op.tempAddr, op.val);
		++op.val;
		updateZeroFlag(op.val);
		updateNegativeFlag(op.val);
	} else if (op.status & Op::WriteModified) {
		mem.write(op.tempAddr, op.val);
		op.status |= Op::Done;
	}
}

void CPU::INX() {
	if (op.status & Op::Modify) {
		++X;
		updateZeroFlag(X);
		updateNegativeFlag(X);
		op.status |= Op::Done;
	}
}

void CPU::INY() {
	if (op.status & Op::Modify) {
		++Y;
		updateZeroFlag(Y);
		updateNegativeFlag(Y);
		op.status |= Op::Done;
	}
}

void CPU::ISC() {
	printUnknownOp();
}

void CPU::JMP() {
	if (((op.addrMode == Op::Absolute) && (op.cycles == 2)) ||
		((op.addrMode == Op::Indirect) && (op.cycles == 4))) {
		PC = op.tempAddr;
		op.status |= Op::Done;
	}
}

void CPU::JSR() {
	if (op.cycles == 2) {
		--PC;
	} else if (op.cycles == 3) {
		uint8_t PCH = (PC & 0xff00) >> 8;
		mem.push(SP, PCH);
	} else if (op.cycles == 4) {
		uint8_t PCL = PC & 0xff;
		mem.push(SP, PCL);
	} else if (op.cycles == 5) {
		PC = op.tempAddr;
		op.status |= Op::Done;
	}
}

void CPU::LAS() {
	printUnknownOp();
}

void CPU::LAX() {
	printUnknownOp();
}

void CPU::LDA() {
	if (op.status & Op::Modify) {
		A = op.val;
		updateZeroFlag(A);
		updateNegativeFlag(A);
		op.status |= Op::Done;
	}
}

void CPU::LDX() {
	if (op.status & Op::Modify) {
		X = op.val;
		updateZeroFlag(X);
		updateNegativeFlag(X);
		op.status |= Op::Done;
	}
}

void CPU::LDY() {
	if (op.status & Op::Modify) {
		Y = op.val;
		updateZeroFlag(Y);
		updateNegativeFlag(Y);
		op.status |= Op::Done;
	}
}

void CPU::LSR() {
	if (op.status & Op::WriteUnmodified) {
		mem.write(op.tempAddr, op.val);
		if (op.val & 1) {
			P |= 1;
		} else {
			P &= 0xfe;
		}
		op.val = op.val >> 1;
		updateZeroFlag(op.val);
		updateNegativeFlag(op.val);
	} else if (op.status & Op::WriteModified) {
		mem.write(op.tempAddr, op.val);
		op.status |= Op::Done;
	}
}

void CPU::NOP() {
	if (op.status & Op::Modify) {
		op.status |= Op::Done;
	}
}

void CPU::ORA() {
	if (op.status & Op::Modify) {
		A |= op.val;
		updateZeroFlag(A);
		updateNegativeFlag(A);
		op.status |= Op::Done;
	}
}

void CPU::PHA() {
	if (op.cycles == 2) {
		mem.push(SP, A);
		op.status |= Op::Done;
	}
}

void CPU::PHP() {
	if (op.cycles == 2) {
		mem.push(SP, P);
		op.status |= Op::Done;
	}
}

void CPU::PLA() {
	if (op.cycles == 2) {
		op.val = mem.pull(SP);
	} else if (op.cycles == 3) {
		A = op.val;
		op.status |= Op::Done;
	}
}

void CPU::PLP() {
	if (op.cycles == 2) {
		op.val = mem.pull(SP);
	} else if (op.cycles == 3) {
		P = op.val;
		op.status |= Op::Done;
	}
}

void CPU::RLA() {
	printUnknownOp();
}

void CPU::ROL() {
	if (op.status & Op::WriteUnmodified) {
		mem.write(op.tempAddr, op.val);
		uint8_t temp = op.val << 1;
		if (P & 1) {
			temp |= 1;
		}
		if (op.val & 0x80) {
			P |= 1;
		} else {
			P &= 0xfe;
		}
		op.val = temp;
		updateZeroFlag(op.val);
		updateNegativeFlag(op.val);
	} else if (op.status & Op::WriteModified) {
		mem.write(op.tempAddr, op.val);
		op.status |= Op::Done;
	}
}

void CPU::ROR() {
	if (op.status & Op::WriteUnmodified) {
		mem.write(op.tempAddr, op.val);
		uint8_t temp = op.val >> 1;
		if (P & 1) {
			temp |= 0x80;
		}
		if (op.val & 1) {
			P |= 1;
		} else {
			P &= 0xfe;
		}
		op.val = temp;
		updateZeroFlag(op.val);
		updateNegativeFlag(op.val);
	} else if (op.status & Op::WriteModified) {
		mem.write(op.tempAddr, op.val);
		op.status |= Op::Done;
	}
}

void CPU::RRA() {
	printUnknownOp();
}

void CPU::RTI() {
	printUnknownOp();
}

void CPU::RTS() {
	if (op.cycles == 3) {
		op.tempAddr = mem.pull(SP);
	} else if (op.cycles == 4) {
		op.tempAddr |= mem.pull(SP) << 8;
	} else if (op.cycles == 5) {
		PC = op.tempAddr + 1;
		op.status |= Op::Done;
	}
}

void CPU::SAX() {
	printUnknownOp();
}

void CPU::SBC() {
	if (op.status & Op::Modify) {
		op.val = 0xff - op.val;
		ADC();
	}
}

void CPU::SEC() {
	if (op.status & Op::Modify) {
		P |= 1;
		op.status |= Op::Done;
	}
}

void CPU::SED() {
	if (op.status & Op::Modify) {
		P |= 8;
		op.status |= Op::Done;
	}
}

void CPU::SEI() {
	if (op.status & Op::Modify) {
		P |= 4;
		op.status |= Op::Done;
	}
}

void CPU::SHX() {
	printUnknownOp();
}

void CPU::SHY() {
	printUnknownOp();
}

void CPU::SLO() {
	printUnknownOp();
}

void CPU::SRE() {
	printUnknownOp();
}

void CPU::STA() {
	if (op.status & Op::Write) {
		mem.write(op.tempAddr, A);
		op.status |= Op::Done;
	}
}

void CPU::STP() {
	printUnknownOp();
}

void CPU::STX() {
	if (op.status & Op::Write) {
		mem.write(op.tempAddr, X);
		op.status |= Op::Done;
	}
}

void CPU::STY() {
	if (op.status & Op::Write) {
		mem.write(op.tempAddr, Y);
		op.status |= Op::Done;
	}
}

void CPU::TAS() {
	printUnknownOp();
}

void CPU::TAX() {
	if (op.status & Op::Modify) {
		X = A;
		updateZeroFlag(X);
		updateNegativeFlag(X);
		op.status |= Op::Done;
	}
}

void CPU::TAY() {
	if (op.status & Op::Modify) {
		Y = A;
		updateZeroFlag(Y);
		updateNegativeFlag(Y);
		op.status |= Op::Done;
	}
}

void CPU::TSX() {
	if (op.status & Op::Modify) {
		X = SP;
		updateZeroFlag(X);
		updateNegativeFlag(X);
		op.status |= Op::Done;
	}
}

void CPU::TXA() {
	if (op.status & Op::Modify) {
		A = X;
		updateZeroFlag(A);
		updateNegativeFlag(A);
		op.status |= Op::Done;
	}
}

void CPU::TXS() {
	if (op.status & Op::Modify) {
		SP = X;
		op.status |= Op::Done;
	}
}

void CPU::TYA() {
	if (op.status & Op::Modify) {
		A = Y;
		updateZeroFlag(A);
		updateNegativeFlag(A);
		op.status |= Op::Done;
	}
}

void CPU::XAA() {
	printUnknownOp();
}

// Processor Status Updates

void CPU::updateZeroFlag(uint8_t result) {
	if (result == 0) {
		P |= 2;
	} else {
		P &= 0xfd;
	}
}

void CPU::updateNegativeFlag(uint8_t result) {
	P |= result & 0x80;
}

// Print Functions

void CPU::print(bool isCycleDone) {
	unsigned int inc = 0;
	std::string time;
	std::bitset<8> binaryP(P);
	std::bitset<8> binaryStatus(op.status);
	if (isCycleDone) {
		time = "After";
	} else {
		time = "Before";
		++inc;
		std::cout << "-------------------------\n";
	}
	std::cout << "Cycle " << totalCycles + inc << ": " << time
		<< std::hex << "\n----------------\n"
		"CPU Fields\n----------------\n"
		"PC          = 0x" << (unsigned int) PC << "\n"
		"SP          = 0x" << (unsigned int) SP << "\n"
		"A           = 0x" << (unsigned int) A << "\n"
		"X           = 0x" << (unsigned int) X << "\n"
		"Y           = 0x" << (unsigned int) Y << "\n"
		"P           = " << binaryP << "\n"
		"totalCycles = " << std::dec << (unsigned int) totalCycles <<
		"\n----------------\n" << std::hex <<
		"Operation Fields\n----------------\n"
		"inst        = 0x" << (unsigned int) op.inst << "\n"
		"PC          = 0x" << (unsigned int) op.PC << "\n"
		"opcode      = 0x" << (unsigned int) op.opcode << "\n"
		"operandLo   = 0x" << (unsigned int) op.operandLo << "\n"
		"operandHi   = 0x" << (unsigned int) op.operandHi << "\n"
		"val         = 0x" << (unsigned int) op.val << "\n"
		"tempAddr    = 0x" << (unsigned int) op.tempAddr << "\n"
		<< std::dec <<
		"addrMode    = " << (unsigned int) op.addrMode << "\n"
		"cycles      = " << (unsigned int) op.cycles << "\n"
		"status      = " << binaryStatus <<
		"\n-------------------------\n";
}

void CPU::printUnknownOp() {
	if (op.cycles == 2) {
		op.status |= Op::Done;
		std::cout << std::hex << "NOTE: Opcode 0x" << (unsigned int) op.opcode
			<< " has not been implemented yet\n-------------------------\n" <<
			std::dec;
	}
}