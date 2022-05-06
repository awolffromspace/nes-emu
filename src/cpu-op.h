#ifndef CPUOP_H
#define CPUOP_H

#include <cstdint>

class CPUOp {
    public:
        CPUOp();
        void clear(bool clearInterrupts);
        void clearInterruptFlags();
        void clearDMA();

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
        enum InstType {
            ReadInst = 1,
            WriteInst = 2,
            RMWInst = 3
        };
        // Powers of two are used, so this enum can be used as a bitmask
        enum OpStatus {
            // When read operations can modify a register
            Modify = 1,
            // When write operations can write to memory
            Write = 2,
            // When read-modify-write operations can write unmodified data to
            // memory
            WriteUnmodified = 4,
            // When read-modify-write operations can write modified data to
            // memory
            WriteModified = 8,
            // When an IRQ or maskable interrupt is triggered
            IRQ = 16,
            // When an NMI or non-maskable interrupt is triggered
            NMI = 32,
            // When a reset interrupt is triggered
            Reset = 64,
            // When currently handling an interrupt
            InterruptPrologue = 128,
            // When performing a DMA transfer to the PPU OAM
            OAMDMA = 256,
            // When the operation is completely finished
            Done = 512
        };

    private:
        // Instruction
        uint32_t inst;
        // Address for the beginning of the instruction (i.e., opcode)
        uint16_t pc;
        // Operation code
        uint8_t opcode;
        // Operand with the least significant byte
        uint8_t operandLo;
        // Operand with the most significant byte
        // OperandLo and OperandHi combine to form one expression
        // e.g., AND $xxyy retrieves val from memory location yyxx
        // where xx is operandLo and yy is operandHi
        uint8_t operandHi;
        // Value to operate on
        uint8_t val;
        // Temporary address to hold when needed
        uint16_t tempAddr;
        uint16_t fixedAddr;
        // Addressing mode that the operation uses
        unsigned int addrMode;
        unsigned int instType;
        // How many cycles the operation has taken thus far
        unsigned int cycle;
        unsigned int dmaCycle;
        // Indicates any cycle-relevant info about the operation
        // Depends on the enum Status
        unsigned int status;
        friend class CPU;
};

#endif