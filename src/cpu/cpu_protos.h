#pragma once
#include <stdint.h>
#include <setjmp.h>
#include "../machine/machine_types.h"

typedef struct Nd500Cpu {
	/* Core registers */
	uint32_t PC;
	/* Integer */
	uint32_t I[4];
	/* Float accumulators and extensions */
	uint32_t A[4];
	uint32_t E[4];
	/* Addressing registers */
	uint32_t L; /* link */
	uint32_t B; /* base */
	uint32_t R; /* record */
	/* Special */
	uint32_t TOS, LL, HL, THA;
	/* Control */
	uint32_t OTE1, OTE2, CTE1, CTE2, MTE1, MTE2, TEMM1, TEMM2;
	/* Status registers (64-bit split into two 32-bit registers) */
	uint32_t ST1, ST2;  /* Status register (64-bit) */
	/* Flags (simplified status) */
	uint32_t FLAGS;

	Nd500Machine* machine;
} Nd500Cpu;

/* ND-500 Trap System Definitions */
/* Based on ND-500 Reference Manual Chapter 6 - THE TRAP SYSTEM */

/* Non-ignorable trap bits (will trigger longjmp) */
#define TRAP_XSE  (1ULL << 32)  /* Index Scaling Error */
#define TRAP_IIC  (1ULL << 33)  /* Illegal Instruction Code */
#define TRAP_IOS  (1ULL << 34)  /* Illegal Operand Specifier */
#define TRAP_ISE  (1ULL << 35)  /* Instruction Sequence Error */
#define TRAP_PV   (1ULL << 36)  /* Protect Violation */

/* Fatal trap bits (always trigger longjmp) */
#define TRAP_THM  (1ULL << 37)  /* Trap Handler Missing */
#define TRAP_PGF  (1ULL << 38)  /* Page Fault */
#define TRAP_PWF  (1ULL << 39)  /* Power Failure */
#define TRAP_PRF  (1ULL << 40)  /* Processor Fault */
#define TRAP_HF   (1ULL << 41)  /* Hardware Fault */

/* Mask for traps that interrupt instruction execution */
#define TRAP_INTERRUPT_MASK (TRAP_XSE | TRAP_IIC | TRAP_IOS | \
                             TRAP_ISE | TRAP_PV | TRAP_THM | \
                             TRAP_PGF | TRAP_PWF | TRAP_PRF | TRAP_HF)

/* Ignorable traps (bits 11-29) - set status but don't longjmp */
#define TRAP_IVO  (1ULL << 11)  /* Invalid Operation */
#define TRAP_DZ   (1ULL << 12)  /* Divide by Zero */
#define TRAP_FU   (1ULL << 13)  /* Floating Underflow */
#define TRAP_FO   (1ULL << 14)  /* Floating Overflow */
#define TRAP_BO   (1ULL << 15)  /* BCD Overflow */
#define TRAP_IOV  (1ULL << 16)  /* Illegal Operand Value */
#define TRAP_SIT  (1ULL << 17)  /* Single Instruction Trap */
#define TRAP_BT   (1ULL << 18)  /* Branch Trap */
#define TRAP_CT   (1ULL << 19)  /* Call Trap */
#define TRAP_BPT  (1ULL << 20)  /* Breakpoint Trap */
#define TRAP_ATF  (1ULL << 21)  /* Address Trap Fetch */
#define TRAP_ATR  (1ULL << 22)  /* Address Trap Read */
#define TRAP_ATW  (1ULL << 23)  /* Address Trap Write */
#define TRAP_AZ   (1ULL << 24)  /* Address Zero Access */
#define TRAP_DR   (1ULL << 25)  /* Descriptor Range */
#define TRAP_IX   (1ULL << 26)  /* Illegal Index */
#define TRAP_STO  (1ULL << 27)  /* Stack Overflow */
#define TRAP_STU  (1ULL << 28)  /* Stack Underflow */
#define TRAP_PRT  (1ULL << 29)  /* Programmed Trap */

/* Mask for ignorable traps only (bits 11-29) */
#define TRAP_IGNORABLE_MASK 0x3FFFF800ULL

/* Global jump buffer for trap handling */
extern jmp_buf cpu_jmp_buf;

/* Trap system state */
typedef struct {
    int trap_occurred;           /* Flag indicating if a trap occurred */
    uint64_t trap_condition;     /* The trap condition that occurred */
    uint32_t trap_pc;            /* PC where trap occurred */
    uint32_t trap_data_addr;     /* Related data address */
    char trap_description[256];  /* Human-readable trap description */
} Nd500TrapState;

extern Nd500TrapState g_trap_state;

typedef struct Nd500Regs {
	uint32_t PC;
	uint32_t FLAGS;
	uint32_t I[4];
	uint32_t A[4];
	uint32_t E[4];
	uint32_t L, B, R;
	uint32_t TOS, LL, HL, THA;
	uint32_t OTE1, OTE2, CTE1, CTE2, MTE1, MTE2, TEMM1, TEMM2;
	uint32_t ST1, ST2;  /* Status registers (64-bit) */
} Nd500Regs;

void nd500_cpu_init(Nd500Cpu* cpu, Nd500Machine* machine);
void nd500_cpu_reset(Nd500Cpu* cpu);
void nd500_cpu_step(Nd500Cpu* cpu);
void nd500_cpu_get_regs(Nd500Cpu* cpu, Nd500Regs* out);
int nd500_cpu_run(Nd500Cpu* cpu, int steps);

/* Trap system functions */
void raise_trap(uint64_t trapBit, uint32_t trapPC, uint32_t dataAddr);
void check_pending_traps(Nd500Cpu* cpu);
void invoke_trap_handler(Nd500Cpu* cpu, uint64_t trapBit, uint32_t trappingP);

/* Trap state management */
void nd500_trap_clear(void);
int nd500_trap_occurred(void);
const Nd500TrapState* nd500_trap_get_state(void);
void nd500_trap_set_state(uint64_t condition, uint32_t pc, uint32_t data_addr, const char* description);

/* Trap helper functions */
void trap_illegal_instruction(uint32_t pc, uint32_t opcode);
void trap_illegal_operand(uint32_t pc);
void trap_instruction_sequence_error(uint32_t pc);
void trap_protect_violation(uint32_t pc, uint32_t address);
void trap_page_fault(uint32_t pc, uint32_t address);
void trap_divide_by_zero(uint32_t pc);
void trap_floating_overflow(uint32_t pc);
void trap_floating_underflow(uint32_t pc);
void trap_invalid_operation(uint32_t pc);
void trap_stack_overflow(uint32_t pc);
void trap_stack_underflow(uint32_t pc);
void trap_breakpoint(uint32_t pc);
void trap_single_instruction(uint32_t pc);
void trap_branch(uint32_t pc);
void trap_call(uint32_t pc);

/* Instruction metadata (loaded from build/src/cpu/instructions.json if available) */
int nd500_instr_load_default(void);
const char* nd500_instr_mnemonic(uint16_t opcode);
int nd500_instr_opcode_length(uint16_t opcode);
int nd500_instr_operand_count(uint16_t opcode);
int nd500_instr_has_rn(uint16_t opcode);
char nd500_instr_default_dtype(uint16_t opcode);
int nd500_instr_dest_reg(uint16_t opcode);
uint8_t nd500_instr_prefixes_mask(uint16_t opcode);
uint8_t nd500_instr_variant(uint16_t opcode);
const char* nd500_instr_dtype_prefix(uint16_t opcode);
int nd500_instr_is_branch(uint16_t opcode);
int nd500_instr_operand_is_direct(uint16_t opcode, uint8_t operand_idx);

/* Decoded instruction model */
typedef enum Nd500AddrMode {
    ND500_ADDR_UNKNOWN = 0,
    ND500_ADDR_CONSTANT_SHORT,
    ND500_ADDR_LOCAL_SHORT,
    ND500_ADDR_RECORD_SHORT,
    ND500_ADDR_LOCAL,
    ND500_ADDR_LOCAL_PI,
    ND500_ADDR_LOCAL_IND,
    ND500_ADDR_LOCAL_IND_PI,
    ND500_ADDR_RECORD,
    ND500_ADDR_PREINDEXED,
    ND500_ADDR_ABSOLUTE,
    ND500_ADDR_ABSOLUTE_PI,
    ND500_ADDR_CONSTANT,
    ND500_ADDR_REGISTER,
    ND500_ADDR_DESCRIPTOR,
    ND500_ADDR_ALTERNATIVE
} Nd500AddrMode;

typedef struct Nd500OperandDecoded {
    uint8_t address_code;
    uint8_t has_alt_prefix;
    uint8_t has_desc_prefix;
    uint8_t reg;
    Nd500AddrMode mode;
    uint8_t data_len;
    uint8_t data[8];
    uint32_t effective_address;  /* Computed effective address for memory operands */
} Nd500OperandDecoded;

typedef struct Nd500FetchedInstruction {
    uint32_t address;
    uint16_t opcode;
    uint8_t opcode_len;
    const char* mnemonic;
    uint8_t operand_count;
    Nd500OperandDecoded operands[4];
    uint32_t total_len;
    uint8_t bytes[32];  /* All bytes consumed by this instruction */
} Nd500FetchedInstruction;

int nd500_decode_at(Nd500Machine* m, uint32_t pc, Nd500FetchedInstruction* out);

/* Execute one decoded instruction */
void nd500_execute_decoded(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi);

/* Operand access helpers for instruction implementations */
uint32_t read_operand_w(Nd500Cpu* cpu, const Nd500OperandDecoded* op);
void write_operand_w(Nd500Cpu* cpu, const Nd500OperandDecoded* op, uint32_t value);


