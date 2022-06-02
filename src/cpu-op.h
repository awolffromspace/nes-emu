#ifndef CPUOP_H
#define CPUOP_H

#include <cstdint>

// CPU Operation
// Holds info specific to the current operation (i.e., instructions, interrupt prologues, and DMA
// transfers)

class CPUOp {
    public:
        CPUOp();
        void clear(bool clearInterrupts, bool clearDMA);
        void clearInterruptFlags();
        void clearDMA();
        bool crossedPageBoundary() const;

    private:
        // Instruction
        uint32_t inst;
        // Address for the beginning of the instruction (i.e., opcode)
        uint16_t pc;
        // Operation code
        uint8_t opcode;
        // Operand with the least significant byte
        uint8_t operandLo;
        // Operand with the most significant byte. OperandLo and OperandHi combine to form one
        // expression (e.g., AND $xxyy retrieves val from memory location yyxx where xx is operandLo
        // and yy is operandHi)
        uint8_t operandHi;
        // Value to operate on
        uint8_t val;
        // Temporary address to hold when needed
        uint16_t tempAddr;
        // Fixed version of the above temporary address in cases where the carry is supposed to
        // affect the upper 8 bits of the address but has to be fixed in a later cycle
        uint16_t fixedAddr;
        // Addressing mode that the operation uses. Depends on enum AddrMode
        unsigned int addrMode;
        // Type of instruction (i.e., read, write, or read-modify-write). Depends on enum InstType
        unsigned int instType;
        // How many cycles the operation has taken thus far
        unsigned int cycle;
        // How many cycles the DMA transfer has taken thus far
        unsigned int dmaCycle;

        // Status Flags

        // If read operations can modify a register
        bool modify;
        // If write operations can write to memory
        bool write;
        // If read-modify-write operations can write unmodified data to memory
        bool writeUnmodified;
        // If read-modify-write operations can write modified data to memory
        bool writeModified;
        // If an IRQ or maskable interrupt was triggered
        bool irq;
        bool brk;
        // If an NMI or non-maskable interrupt was triggered
        bool nmi;
        // If a reset interrupt was triggered
        bool reset;
        // If currently handling an interrupt
        bool interruptPrologue;
        // If performing a DMA transfer to the PPU OAM
        bool oamDMATransfer;
        // If the operation is completely finished
        bool done;

        friend class CPU;

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
};

#endif