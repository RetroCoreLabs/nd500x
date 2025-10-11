#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "cpu_protos.h"
#include "../machine/machine_protos.h"
#include "../../build/include/nd500_instructions_gen.h"

typedef struct InstrMeta {
	uint16_t opcode;
	char mnemonic[16];
	uint8_t operands;
} InstrMeta;

static InstrMeta* g_table = NULL;
static size_t g_table_count = 0;

int nd500_instr_load_default(void) {
	/* Minimal seed: unknown */
	if (g_table) return 0;
	g_table = (InstrMeta*)calloc(1, sizeof(InstrMeta));
	if (!g_table) return -1;
	g_table[0].opcode = 0xFFFF;
	strcpy(g_table[0].mnemonic, "???");
	g_table[0].operands = 0;
	g_table_count = 1;
	return 0;
}

static const InstrMeta* lookup(uint16_t opcode) {
    if (!g_table) {
        /* Map generated table to our simple view */
        g_table_count = g_nd500_instrs_count;
        g_table = (InstrMeta*)calloc(g_table_count, sizeof(InstrMeta));
        if (!g_table) {
            nd500_instr_load_default();
            return &g_table[0];
        }
        for (size_t i = 0; i < g_table_count; ++i) {
            g_table[i].opcode = g_nd500_instrs[i].opcode;
            snprintf(g_table[i].mnemonic, sizeof(g_table[i].mnemonic), "%s", g_nd500_instrs[i].mnemonic);
            g_table[i].operands = g_nd500_instrs[i].operands;
        }
    }
    /* Linear scan for now */
    for (size_t i = 0; i < g_table_count; ++i) {
        if (g_table[i].opcode == opcode) return &g_table[i];
    }
    return &g_table[0];
}

const char* nd500_instr_mnemonic(uint16_t opcode) {
	return lookup(opcode)->mnemonic;
}

int nd500_instr_opcode_length(uint16_t opcode) {
    const InstrMeta* im = lookup(opcode);
    /* Heuristic: if opcode < 0x0100 -> 1, else 2; refine later with operandTemplates */
    return (opcode < 0x0100) ? 1 : 2;
}

int nd500_instr_operand_count(uint16_t opcode) {
    return lookup(opcode)->operands;
}

static Nd500AddrMode classify_mode(uint8_t addr_code) {
	uint8_t top = (addr_code & 0xC0) >> 6;
	if (top == 0x00) return ND500_ADDR_CONSTANT_SHORT;
	if (top == 0x01) return ND500_ADDR_LOCAL_SHORT;
	if (top == 0x02) return ND500_ADDR_RECORD_SHORT;
    /* 0x3? extended - map common explicit encodings */
    if (addr_code == 0xC4) return ND500_ADDR_ABSOLUTE;            /* $address (word) */
    if ((addr_code >= 0xC1 && addr_code <= 0xC3)) return ND500_ADDR_LOCAL; /* b.N (1,2,4) */
    if ((addr_code >= 0xC5 && addr_code <= 0xC7)) return ND500_ADDR_LOCAL_IND; /* @b.N */
    if ((addr_code >= 0xC9 && addr_code <= 0xCB)) return ND500_ADDR_RECORD; /* r.N */
    if ((addr_code >= 0xD0 && addr_code <= 0xD3)) return ND500_ADDR_REGISTER; /* r1-r4 */
    if ((addr_code >= 0xD4 && addr_code <= 0xDF)) return ND500_ADDR_LOCAL_PI; /* b.N+ */
    if ((addr_code >= 0xE0 && addr_code <= 0xE3)) return ND500_ADDR_ABSOLUTE_PI; /* $addr+ */
    if ((addr_code >= 0xE4 && addr_code <= 0xEF)) return ND500_ADDR_LOCAL_IND_PI; /* @b.N+ */
    if ((addr_code >= 0xF0 && addr_code <= 0xF3)) return ND500_ADDR_DESCRIPTOR; /* DESC reg */
    if ((addr_code >= 0xF4 && addr_code <= 0xFF)) return ND500_ADDR_PREINDEXED; /* rN.(disp) */
    if (addr_code == 0xC8) return ND500_ADDR_ALTERNATIVE; /* ALT prefix (standalone if not pre-parsed) */
    if ((addr_code >= 0xCC && addr_code <= 0xCF)) return ND500_ADDR_CONSTANT; /* immediate const (1,2,4,8) */
	return ND500_ADDR_UNKNOWN;
}

static uint8_t read_data_part(Nd500Machine* m, uint32_t base, uint8_t addr_code, uint8_t* out, uint8_t out_cap) {
    /* Determine size by address code group */
    uint8_t len = 0;
    if (addr_code == 0xC4) {
        len = 4; /* absolute */
    } else if (addr_code >= 0xC1 && addr_code <= 0xC3) {
        static const uint8_t sizes[] = {1,2,4};
        len = sizes[(addr_code - 0xC1) & 0x03];
    } else if (addr_code >= 0xC5 && addr_code <= 0xC7) {
        static const uint8_t sizes[] = {1,2,4};
        len = sizes[(addr_code - 0xC5) & 0x03];
    } else if (addr_code >= 0xC9 && addr_code <= 0xCB) {
        static const uint8_t sizes[] = {1,2,4};
        len = sizes[(addr_code - 0xC9) & 0x03];
    } else if (addr_code >= 0xD4 && addr_code <= 0xDF) {
        static const uint8_t sizes[] = {1,2,4,4};
        len = sizes[((addr_code - 0xD4) >> 2) & 0x03];
    } else if (addr_code >= 0xE0 && addr_code <= 0xE3) {
        len = 4; /* absolute+ */
    } else if (addr_code >= 0xE4 && addr_code <= 0xEF) {
        static const uint8_t sizes[] = {1,2,4,4};
        len = sizes[((addr_code - 0xE4) >> 2) & 0x03];
    } else if (addr_code >= 0xF4 && addr_code <= 0xFF) {
        static const uint8_t sizes[] = {1,2,4,4};
        len = sizes[((addr_code - 0xF4) >> 2) & 0x03];
    } else if (addr_code >= 0xCC && addr_code <= 0xCF) {
        static const uint8_t sizes[] = {1,2,4,8};
        len = sizes[(addr_code - 0xCC) & 0x03];
    }
    if (len > out_cap) len = out_cap;
    for (uint8_t i = 0; i < len; ++i) out[i] = nd500_bus_read8(m, base + i);
    return len;
}

int nd500_decode_at(Nd500Machine* m, uint32_t pc, Nd500FetchedInstruction* out) {
	if (!m || !out) return -1;
	memset(out, 0, sizeof(*out));
	out->address = pc;
	uint8_t b0 = nd500_bus_read8(m, pc);
	uint8_t b1 = nd500_bus_read8(m, pc+1);
	uint16_t opcode = (uint16_t)b0 | ((uint16_t)b1 << 8);
	int oplen = nd500_instr_opcode_length(opcode);
	out->opcode = opcode;
	out->opcode_len = (uint8_t)oplen;
	out->mnemonic = nd500_instr_mnemonic(opcode);
    /* Set operand_count from table */
    out->operand_count = (uint8_t)nd500_instr_operand_count(opcode);
    uint32_t cursor = pc + out->opcode_len;
    /* Decode simplistic operand address codes and data parts */
    for (uint8_t i = 0; i < out->operand_count && i < 4; ++i) {
        Nd500OperandDecoded *op = &out->operands[i];
        /* Handle optional ALT/DESC prefixes */
        uint8_t ac = nd500_bus_read8(m, cursor);
        op->has_alt_prefix = 0;
        op->has_desc_prefix = 0;
        while (ac == 0xC8 || (ac >= 0xF0 && ac <= 0xF3)) {
            if (ac == 0xC8) {
                op->has_alt_prefix = 1;
                cursor += 1;
            } else {
                op->has_desc_prefix = 1;
                op->reg = ac & 0x03;
                cursor += 1;
            }
            ac = nd500_bus_read8(m, cursor);
        }
        op->address_code = ac;
        op->reg = ac & 0x03;
        op->mode = classify_mode(ac);
        cursor += 1;
        /* Read a data part heuristically for extended modes */
        if (op->mode == ND500_ADDR_LOCAL || op->mode == ND500_ADDR_RECORD || op->mode == ND500_ADDR_ABSOLUTE || op->mode == ND500_ADDR_PREINDEXED || op->mode == ND500_ADDR_CONSTANT || op->mode == ND500_ADDR_LOCAL_IND || op->mode == ND500_ADDR_LOCAL_PI || op->mode == ND500_ADDR_ABSOLUTE_PI || op->mode == ND500_ADDR_LOCAL_IND_PI) {
            op->data_len = read_data_part(m, cursor, ac, op->data, sizeof(op->data));
            cursor += op->data_len;
        } else {
            op->data_len = 0;
        }
    }
    out->total_len = (uint32_t)(cursor - pc);
	return 0;
}


