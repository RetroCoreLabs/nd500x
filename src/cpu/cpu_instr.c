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
    uint8_t prefixes_mask;
    uint8_t variant;
    uint32_t op_templates[4];
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

static InstrMeta g_fallback_unknown = {0xFFFF, "???", 0, 0};

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
            g_table[i].prefixes_mask = g_nd500_instrs[i].prefixes_mask;
            g_table[i].variant = g_nd500_instrs[i].variant;
            for (int j = 0; j < 4; j++) g_table[i].op_templates[j] = g_nd500_instrs[i].op_templates[j];
        }
    }
    /* Linear scan for now */
    for (size_t i = 0; i < g_table_count; ++i) {
        if (g_table[i].opcode == opcode) return &g_table[i];
    }
    return &g_fallback_unknown;
}

const char* nd500_instr_mnemonic(uint16_t opcode) {
	return lookup(opcode)->mnemonic;
}

int nd500_instr_opcode_length(uint16_t opcode) {
    /* Check high byte for long opcode marker (little-endian: bits 15-12) */
    return ((opcode >> 8) >= 0xFC) ? 2 : 1;
}

int nd500_instr_operand_count(uint16_t opcode) {
    return lookup(opcode)->operands;
}

int nd500_instr_has_rn(uint16_t opcode) {
    /* R_N bit is 0x40 in prefixes; check mask if present */
    const InstrMeta* im = lookup(opcode);
    return (im->prefixes_mask & 0x40) ? 1 : 0;
}

char nd500_instr_default_dtype(uint16_t opcode) {
    const InstrMeta* im = lookup(opcode);
    if (im->prefixes_mask & 0x08) return 'W';
    if (im->prefixes_mask & 0x02) return 'B';
    if (im->prefixes_mask & 0x04) return 'H';
    return 'W';
}

int nd500_instr_dest_reg(uint16_t opcode) {
    /* Heuristic: low 2 bits select r1..r4 when R_N applies */
    if (!nd500_instr_has_rn(opcode)) return -1;
    return (int)((opcode & 0x0003));
}

uint8_t nd500_instr_prefixes_mask(uint16_t opcode) {
    const InstrMeta* im = lookup(opcode);
    return im->prefixes_mask;
}

uint8_t nd500_instr_variant(uint16_t opcode) {
    const InstrMeta* im = lookup(opcode);
    return im->variant;
}

const char* nd500_instr_dtype_prefix(uint16_t opcode) {
    /* Map variant number to data type prefix */
    /* Typical order: BI=0, BY=1, H=2, W=3, F=4, D=5 */
    const InstrMeta* im = lookup(opcode);
    uint8_t mask = im->prefixes_mask;
    uint8_t var = im->variant;
    
    /* Count which data types are available and map variant to type */
    int idx = 0;
    if (mask & 0x01) { if (var == idx++) return "bi"; } /* BI */
    if (mask & 0x02) { if (var == idx++) return "by"; } /* BY */
    if (mask & 0x04) { if (var == idx++) return "h"; }  /* H */
    if (mask & 0x08) { if (var == idx++) return "w"; }  /* W */
    if (mask & 0x10) { if (var == idx++) return "f"; }  /* F */
    if (mask & 0x20) { if (var == idx++) return "d"; }  /* D */
    
    /* Default to 'w' if no match */
    return "w";
}

int nd500_instr_is_branch(uint16_t opcode) {
    /* Branch/call instructions have operand 0 with direct encoding (O_DIR bit set) */
    return nd500_instr_operand_is_direct(opcode, 0);
}

int nd500_instr_operand_is_direct(uint16_t opcode, uint8_t operand_idx) {
    /* Check operandTemplates O_DIR bit (0x20000) for this operand */
    const InstrMeta* im = lookup(opcode);
    if (operand_idx >= 4) return 0;
    uint32_t tmpl = im->op_templates[operand_idx];
    return (tmpl & 0x20000) ? 1 : 0;
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

static uint8_t data_part_size(uint8_t ac) {
    if (ac < 0xC0) return 0; /* short codes have no extra bytes */
    if (ac == 0xC8) return 0; /* ALT */
    if (ac >= 0xD0 && ac <= 0xD3) return 0; /* REGISTER */
    if (ac >= 0xF0 && ac <= 0xF3) return 0; /* DESCRIPTOR */
    if (ac == 0xC1 || ac == 0xC5 || ac == 0xC9 || ac == 0xCD ||
        (ac >= 0xD4 && ac <= 0xD7) ||
        (ac >= 0xE4 && ac <= 0xE7) ||
        (ac >= 0xF4 && ac <= 0xF7)) return 1; /* byte */
    if (ac == 0xC2 || ac == 0xC6 || ac == 0xCA || ac == 0xCE ||
        (ac >= 0xD8 && ac <= 0xDB) ||
        (ac >= 0xE8 && ac <= 0xEB) ||
        (ac >= 0xF8 && ac <= 0xFB)) return 2; /* halfword */
    if (ac == 0xCC) return 8; /* double */
    return 4; /* default word */
}

static uint8_t read_data_part(Nd500Machine* m, uint32_t base, uint8_t addr_code, uint8_t* out, uint8_t out_cap) {
    uint8_t len = data_part_size(addr_code);
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
    /* For little-endian, check HIGH byte (b1) for long opcode marker 0xFC-0xFF */
    int oplen = (b1 >= 0xFC) ? 2 : 1;
    uint16_t opcode = (oplen == 2) ? (uint16_t)b0 | ((uint16_t)b1 << 8) : (uint16_t)b0;
	out->opcode = opcode;
	out->opcode_len = (uint8_t)oplen;
	out->mnemonic = nd500_instr_mnemonic(opcode);
    
    /* Capture bytes as we read them */
    out->bytes[0] = b0;
    if (oplen == 2) out->bytes[1] = b1;
    /* Set operand_count from table */
    out->operand_count = (uint8_t)nd500_instr_operand_count(opcode);
    /* For unknown/invalid opcodes, treat as 1 or 2-byte unknown based on opcode_len and return early */
    if (opcode == 0 || !out->mnemonic || strcmp(out->mnemonic, "???") == 0) {
        out->total_len = out->opcode_len;
        return 0;
    }
    uint32_t cursor = pc + out->opcode_len;
    
    /* Special handling for instructions with direct operands (no address code) */
    /* Check if operand 0 is direct using operandTemplates */
    if (out->operand_count >= 1 && nd500_instr_operand_is_direct(opcode, 0)) {
        /* Operand 0 is direct - read inline bytes without address code */
        Nd500OperandDecoded *op = &out->operands[0];
        uint8_t var = nd500_instr_variant(opcode);
        op->has_alt_prefix = 0;
        op->has_desc_prefix = 0;
        op->address_code = 0xFF; /* Special marker for direct operand */
        op->mode = ND500_ADDR_CONSTANT;
        op->reg = 0;
        
        /* Determine size from template bits: O_BS=0x01, O_HS=0x04, O_WS=0x08, O_DS=0x10 */
        /* Priority: DS > WS > HS > BS (larger sizes take precedence) */
        uint32_t tmpl = lookup(opcode)->op_templates[0];
        uint8_t disp_len;
        if (tmpl & 0x10) disp_len = 8;      /* O_DS - double */
        else if (tmpl & 0x08) disp_len = 4; /* O_WS - word */
        else if (tmpl & 0x04) disp_len = 2; /* O_HS - halfword */
        else if (tmpl & 0x01) disp_len = 1; /* O_BS - byte */
        else disp_len = 4;                  /* Default word */
        
        op->data_len = disp_len;
        for (uint8_t i = 0; i < disp_len; i++) {
            op->data[i] = nd500_bus_read8(m, cursor + i);
            if (oplen + i < 32) out->bytes[oplen + i] = op->data[i];
        }
        cursor += disp_len;
        
        /* If only 1 operand, we're done */
        if (out->operand_count == 1) {
            out->total_len = (uint32_t)(cursor - pc);
            return 0;
        }
        /* Otherwise continue to decode remaining operands with standard addressing */
    }
    
    /* Decode operand address codes and data parts */
    uint32_t byte_idx = (uint32_t)(cursor - pc); /* Start from current cursor position */
    uint8_t start_operand = nd500_instr_operand_is_direct(opcode, 0) ? 1 : 0; /* Skip first if it was direct */
    for (uint8_t i = start_operand; i < out->operand_count && i < 4; ++i) {
        Nd500OperandDecoded *op = &out->operands[i];
        /* Handle optional ALT/DESC prefixes */
        uint8_t ac = nd500_bus_read8(m, cursor);
        op->has_alt_prefix = 0;
        op->has_desc_prefix = 0;
        while (ac == 0xC8 || (ac >= 0xF0 && ac <= 0xF3)) {
            if (byte_idx < 32) out->bytes[byte_idx++] = ac;
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
        if (byte_idx < 32) out->bytes[byte_idx++] = ac;
        op->mode = classify_mode(ac);
        /* Only set reg from AC when mode uses AC low bits for register and no DESC prefix was present */
        if (!op->has_desc_prefix && (op->mode == ND500_ADDR_REGISTER || op->mode == ND500_ADDR_PREINDEXED)) {
            op->reg = ac & 0x03;
        }
        cursor += 1;
        /* Read a data part based on addressing */
        if (op->mode == ND500_ADDR_LOCAL || op->mode == ND500_ADDR_RECORD || op->mode == ND500_ADDR_ABSOLUTE || op->mode == ND500_ADDR_PREINDEXED || op->mode == ND500_ADDR_CONSTANT || op->mode == ND500_ADDR_LOCAL_IND || op->mode == ND500_ADDR_LOCAL_PI || op->mode == ND500_ADDR_ABSOLUTE_PI || op->mode == ND500_ADDR_LOCAL_IND_PI) {
            op->data_len = read_data_part(m, cursor, ac, op->data, sizeof(op->data));
            for (uint8_t j = 0; j < op->data_len && byte_idx < 32; j++) {
                out->bytes[byte_idx++] = op->data[j];
            }
            cursor += op->data_len;
        } else {
            op->data_len = 0;
        }
        /* Do not override operand decoding for R_N; destination register is encoded in opcode, not operands */
    }
    out->total_len = (uint32_t)(cursor - pc);
	return 0;
}

static uint32_t get_operand_value32(const Nd500OperandDecoded* op) {
    /* Little-endian decode to match nd500-dis formatting */
    uint32_t v = 0;
    if (op->data_len == 1) v = op->data[0];
    else if (op->data_len == 2) v = (uint32_t)op->data[0] | ((uint32_t)op->data[1] << 8);
    else if (op->data_len >= 4) v = (uint32_t)op->data[0] | ((uint32_t)op->data[1] << 8) | ((uint32_t)op->data[2] << 16) | ((uint32_t)op->data[3] << 24);
    return v;
}

static uint32_t get_short_embedded(const Nd500OperandDecoded* op) {
    return (uint32_t)(op->address_code & 0x3F);
}

static uint32_t compute_effective_address(Nd500Cpu* cpu, const Nd500OperandDecoded* op) {
    switch (op->mode) {
        case ND500_ADDR_ABSOLUTE:
        case ND500_ADDR_ABSOLUTE_PI:
            return get_operand_value32(op);
        case ND500_ADDR_LOCAL:
        case ND500_ADDR_LOCAL_PI:
        case ND500_ADDR_LOCAL_IND:
        case ND500_ADDR_LOCAL_IND_PI:
            return cpu->B + get_operand_value32(op);
        case ND500_ADDR_RECORD:
            return cpu->R + get_operand_value32(op);
        case ND500_ADDR_LOCAL_SHORT:
            return cpu->B + (get_short_embedded(op) * 4u);
        case ND500_ADDR_RECORD_SHORT:
            return cpu->R + (get_short_embedded(op) * 4u);
        case ND500_ADDR_PREINDEXED:
            if (op->reg < 4) return cpu->I[op->reg] + get_operand_value32(op);
            return get_operand_value32(op);
        default:
            return 0;
    }
}

static uint32_t read_operand_w(Nd500Cpu* cpu, const Nd500OperandDecoded* op) {
    switch (op->mode) {
        case ND500_ADDR_CONSTANT:
            return get_operand_value32(op);
        case ND500_ADDR_CONSTANT_SHORT:
            return get_short_embedded(op);
        case ND500_ADDR_REGISTER:
            if (op->reg < 4) return cpu->I[op->reg];
            return 0;
        case ND500_ADDR_ABSOLUTE:
        case ND500_ADDR_ABSOLUTE_PI:
        case ND500_ADDR_LOCAL:
        case ND500_ADDR_LOCAL_PI:
        case ND500_ADDR_LOCAL_IND:
        case ND500_ADDR_LOCAL_IND_PI:
        case ND500_ADDR_RECORD:
        case ND500_ADDR_LOCAL_SHORT:
        case ND500_ADDR_RECORD_SHORT:
        case ND500_ADDR_PREINDEXED: {
            uint32_t ea = compute_effective_address(cpu, op);
            return nd500_bus_read32(cpu->machine, ea);
        }
        default:
            return 0;
    }
}

static void write_operand_w(Nd500Cpu* cpu, const Nd500OperandDecoded* op, uint32_t value) {
    switch (op->mode) {
        case ND500_ADDR_REGISTER:
            if (op->reg < 4) cpu->I[op->reg] = value;
            break;
        case ND500_ADDR_ABSOLUTE:
        case ND500_ADDR_ABSOLUTE_PI:
        case ND500_ADDR_LOCAL:
        case ND500_ADDR_LOCAL_PI:
        case ND500_ADDR_LOCAL_IND:
        case ND500_ADDR_LOCAL_IND_PI:
        case ND500_ADDR_RECORD:
        case ND500_ADDR_LOCAL_SHORT:
        case ND500_ADDR_RECORD_SHORT:
        case ND500_ADDR_PREINDEXED: {
            uint32_t ea = compute_effective_address(cpu, op);
            nd500_bus_write32(cpu->machine, ea, value);
            break;
        }
        default:
            break;
    }
}

void nd500_execute_decoded(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (!cpu || !cpu->machine || !fi || !fi->mnemonic) return;
    /* Minimal: implement move and comp as examples */
    if (strcmp(fi->mnemonic, "move") == 0 && fi->operand_count == 2) {
        const Nd500OperandDecoded* src = &fi->operands[0];
        const Nd500OperandDecoded* dst = &fi->operands[1];
        uint32_t val = read_operand_w(cpu, src);
        write_operand_w(cpu, dst, val);
        return;
    }
    if (strcmp(fi->mnemonic, "comp") == 0 && fi->operand_count == 1) {
        /* Compare with zero as placeholder */
        const Nd500OperandDecoded* s = &fi->operands[0];
        uint32_t v = read_operand_w(cpu, s);
        /* Update FLAGS basic */
        cpu->FLAGS = 0;
        if (v == 0) cpu->FLAGS |= 1; /* Z */
        return;
    }
}


