#include "cpu.h"

CPU::CPU()
	: PC(0),
	  SP(0xff),
	  A(0),
	  X(0),
	  Y(0),
	  P(32),
	  totalCycles(0),
	  totalInst(0) {

}

CPU::CPU(uint8_t data[])
	: PC(0x8000),
	  SP(0xff),
	  A(0),
	  X(0),
	  Y(0),
	  P(32),
	  mem(data),
	  totalCycles(0),
	  totalInst(0) {

}

// Executes exactly one CPU cycle

void CPU::step() {
	while (true) {
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
		if (op.status & Op::Done) {
			++totalInst;
		}
		// If the operation is done and didn't write to memory in its last
		// cycle, the next op can be fetched in the same cycle
		if ((op.status & Op::Done) && !(op.status & Op::WroteToMem)) {
			continue;
		}
		++totalCycles;
		break;
	}
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
			fixedAddr = (op.operandHi << 8) + op.operandLo + X;
			if (op.tempAddr == fixedAddr) {
				op.status |= Op::Modify;
			} else {
				op.tempAddr = fixedAddr;
				op.status |= Op::PgBoundCrossed;
			}
			break;
		case 4:
			op.status |= Op::Modify;
			op.status |= Op::Write;
			op.status |= Op::Reread;
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
			fixedAddr = (op.operandHi << 8) + op.operandLo + Y;
			if (op.tempAddr == fixedAddr) {
				op.status |= Op::Modify;
			} else {
				op.tempAddr = fixedAddr;
				op.status |= Op::PgBoundCrossed;
			}
			break;
		case 4:
			op.status |= Op::Modify;
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
			op.status |= Op::Modify;
			op.status |= Op::Write;
			op.status |= Op::Reread;
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
			fixedAddr = (mem.read(temp) << 8) + mem.read(op.operandLo) + Y;
			if (op.tempAddr == fixedAddr) {
				op.status |= Op::Modify;
			} else {
				op.tempAddr = fixedAddr;
				op.status |= Op::PgBoundCrossed;
			}
			break;
		case 5:
			op.status |= Op::Modify;
			op.status |= Op::Write;
			op.status |= Op::Reread;
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
			op.inst = (op.inst << 8) + op.operandLo;
			++PC;
			break;
		case 2:
			temp = (PC & 0xff) + op.operandLo;
			op.tempAddr = (PC & 0xff00) | temp;
			PC = op.tempAddr;
			fixedAddr = PC + op.operandLo;
			if (op.tempAddr == fixedAddr) {
				op.status |= Op::Done;
			} else {
				op.tempAddr = fixedAddr;
				op.status |= Op::PgBoundCrossed;
			}
			break;
		case 3:
			PC = op.tempAddr;
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

}

void CPU::ALR() {

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

}

void CPU::ARR() {

}

void CPU::ASL() {
	if (op.status & Op::Reread) {
		mem.read(op.tempAddr);
	} else if (op.status & Op::WriteUnmodified) {
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

}

void CPU::BCC() {

}

void CPU::BCS() {

}

void CPU::BEQ() {

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

}

void CPU::BNE() {

}

void CPU::BPL() {

}

void CPU::BRK() {

}

void CPU::BVC() {

}

void CPU::BVS() {

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

}

void CPU::DEC() {
	if (op.status & Op::Reread) {
		mem.read(op.tempAddr);
	} else if (op.status & Op::WriteUnmodified) {
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
	if (op.status & Op::Reread) {
		mem.read(op.tempAddr);
	} else if (op.status & Op::WriteUnmodified) {
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

}

void CPU::LAX() {

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
	if (op.status & Op::Reread) {
		mem.read(op.tempAddr);
	} else if (op.status & Op::WriteUnmodified) {
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

}

void CPU::ROL() {
	if (op.status & Op::Reread) {
		mem.read(op.tempAddr);
	} else if (op.status & Op::WriteUnmodified) {
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
	if (op.status & Op::Reread) {
		mem.read(op.tempAddr);
	} else if (op.status & Op::WriteUnmodified) {
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

}

void CPU::RTI() {

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

}

void CPU::SHY() {

}

void CPU::SLO() {

}

void CPU::SRE() {

}

void CPU::STA() {
	if (op.status & Op::Write) {
		mem.write(op.tempAddr, A);
		op.status |= Op::Done;
	}
}

void CPU::STP() {

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

}

// Processor status updates

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

// Miscellaneous

uint8_t CPU::BCD2binary(uint8_t BCDNum) {
	uint8_t binaryNum = 0;
	unsigned int powOfTen = 1;
	for (int i = 0; i < 2; ++i) {
		binaryNum += ((BCDNum & (0xf << (4 * i))) >> (4 * i)) * powOfTen;
		powOfTen *= 10;
	}
	return binaryNum;
}

uint8_t CPU::binary2BCD(uint8_t binaryNum) {
	uint8_t BCDNum = 0;
	for (int i = 0; i < 2; ++i) {
		BCDNum = BCDNum | ((binaryNum % 10) << (4 * i));
		binaryNum /= 10;
	}
	return BCDNum;
}

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

unsigned int CPU::getTotalInst() {
	return totalInst;
}