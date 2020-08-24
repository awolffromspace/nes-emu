#ifndef OP_H
#define OP_H

#include <cstdint>

class Op {
	public:
		Op();
		void reset();

		enum AddrMode {
			Absolute = 1,
			AbsoluteX = 2,
			AbsoluteY = 3,
			Accumulator = 4,
			Immediate = 5,
			Implied = 6,
			Indirect = 7,
			IndirectX = 8,
			IndirectY = 9,
			Relative = 10,
			ZeroPage = 11,
			ZeroPageX = 12,
			ZeroPageY = 13
		};
		// Powers of two are used, so this enum can be used as a bitmask
		enum Status {
			// When read operations can modify a register
			Modify = 1,
			// When write operations can write to memory
			Write = 2,
			// When read-modify-write operations can re-read from memory
			Reread = 4,
			// When read-modify-write operations can write unmodified data to
			// memory
			WriteUnmodified = 8,
			// When read-modify-write operations can write modified data to
			// memory
			WriteModified = 16,
			// When the operation is completely finished
			Done = 32
		};

	private:
		// Instruction
		uint32_t inst;
		// Address for the beginning of the instruction (i.e. opcode)
		uint16_t PC;
		// Operation code
		uint8_t opcode;
		// Operand with the least significant byte
		uint8_t operandLo;
		// Operand with the most significant byte
		// OperandLo and OperandHi combine to form one expression.
		// e.g. AND $xxyy retrieves val from memory location yyxx
		// where xx is operandLo and yy is operandHi
		uint8_t operandHi;
		// Value to operate on
		uint8_t val;
		// Temporary address to hold when needed
		uint16_t tempAddr;
		// Addressing mode that the operation uses
		int addrMode;
		// How many cycles the operation has taken thus far
		unsigned int cycles;
		// Indicates any cycle-relevant info about the operation
		int status;
		friend class CPU;
};

#endif