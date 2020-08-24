#ifndef CPU_H
#define CPU_H

#include <bitset>
#include <iostream>

#include "memory.h"
#include "op.h"

class CPU {
	public:
		CPU();
		CPU(uint8_t data[]);
		void step();

		// Addressing Modes
		void ABS(); // Absolute
		void ABX(); // Absolute, X
		void ABY(); // Absolute, Y
		void ACC(); // Accumulator
		void IMM(); // Immediate
		void IMP(); // Implied
		void IDR(); // Indirect
		void IDX(); // Indirect, X (AKA Indexed Indirect)
		void IDY(); // Indirect, Y (AKA Indirect Indexed)
		void REL(); // Relative
		void ZPG(); // Zero Page
		void ZPX(); // Zero Page, X
		void ZPY(); // Zero Page, Y

		// Operations
		// Functions without comments next to them are unofficial opcodes
		void ADC(); // ADd with Carry
		void AHX();
		void ALR();
		void AND(); // bitwise AND with accumulator
		void ANC();
		void ARR();
		void ASL(); // Arithmetic Shift Left
		void AXS();
		void BCC(); // Branch on Carry Clear
		void BCS(); // Branch on Carry Set
		void BEQ(); // Branch on EQual
		void BIT(); // test BITs
		void BMI(); // Branch on MInus
		void BNE(); // Branch on Not Equal
		void BPL(); // Branch on PLus
		void BRK(); // BReaK
		void BVC(); // Branch on oVerflow Clear
		void BVS(); // Branch on oVerflow Set
		void CLC(); // CLear Carry
		void CLD(); // CLear Decimal
		void CLI(); // CLear Interrupt
		void CLV(); // CLear oVerflow
		void CMP(); // CoMPare accumulator
		void CPX(); // ComPare X register
		void CPY(); // ComPare Y register
		void DCP();
		void DEC(); // DECrement memory
		void DEX(); // DEcrement X
		void DEY(); // DEcrement Y
		void EOR(); // bitwise Exclusive OR
		void INC(); // INCrement memory
		void INX(); // INcrement X
		void INY(); // INcrement Y
		void ISC();
		void JMP(); // JuMP
		void JSR(); // Jump to SubRoutine
		void LAS();
		void LAX();
		void LDA(); // LoaD Accumulator
		void LDX(); // LoaD X register
		void LDY(); // LoaD Y register
		void LSR(); // Logical Shift Right
		void NOP(); // No OPeration
		void ORA(); // bitwise OR with Accumulator
		void PHA(); // PusH Accumulator
		void PHP(); // PusH Processor status
		void PLA(); // PuLl Accumulator
		void PLP(); // PuLl Processor status
		void RLA();
		void ROL(); // ROtate Left
		void ROR(); // ROtate Right
		void RRA();
		void RTI(); // ReTurn from Interrupt
		void RTS(); // ReTurn from Subroutine
		void SAX();
		void SBC(); // SuBtract with Carry
		void SEC(); // SEt Carry
		void SED(); // SEt Decimal
		void SEI(); // SEt Interrupt
		void SHX();
		void SHY();
		void SLO();
		void SRE();
		void STA(); // STore Accumulator
		void STP(); // STop the Processor
		void STX(); // STore X register
		void STY(); // STore Y register
		void TAS();
		void TAX(); // Transfer A to X
		void TAY(); // Transfer A to Y
		void TSX(); // Transfer Stack pointer to X
		void TXA(); // Transfer X to A
		void TXS(); // Transfer X to Stack pointer
		void TYA(); // Transfer Y to A
		void XAA();

		// Functions that update the processor status
		// Only the zero and negative flags have dedicated functions because
		// many operations set or clear them using the exact same conditions
		void updateZeroFlag(uint8_t result);
		void updateNegativeFlag(uint8_t result);

		uint8_t BCD2binary(uint8_t BCDNum);
		uint8_t binary2BCD(uint8_t binaryNum);
		void print(bool isCycleDone);
		void printUnknownOp();

	private:
		// Program Counter
		uint16_t PC;
		// Stack Pointer
		uint8_t SP;
		// Accumulator
		uint8_t A;
		// Index register X
		uint8_t X;
		// Index register Y
		uint8_t Y;
		// Processor status
		uint8_t P;
		// Current operation
		Op op;
		Memory mem;
		// Total number of cycles since initialization
		unsigned int totalCycles;

		// These arrays map machine language opcodes to addressing mode and
		// operation function calls that are associated with said opcodes
		typedef void (CPU::*funcPtr)();
		funcPtr addrModeArr[256] = {
			&CPU::IMP, &CPU::IDX, &CPU::IMP, &CPU::IDX, &CPU::ZPG, &CPU::ZPG,
			&CPU::ZPG, &CPU::ZPG, &CPU::IMP, &CPU::IMM, &CPU::ACC, &CPU::IMM,
			&CPU::ABS, &CPU::ABS, &CPU::ABS, &CPU::ABS, &CPU::REL, &CPU::IDY,
			&CPU::IMP, &CPU::IDY, &CPU::ZPX, &CPU::ZPX, &CPU::ZPX, &CPU::ZPX,
			&CPU::IMP, &CPU::ABY, &CPU::IMP, &CPU::ABY, &CPU::ABX, &CPU::ABX,
			&CPU::ABX, &CPU::ABX, &CPU::ABS, &CPU::IDX, &CPU::IMP, &CPU::IDX,
			&CPU::ZPG, &CPU::ZPG, &CPU::ZPG, &CPU::ZPG, &CPU::IMP, &CPU::IMM,
			&CPU::ACC, &CPU::IMM, &CPU::ABS, &CPU::ABS, &CPU::ABS, &CPU::ABS,
			&CPU::REL, &CPU::IDY, &CPU::IMP, &CPU::IDY, &CPU::ZPX, &CPU::ZPX,
			&CPU::ZPX, &CPU::ZPX, &CPU::IMP, &CPU::ABY, &CPU::IMP, &CPU::ABY,
			&CPU::ABX, &CPU::ABX, &CPU::ABX, &CPU::ABX, &CPU::IMP, &CPU::IDX,
			&CPU::IMP, &CPU::IDX, &CPU::ZPG, &CPU::ZPG, &CPU::ZPG, &CPU::ZPG,
			&CPU::IMP, &CPU::IMM, &CPU::ACC, &CPU::IMM, &CPU::ABS, &CPU::ABS,
			&CPU::ABS, &CPU::ABS, &CPU::REL, &CPU::IDY, &CPU::IMP, &CPU::IDY,
			&CPU::ZPX, &CPU::ZPX, &CPU::ZPX, &CPU::ZPX, &CPU::IMP, &CPU::ABY,
			&CPU::IMP, &CPU::ABY, &CPU::ABX, &CPU::ABX, &CPU::ABX, &CPU::ABX,
			&CPU::IMP, &CPU::IDX, &CPU::IMP, &CPU::IDX, &CPU::ZPG, &CPU::ZPG,
			&CPU::ZPG, &CPU::ZPG, &CPU::IMP, &CPU::IMM, &CPU::ACC, &CPU::IMM,
			&CPU::IDR, &CPU::ABS, &CPU::ABS, &CPU::ABS, &CPU::REL, &CPU::IDY,
			&CPU::IMP, &CPU::IDY, &CPU::ZPX, &CPU::ZPX, &CPU::ZPX, &CPU::ZPX,
			&CPU::IMP, &CPU::ABY, &CPU::IMP, &CPU::ABY, &CPU::ABX, &CPU::ABX,
			&CPU::ABX, &CPU::ABX, &CPU::IMM, &CPU::IDX, &CPU::IMM, &CPU::IDX,
			&CPU::ZPG, &CPU::ZPG, &CPU::ZPG, &CPU::ZPG, &CPU::IMP, &CPU::IMM,
			&CPU::IMP, &CPU::IMM, &CPU::ABS, &CPU::ABS, &CPU::ABS, &CPU::ABS,
			&CPU::REL, &CPU::IDY, &CPU::IMP, &CPU::IDY, &CPU::ZPX, &CPU::ZPX,
			&CPU::ZPY, &CPU::ZPY, &CPU::IMP, &CPU::ABY, &CPU::IMP, &CPU::ABY,
			&CPU::ABX, &CPU::ABX, &CPU::ABY, &CPU::ABY, &CPU::IMM, &CPU::IDX,
			&CPU::IMM, &CPU::IDX, &CPU::ZPG, &CPU::ZPG, &CPU::ZPG, &CPU::ZPG,
			&CPU::IMP, &CPU::IMM, &CPU::IMP, &CPU::IMM, &CPU::ABS, &CPU::ABS,
			&CPU::ABS, &CPU::ABS, &CPU::REL, &CPU::IDY, &CPU::IMP, &CPU::IDY,
			&CPU::ZPX, &CPU::ZPX, &CPU::ZPY, &CPU::ZPY, &CPU::IMP, &CPU::ABY,
			&CPU::IMP, &CPU::ABY, &CPU::ABX, &CPU::ABX, &CPU::ABY, &CPU::ABY,
			&CPU::IMM, &CPU::IDX, &CPU::IMM, &CPU::IDX, &CPU::ZPG, &CPU::ZPG,
			&CPU::ZPG, &CPU::ZPG, &CPU::IMP, &CPU::IMM, &CPU::IMP, &CPU::IMM,
			&CPU::ABS, &CPU::ABS, &CPU::ABS, &CPU::ABS, &CPU::REL, &CPU::IDY,
			&CPU::IMP, &CPU::IDY, &CPU::ZPX, &CPU::ZPX, &CPU::ZPX, &CPU::ZPX,
			&CPU::IMP, &CPU::ABY, &CPU::IMP, &CPU::ABY, &CPU::ABX, &CPU::ABX,
			&CPU::ABX, &CPU::ABX, &CPU::IMM, &CPU::IDX, &CPU::IMM, &CPU::IDX,
			&CPU::ZPG, &CPU::ZPG, &CPU::ZPG, &CPU::ZPG, &CPU::IMP, &CPU::IMM,
			&CPU::IMP, &CPU::IMM, &CPU::ABS, &CPU::ABS, &CPU::ABS, &CPU::ABS,
			&CPU::REL, &CPU::IDY, &CPU::IMP, &CPU::IDY, &CPU::ZPX, &CPU::ZPX,
			&CPU::ZPX, &CPU::ZPX, &CPU::IMP, &CPU::ABY, &CPU::IMP, &CPU::ABY,
			&CPU::ABX, &CPU::ABX, &CPU::ABX, &CPU::ABX
		};
		funcPtr opcodeArr[256] = {
			&CPU::BRK, &CPU::ORA, &CPU::STP, &CPU::SLO, &CPU::NOP, &CPU::ORA,
			&CPU::ASL, &CPU::SLO, &CPU::PHP, &CPU::ORA, &CPU::ASL, &CPU::ANC,
			&CPU::NOP, &CPU::ORA, &CPU::ASL, &CPU::SLO, &CPU::BPL, &CPU::ORA,
			&CPU::STP, &CPU::SLO, &CPU::NOP, &CPU::ORA, &CPU::ASL, &CPU::SLO,
			&CPU::CLC, &CPU::ORA, &CPU::NOP, &CPU::SLO, &CPU::NOP, &CPU::ORA,
			&CPU::ASL, &CPU::SLO, &CPU::JSR, &CPU::AND, &CPU::STP, &CPU::RLA,
			&CPU::BIT, &CPU::AND, &CPU::ROL, &CPU::RLA, &CPU::PLP, &CPU::AND,
			&CPU::ROL, &CPU::ANC, &CPU::BIT, &CPU::AND, &CPU::ROL, &CPU::RLA,
			&CPU::BMI, &CPU::AND, &CPU::STP, &CPU::RLA, &CPU::NOP, &CPU::AND,
			&CPU::ROL, &CPU::RLA, &CPU::SEC, &CPU::AND, &CPU::NOP, &CPU::RLA,
			&CPU::NOP, &CPU::AND, &CPU::ROL, &CPU::RLA, &CPU::RTI, &CPU::EOR,
			&CPU::STP, &CPU::SRE, &CPU::NOP, &CPU::EOR, &CPU::LSR, &CPU::SRE,
			&CPU::PHA, &CPU::EOR, &CPU::LSR, &CPU::ALR, &CPU::JMP, &CPU::EOR,
			&CPU::LSR, &CPU::SRE, &CPU::BVC, &CPU::EOR, &CPU::STP, &CPU::SRE,
			&CPU::NOP, &CPU::EOR, &CPU::LSR, &CPU::SRE, &CPU::CLI, &CPU::EOR,
			&CPU::NOP, &CPU::SRE, &CPU::NOP, &CPU::EOR, &CPU::LSR, &CPU::SRE,
			&CPU::RTS, &CPU::ADC, &CPU::STP, &CPU::RRA, &CPU::NOP, &CPU::ADC,
			&CPU::ROR, &CPU::RRA, &CPU::PLA, &CPU::ADC, &CPU::ROR, &CPU::ARR,
			&CPU::JMP, &CPU::ADC, &CPU::ROR, &CPU::RRA, &CPU::BVS, &CPU::ADC,
			&CPU::STP, &CPU::RRA, &CPU::NOP, &CPU::ADC, &CPU::ROR, &CPU::RRA,
			&CPU::SEI, &CPU::ADC, &CPU::NOP, &CPU::RRA, &CPU::NOP, &CPU::ADC,
			&CPU::ROR, &CPU::RRA, &CPU::NOP, &CPU::STA, &CPU::NOP, &CPU::SAX,
			&CPU::STY, &CPU::STA, &CPU::STX, &CPU::SAX, &CPU::DEY, &CPU::NOP,
			&CPU::TXA, &CPU::XAA, &CPU::STY, &CPU::STA, &CPU::STX, &CPU::SAX,
			&CPU::BCC, &CPU::STA, &CPU::STP, &CPU::AHX, &CPU::STY, &CPU::STA,
			&CPU::STX, &CPU::SAX, &CPU::TYA, &CPU::STA, &CPU::TXS, &CPU::TAS,
			&CPU::SHY, &CPU::STA, &CPU::SHX, &CPU::AHX, &CPU::LDY, &CPU::LDA,
			&CPU::LDX, &CPU::LAX, &CPU::LDY, &CPU::LDA, &CPU::LDX, &CPU::LAX,
			&CPU::TAY, &CPU::LDA, &CPU::TAX, &CPU::LAX, &CPU::LDY, &CPU::LDA,
			&CPU::LDX, &CPU::LAX, &CPU::BCS, &CPU::LDA, &CPU::STP, &CPU::LAX,
			&CPU::LDY, &CPU::LDA, &CPU::LDX, &CPU::LAX, &CPU::CLV, &CPU::LDA,
			&CPU::TSX, &CPU::LAS, &CPU::LDY, &CPU::LDA, &CPU::LDX, &CPU::LAX,
			&CPU::CPY, &CPU::CMP, &CPU::NOP, &CPU::DCP, &CPU::CPY, &CPU::CMP,
			&CPU::DEC, &CPU::DCP, &CPU::INY, &CPU::CMP, &CPU::DEX, &CPU::AXS,
			&CPU::CPY, &CPU::CMP, &CPU::DEC, &CPU::DCP, &CPU::BNE, &CPU::CMP,
			&CPU::STP, &CPU::DCP, &CPU::NOP, &CPU::CMP, &CPU::DEC, &CPU::DCP,
			&CPU::CLD, &CPU::CMP, &CPU::NOP, &CPU::DCP, &CPU::NOP, &CPU::CMP,
			&CPU::DEC, &CPU::DCP, &CPU::CPX, &CPU::SBC, &CPU::NOP, &CPU::ISC,
			&CPU::CPX, &CPU::SBC, &CPU::INC, &CPU::ISC, &CPU::INX, &CPU::SBC,
			&CPU::NOP, &CPU::SBC, &CPU::CPX, &CPU::SBC, &CPU::INC, &CPU::ISC,
			&CPU::BEQ, &CPU::SBC, &CPU::STP, &CPU::ISC, &CPU::NOP, &CPU::SBC,
			&CPU::INC, &CPU::ISC, &CPU::SED, &CPU::SBC, &CPU::NOP, &CPU::ISC,
			&CPU::NOP, &CPU::SBC, &CPU::INC, &CPU::ISC
		};
};

#endif