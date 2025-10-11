#pragma once
#include <stdint.h>
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
	/* Flags (simplified status) */
	uint32_t FLAGS;

	Nd500Machine* machine;
} Nd500Cpu;

typedef struct Nd500Regs {
	uint32_t PC;
	uint32_t FLAGS;
	uint32_t I[4];
	uint32_t A[4];
	uint32_t E[4];
	uint32_t L, B, R;
	uint32_t TOS, LL, HL, THA;
	uint32_t OTE1, OTE2, CTE1, CTE2, MTE1, MTE2, TEMM1, TEMM2;
} Nd500Regs;

void nd500_cpu_init(Nd500Cpu* cpu, Nd500Machine* machine);
void nd500_cpu_reset(Nd500Cpu* cpu);
void nd500_cpu_step(Nd500Cpu* cpu);
void nd500_cpu_get_regs(Nd500Cpu* cpu, Nd500Regs* out);

/* Instruction metadata (loaded from build/src/cpu/instructions.json if available) */
int nd500_instr_load_default(void);
const char* nd500_instr_mnemonic(uint16_t opcode);
int nd500_instr_opcode_length(uint16_t opcode);
int nd500_instr_operand_count(uint16_t opcode);

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
} Nd500OperandDecoded;

typedef struct Nd500FetchedInstruction {
    uint32_t address;
    uint16_t opcode;
    uint8_t opcode_len;
    const char* mnemonic;
    uint8_t operand_count;
    Nd500OperandDecoded operands[4];
    uint32_t total_len;
} Nd500FetchedInstruction;

int nd500_decode_at(Nd500Machine* m, uint32_t pc, Nd500FetchedInstruction* out);


