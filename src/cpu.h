#ifndef CPU_H
#define CPU_H

#include <bitset>

#include "apu.h"
#include "cpu-op.h"
#include "io.h"
#include "ppu.h"
#include "ram.h"

#define LOWER_IRQ_ADDR 0xfffe
#define LOWER_NMI_ADDR 0xfffa
#define UPPER_IRQ_ADDR 0xffff
#define UPPER_NMI_ADDR 0xfffb

// Central Processing Unit

class CPU {
    public:
        CPU();
        void clear();
        void step(SDL_Renderer* renderer, SDL_Texture* texture);

        // Struct that represents the CPU's state. Used for comparisons
        struct State {
            uint16_t pc;
            uint8_t sp;
            uint8_t a;
            uint8_t x;
            uint8_t y;
            uint8_t p;
            unsigned int totalCycles;
        };

        // File Reading
        void readInInst(const std::string& filename);
        void readInINES(const std::string& filename);

        // Miscellaneous Functions
        void updateButton(const SDL_Event& event);
        bool compareState(const struct CPU::State& state) const;

        // Getters
        uint32_t getFutureInst();
        uint16_t getPC() const;
        unsigned int getTotalCycles() const;
        bool isEndOfProgram() const;
        bool isHaltAtBrk() const;
        unsigned int getOpCycles() const;
        uint8_t readRAM(uint16_t addr) const;
        unsigned int getTotalPPUCycles() const;
        uint8_t readPRG(uint16_t addr) const;

        // Setters
        void setHaltAtBrk(bool h);
        void setMute(bool m);
        void clearTotalPPUCycles();

        // Printing
        void print(bool isCycleDone) const;
        void printUnknownOp() const;
        void printStateInst(uint32_t inst) const;
        void printPPU() const;

    private:
        uint16_t pc; // Program Counter
        uint8_t sp; // Stack Pointer
        uint8_t a; // Accumulator
        uint8_t x; // Index register X
        uint8_t y; // Index register Y
        uint8_t p; // Processor status
        CPUOp op; // Current operation
        RAM ram; // Random Access Memory
        PPU ppu; // Picture Processing Unit
        APU apu; // Audio Processing Unit
        IO io; // Input/Output (joysticks)
        MMC mmc; // Memory Management Controller (mapper)
        unsigned int totalCycles; // Total number of cycles since initialization
        bool endOfProgram; // Set to true if haltAtBrk is true and break operation is ran
        bool haltAtBrk; // Set to true if the program should halt when the break operation is ran
        bool mute; // Set to true to hide debug info

        // These arrays map machine language opcodes to addressing mode and operation function calls
        // that are associated with said opcodes
        typedef void (CPU::*funcPtr)();
        funcPtr addrModeArr[256] = {
            &CPU::imp, &CPU::idx, &CPU::imp, &CPU::idx, &CPU::zpg, &CPU::zpg, &CPU::zpg, &CPU::zpg,
            &CPU::imp, &CPU::imm, &CPU::acc, &CPU::imm, &CPU::abs, &CPU::abs, &CPU::abs, &CPU::abs,
            &CPU::rel, &CPU::idy, &CPU::imp, &CPU::idy, &CPU::zpx, &CPU::zpx, &CPU::zpx, &CPU::zpx,
            &CPU::imp, &CPU::aby, &CPU::imp, &CPU::aby, &CPU::abx, &CPU::abx, &CPU::abx, &CPU::abx,
            &CPU::abs, &CPU::idx, &CPU::imp, &CPU::idx, &CPU::zpg, &CPU::zpg, &CPU::zpg, &CPU::zpg,
            &CPU::imp, &CPU::imm, &CPU::acc, &CPU::imm, &CPU::abs, &CPU::abs, &CPU::abs, &CPU::abs,
            &CPU::rel, &CPU::idy, &CPU::imp, &CPU::idy, &CPU::zpx, &CPU::zpx, &CPU::zpx, &CPU::zpx,
            &CPU::imp, &CPU::aby, &CPU::imp, &CPU::aby, &CPU::abx, &CPU::abx, &CPU::abx, &CPU::abx,
            &CPU::imp, &CPU::idx, &CPU::imp, &CPU::idx, &CPU::zpg, &CPU::zpg, &CPU::zpg, &CPU::zpg,
            &CPU::imp, &CPU::imm, &CPU::acc, &CPU::imm, &CPU::abs, &CPU::abs, &CPU::abs, &CPU::abs,
            &CPU::rel, &CPU::idy, &CPU::imp, &CPU::idy, &CPU::zpx, &CPU::zpx, &CPU::zpx, &CPU::zpx,
            &CPU::imp, &CPU::aby, &CPU::imp, &CPU::aby, &CPU::abx, &CPU::abx, &CPU::abx, &CPU::abx,
            &CPU::imp, &CPU::idx, &CPU::imp, &CPU::idx, &CPU::zpg, &CPU::zpg, &CPU::zpg, &CPU::zpg,
            &CPU::imp, &CPU::imm, &CPU::acc, &CPU::imm, &CPU::idr, &CPU::abs, &CPU::abs, &CPU::abs,
            &CPU::rel, &CPU::idy, &CPU::imp, &CPU::idy, &CPU::zpx, &CPU::zpx, &CPU::zpx, &CPU::zpx,
            &CPU::imp, &CPU::aby, &CPU::imp, &CPU::aby, &CPU::abx, &CPU::abx, &CPU::abx, &CPU::abx,
            &CPU::imm, &CPU::idx, &CPU::imm, &CPU::idx, &CPU::zpg, &CPU::zpg, &CPU::zpg, &CPU::zpg,
            &CPU::imp, &CPU::imm, &CPU::imp, &CPU::imm, &CPU::abs, &CPU::abs, &CPU::abs, &CPU::abs,
            &CPU::rel, &CPU::idy, &CPU::imp, &CPU::idy, &CPU::zpx, &CPU::zpx, &CPU::zpy, &CPU::zpy,
            &CPU::imp, &CPU::aby, &CPU::imp, &CPU::aby, &CPU::abx, &CPU::abx, &CPU::aby, &CPU::aby,
            &CPU::imm, &CPU::idx, &CPU::imm, &CPU::idx, &CPU::zpg, &CPU::zpg, &CPU::zpg, &CPU::zpg,
            &CPU::imp, &CPU::imm, &CPU::imp, &CPU::imm, &CPU::abs, &CPU::abs, &CPU::abs, &CPU::abs,
            &CPU::rel, &CPU::idy, &CPU::imp, &CPU::idy, &CPU::zpx, &CPU::zpx, &CPU::zpy, &CPU::zpy,
            &CPU::imp, &CPU::aby, &CPU::imp, &CPU::aby, &CPU::abx, &CPU::abx, &CPU::aby, &CPU::aby,
            &CPU::imm, &CPU::idx, &CPU::imm, &CPU::idx, &CPU::zpg, &CPU::zpg, &CPU::zpg, &CPU::zpg,
            &CPU::imp, &CPU::imm, &CPU::imp, &CPU::imm, &CPU::abs, &CPU::abs, &CPU::abs, &CPU::abs,
            &CPU::rel, &CPU::idy, &CPU::imp, &CPU::idy, &CPU::zpx, &CPU::zpx, &CPU::zpx, &CPU::zpx,
            &CPU::imp, &CPU::aby, &CPU::imp, &CPU::aby, &CPU::abx, &CPU::abx, &CPU::abx, &CPU::abx,
            &CPU::imm, &CPU::idx, &CPU::imm, &CPU::idx, &CPU::zpg, &CPU::zpg, &CPU::zpg, &CPU::zpg,
            &CPU::imp, &CPU::imm, &CPU::imp, &CPU::imm, &CPU::abs, &CPU::abs, &CPU::abs, &CPU::abs,
            &CPU::rel, &CPU::idy, &CPU::imp, &CPU::idy, &CPU::zpx, &CPU::zpx, &CPU::zpx, &CPU::zpx,
            &CPU::imp, &CPU::aby, &CPU::imp, &CPU::aby, &CPU::abx, &CPU::abx, &CPU::abx, &CPU::abx
        };
        funcPtr opcodeArr[256] = {
            &CPU::brk,   &CPU::ora,   &CPU::stp,   &CPU::slo,   &CPU::nop,   &CPU::ora,
            &CPU::asl,   &CPU::slo,   &CPU::php,   &CPU::ora,   &CPU::asl,   &CPU::anc,
            &CPU::nop,   &CPU::ora,   &CPU::asl,   &CPU::slo,   &CPU::bpl,   &CPU::ora,
            &CPU::stp,   &CPU::slo,   &CPU::nop,   &CPU::ora,   &CPU::asl,   &CPU::slo,
            &CPU::clc,   &CPU::ora,   &CPU::nop,   &CPU::slo,   &CPU::nop,   &CPU::ora,
            &CPU::asl,   &CPU::slo,   &CPU::jsr,   &CPU::andOp, &CPU::stp,   &CPU::rla,
            &CPU::bit,   &CPU::andOp, &CPU::rol,   &CPU::rla,   &CPU::plp,   &CPU::andOp,
            &CPU::rol,   &CPU::anc,   &CPU::bit,   &CPU::andOp, &CPU::rol,   &CPU::rla,
            &CPU::bmi,   &CPU::andOp, &CPU::stp,   &CPU::rla,   &CPU::nop,   &CPU::andOp,
            &CPU::rol,   &CPU::rla,   &CPU::sec,   &CPU::andOp, &CPU::nop,   &CPU::rla,
            &CPU::nop,   &CPU::andOp, &CPU::rol,   &CPU::rla,   &CPU::rti,   &CPU::eor,
            &CPU::stp,   &CPU::sre,   &CPU::nop,   &CPU::eor,   &CPU::lsr,   &CPU::sre,
            &CPU::pha,   &CPU::eor,   &CPU::lsr,   &CPU::alr,   &CPU::jmp,   &CPU::eor,
            &CPU::lsr,   &CPU::sre,   &CPU::bvc,   &CPU::eor,   &CPU::stp,   &CPU::sre,
            &CPU::nop,   &CPU::eor,   &CPU::lsr,   &CPU::sre,   &CPU::cli,   &CPU::eor,
            &CPU::nop,   &CPU::sre,   &CPU::nop,   &CPU::eor,   &CPU::lsr,   &CPU::sre,
            &CPU::rts,   &CPU::adc,   &CPU::stp,   &CPU::rra,   &CPU::nop,   &CPU::adc,
            &CPU::ror,   &CPU::rra,   &CPU::pla,   &CPU::adc,   &CPU::ror,   &CPU::arr,
            &CPU::jmp,   &CPU::adc,   &CPU::ror,   &CPU::rra,   &CPU::bvs,   &CPU::adc,
            &CPU::stp,   &CPU::rra,   &CPU::nop,   &CPU::adc,   &CPU::ror,   &CPU::rra,
            &CPU::sei,   &CPU::adc,   &CPU::nop,   &CPU::rra,   &CPU::nop,   &CPU::adc,
            &CPU::ror,   &CPU::rra,   &CPU::nop,   &CPU::sta,   &CPU::nop,   &CPU::sax,
            &CPU::sty,   &CPU::sta,   &CPU::stx,   &CPU::sax,   &CPU::dey,   &CPU::nop,
            &CPU::txa,   &CPU::xaa,   &CPU::sty,   &CPU::sta,   &CPU::stx,   &CPU::sax,
            &CPU::bcc,   &CPU::sta,   &CPU::stp,   &CPU::ahx,   &CPU::sty,   &CPU::sta,
            &CPU::stx,   &CPU::sax,   &CPU::tya,   &CPU::sta,   &CPU::txs,   &CPU::tas,
            &CPU::shy,   &CPU::sta,   &CPU::shx,   &CPU::ahx,   &CPU::ldy,   &CPU::lda,
            &CPU::ldx,   &CPU::lax,   &CPU::ldy,   &CPU::lda,   &CPU::ldx,   &CPU::lax,
            &CPU::tay,   &CPU::lda,   &CPU::tax,   &CPU::lax,   &CPU::ldy,   &CPU::lda,
            &CPU::ldx,   &CPU::lax,   &CPU::bcs,   &CPU::lda,   &CPU::stp,   &CPU::lax,
            &CPU::ldy,   &CPU::lda,   &CPU::ldx,   &CPU::lax,   &CPU::clv,   &CPU::lda,
            &CPU::tsx,   &CPU::las,   &CPU::ldy,   &CPU::lda,   &CPU::ldx,   &CPU::lax,
            &CPU::cpy,   &CPU::cmp,   &CPU::nop,   &CPU::dcp,   &CPU::cpy,   &CPU::cmp,
            &CPU::dec,   &CPU::dcp,   &CPU::iny,   &CPU::cmp,   &CPU::dex,   &CPU::axs,
            &CPU::cpy,   &CPU::cmp,   &CPU::dec,   &CPU::dcp,   &CPU::bne,   &CPU::cmp,
            &CPU::stp,   &CPU::dcp,   &CPU::nop,   &CPU::cmp,   &CPU::dec,   &CPU::dcp,
            &CPU::cld,   &CPU::cmp,   &CPU::nop,   &CPU::dcp,   &CPU::nop,   &CPU::cmp,
            &CPU::dec,   &CPU::dcp,   &CPU::cpx,   &CPU::sbc,   &CPU::nop,   &CPU::isc,
            &CPU::cpx,   &CPU::sbc,   &CPU::inc,   &CPU::isc,   &CPU::inx,   &CPU::sbc,
            &CPU::nop,   &CPU::sbc,   &CPU::cpx,   &CPU::sbc,   &CPU::inc,   &CPU::isc,
            &CPU::beq,   &CPU::sbc,   &CPU::stp,   &CPU::isc,   &CPU::nop,   &CPU::sbc,
            &CPU::inc,   &CPU::isc,   &CPU::sed,   &CPU::sbc,   &CPU::nop,   &CPU::isc,
            &CPU::nop,   &CPU::sbc,   &CPU::inc,   &CPU::isc
        };

        // Addressing Modes
        void abs(); // ABSolute
        void abx(); // ABsolute, X
        void aby(); // ABsolute, Y
        void acc(); // ACCumulator
        void imm(); // IMMediate
        void imp(); // IMPlied
        void idr(); // InDiRect
        void idx(); // InDirect, X (AKA Indexed Indirect)
        void idy(); // InDirect, Y (AKA Indirect Indexed)
        void rel(); // RELative
        void zpg(); // Zero PaGe
        void zpx(); // Zero Page, X
        void zpy(); // Zero Page, Y

        // Operations
        // Functions without comments next to them are unofficial opcodes
        void adc(); // ADd with Carry
        void ahx();
        void alr();
        void andOp(); // bitwise AND with accumulator
        void anc();
        void arr();
        void asl(); // Arithmetic Shift Left
        void axs();
        void bcc(); // Branch on Carry Clear
        void bcs(); // Branch on Carry Set
        void beq(); // Branch on EQual
        void bit(); // test BITs
        void bmi(); // Branch on MInus
        void bne(); // Branch on Not Equal
        void bpl(); // Branch on PLus
        void brk(); // BReaK
        void bvc(); // Branch on oVerflow Clear
        void bvs(); // Branch on oVerflow Set
        void clc(); // CLear Carry
        void cld(); // CLear Decimal
        void cli(); // CLear Interrupt
        void clv(); // CLear oVerflow
        void cmp(); // CoMPare accumulator
        void cpx(); // ComPare X register
        void cpy(); // ComPare Y register
        void dcp();
        void dec(); // DECrement memory
        void dex(); // DEcrement X
        void dey(); // DEcrement Y
        void eor(); // bitwise Exclusive OR
        void inc(); // INCrement memory
        void inx(); // INcrement X
        void iny(); // INcrement Y
        void isc();
        void jmp(); // JuMP
        void jsr(); // Jump to SubRoutine
        void las();
        void lax();
        void lda(); // LoaD Accumulator
        void ldx(); // LoaD X register
        void ldy(); // LoaD Y register
        void lsr(); // Logical Shift Right
        void nop(); // No OPeration
        void ora(); // bitwise OR with Accumulator
        void pha(); // PusH Accumulator
        void php(); // PusH Processor status
        void pla(); // PuLl Accumulator
        void plp(); // PuLl Processor status
        void rla();
        void rol(); // ROtate Left
        void ror(); // ROtate Right
        void rra();
        void rti(); // ReTurn from Interrupt
        void rts(); // ReTurn from Subroutine
        void sax();
        void sbc(); // SuBtract with Carry
        void sec(); // SEt Carry
        void sed(); // SEt Decimal
        void sei(); // SEt Interrupt
        void shx();
        void shy();
        void slo();
        void sre();
        void sta(); // STore Accumulator
        void stp(); // STop the Processor
        void stx(); // STore X register
        void sty(); // STore Y register
        void tas();
        void tax(); // Transfer A to X
        void tay(); // Transfer A to Y
        void tsx(); // Transfer Stack pointer to X
        void txa(); // Transfer X to A
        void txs(); // Transfer X to Stack pointer
        void tya(); // Transfer Y to A
        void xaa();

        // Read/Write Functions
        uint8_t read(uint16_t addr);
        void write(uint16_t addr, uint8_t val);
        void oamDMATransfer();

        // Interrupts
        void pollInterrupts();
        void prepareIRQ();
        void prepareNMI();
        void prepareReset();

        // Processor Status Updates
        void updateZeroFlag(uint8_t result);
        void updateNegativeFlag(uint8_t result);

        // Processor Status Getters
        bool isCarry() const;
        bool isZero() const;
        bool areInterruptsDisabled() const;
        bool isOverflow() const;
        bool isNegative() const;

        // Processor Status Setters
        void setCarryFlag(bool val);
        void setZeroFlag(bool val);
        void setInterruptDisable(bool val);
        void setDecimalMode(bool val);
        void setOverflowFlag(bool val);
        void setNegativeFlag(bool val);

        // Processor Status Flags
        // Used for getting/setting bits in the P register
        enum ProcessorStatus {
            // Set if the last instruction resulted in overflow from bit 7 or underflow from bit 0
            Carry = 1,
            // Set if the result of the last instruction was zero
            Zero = 2,
            // Set to prevent IRQ interrupts. Set by SEI and cleared by CLI
            InterruptDisable = 4,
            // Set to switch to BCD mode. The NES' CPU, 2A03, ignores this flag and doesn't support
            // BCD
            DecimalMode = 8,
            // Set if BRK has been executed. The CPU always has this flag set except when pushing
            // the P register to the stack for non-BRK IRQ, NMI, and reset interrupts
            Break = 16,
            // Unused flag that doesn't exist in the CPU and is always set if read
            UnusedFlag = 32,
            // Set if an invalid two's complement result was obtained by the last instruction. E.g.,
            // adding two positive numbers and getting a negative
            Overflow = 64,
            // Set if the result of the last instruction was negative. I.e., bit 7 is 1
            Negative = 128
        };
};

#endif