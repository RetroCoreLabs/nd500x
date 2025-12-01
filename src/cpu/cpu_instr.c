#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "cpu_protos.h"
#include "../machine/machine_protos.h"
#include "nd500_instructions.h"
#include "nd500_mmu.h"

/* ═══════════════════════════════════════════════════════
 * MMU-AWARE MEMORY ACCESS HELPERS
 * ═══════════════════════════════════════════════════════
 * These functions handle MMU translation automatically when enabled.
 * They provide a clean abstraction for instruction implementations.
 */

/**
 * Read 8-bit value with MMU translation
 * @param cpu   CPU state (for MMU translation)
 * @param vaddr Virtual address to read from
 * @param is_write 0 for read, 1 for write access (for permission checking)
 * @param is_instruction 1 for instruction fetch, 0 for data access
 * @return Physical memory contents
 */
static inline uint8_t mmu_read8(Nd500Cpu* cpu, uint32_t vaddr, int is_write, int is_instruction) {
	if (!cpu || !cpu->machine) return 0;

	/* Translate virtual → physical if MMU enabled */
	uint32_t paddr = vaddr;
	if (cpu->machine->mmu_enabled) {
		paddr = nd500_mmu_translate(cpu, vaddr, is_write, is_instruction);
	}

	/* Access physical memory via bus (no further translation) */
	uint8_t byte = nd500_bus_read8(cpu->machine, paddr);
	return byte;
}

/**
 * Write 8-bit value with MMU translation
 */
static inline void mmu_write8(Nd500Cpu* cpu, uint32_t vaddr, uint8_t val) {
	if (!cpu || !cpu->machine) return;

	uint32_t paddr = vaddr;
	if (cpu->machine->mmu_enabled) {
		paddr = nd500_mmu_translate(cpu, vaddr, 1, 0); /* is_write=1, is_instruction=0 */
	}

	nd500_bus_write8(cpu->machine, paddr, val);
}

/**
 * Read 16-bit value with MMU translation
 */
static inline uint16_t mmu_read16(Nd500Cpu* cpu, uint32_t vaddr, int is_write, int is_instruction) {
	if (!cpu || !cpu->machine) return 0;

	uint32_t paddr = vaddr;
	if (cpu->machine->mmu_enabled) {
		paddr = nd500_mmu_translate(cpu, vaddr, is_write, is_instruction);
	}

	return nd500_bus_read16(cpu->machine, paddr);
}

/**
 * Write 16-bit value with MMU translation
 */
static inline void mmu_write16(Nd500Cpu* cpu, uint32_t vaddr, uint16_t val) {
	if (!cpu || !cpu->machine) return;

	uint32_t paddr = vaddr;
	if (cpu->machine->mmu_enabled) {
		paddr = nd500_mmu_translate(cpu, vaddr, 1, 0);
	}

	nd500_bus_write16(cpu->machine, paddr, val);
}

/**
 * Read 32-bit value with MMU translation
 */
static inline uint32_t mmu_read32(Nd500Cpu* cpu, uint32_t vaddr, int is_write, int is_instruction) {
	if (!cpu || !cpu->machine) return 0;

	uint32_t paddr = vaddr;
	if (cpu->machine->mmu_enabled) {
		paddr = nd500_mmu_translate(cpu, vaddr, is_write, is_instruction);
	}

	return nd500_bus_read32(cpu->machine, paddr);
}

/**
 * Write 32-bit value with MMU translation
 */
static inline void mmu_write32(Nd500Cpu* cpu, uint32_t vaddr, uint32_t val) {
	if (!cpu || !cpu->machine) return;

	uint32_t paddr = vaddr;
	if (cpu->machine->mmu_enabled) {
		paddr = nd500_mmu_translate(cpu, vaddr, 1, 0);
	}

	nd500_bus_write32(cpu->machine, paddr, val);
}

/* ═══════════════════════════════════════════════════════ */

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

/* Forward declarations */
static uint32_t compute_effective_address(Nd500Cpu* cpu, const Nd500OperandDecoded* op);
static uint32_t get_operand_value32(const Nd500OperandDecoded* op);
static uint32_t get_short_embedded(const Nd500OperandDecoded* op);

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
    /* Operand data: NOT instruction bytes, use data access for MMU */
    if (m->cpu) {
        for (uint8_t i = 0; i < len; ++i) {
            /* Operand data is DATA, not instruction - use is_instruction=0 for correct MMU translation */
            out[i] = mmu_read8(m->cpu, base + i, 0, 0); /* is_write=0, is_instruction=0 */
        }
    } else {
        /* Debugger/disassembler: direct physical access */
        for (uint8_t i = 0; i < len; ++i) {
            out[i] = nd500_bus_read8(m, base + i);
        }
    }
    return len;
}

int nd500_decode_at(Nd500Machine* m, uint32_t pc, Nd500FetchedInstruction* out) {
	if (!m || !out) return -1;
	memset(out, 0, sizeof(*out));
	out->address = pc;

	/* Fetch opcode with MMU translation if CPU available */
	uint8_t b0, b1;
	if (m->cpu) {
		b0 = mmu_read8(m->cpu, pc, 0, 1);     /* is_write=0, is_instruction=1 */
		b1 = mmu_read8(m->cpu, pc+1, 0, 1);
	} else {
		/* Debugger/disassembler: direct physical access */
		b0 = nd500_bus_read8(m, pc);
		b1 = nd500_bus_read8(m, pc+1);
	}
    /* Check FIRST byte (b0) for 2-byte opcode prefix 0xFC-0xFF */
    int oplen = (b0 >= 0xFC) ? 2 : 1;
    uint16_t opcode = (oplen == 2) ? ((uint16_t)b0 << 8) | (uint16_t)b1 : (uint16_t)b0;
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

    /* Extract metadata from InstrMeta table (like C# FetchedInstruction) */
    const InstrMeta* instr_meta = lookup(opcode);
    if (instr_meta) {
        /* Extract target register from opcode low bits (I1-I4, A1-A4, etc.) */
        /* Registers are numbered 1-4 (I1=1, I2=2, I3=3, I4=4) to match C# */
        out->target_register = ((opcode & 0x03) + 1);

        /* Use variant field from dispatch table - this is authoritative for data type */
        /* Variant: 0=BYTE, 1=HALFWORD, 2=WORD, 3=FLOAT, 4=DOUBLE */
        uint8_t variant = instr_meta->variant;
        out->uses_float_registers = false;
        switch (variant) {
            case 0: out->data_type = ND500_DTYPE_BYTE; break;
            case 1: out->data_type = ND500_DTYPE_HALFWORD; break;
            case 2: out->data_type = ND500_DTYPE_WORD; break;
            case 3: out->data_type = ND500_DTYPE_WORD; out->uses_float_registers = true; break;       /* Float */
            case 4: out->data_type = ND500_DTYPE_DOUBLEWORD; out->uses_float_registers = true; break; /* Double */
            default: out->data_type = ND500_DTYPE_WORD; break;
        }

        /* NOTE: Removed 0xFD00 BYTE override - it incorrectly affected GETBI WORD variants.
         * The variant field from the dispatch table correctly indicates data type. */

        /* AssignTo/AssignFrom (0x0004-0x001B) use 6-variant pattern:
         * BY=0x04-07, H=0x08-0B, W=0x0C-0F, F=0x10-13, D=0x14-17
         * Then AssignFrom continues: BY=0x18-1B with 4 registers each
         */
        if (opcode >= 0x0004 && opcode <= 0x0017) {
            uint8_t type_offset = ((opcode - 0x0004) >> 2);  /* Offset from 0x04 */
            out->uses_float_registers = false;
            switch (type_offset) {
                case 0:  /* 0x04-07: BY */
                    out->data_type = ND500_DTYPE_BYTE;
                    break;
                case 1:  /* 0x08-0B: H */
                    out->data_type = ND500_DTYPE_HALFWORD;
                    break;
                case 2:  /* 0x0C-0F: W */
                    out->data_type = ND500_DTYPE_WORD;
                    break;
                case 3:  /* 0x10-13: F */
                    out->data_type = ND500_DTYPE_WORD;
                    out->uses_float_registers = true;
                    break;
                case 4:  /* 0x14-17: D */
                    out->data_type = ND500_DTYPE_DOUBLEWORD;
                    out->uses_float_registers = true;
                    break;
            }
        }
        /* AssignFrom (0x0018-0x002B) same pattern, offset by 0x14 */
        else if (opcode >= 0x0018 && opcode <= 0x002B) {
            uint8_t type_offset = ((opcode - 0x0018) >> 2);
            out->uses_float_registers = false;
            switch (type_offset) {
                case 0:  /* 0x18-1B: BY */
                    out->data_type = ND500_DTYPE_BYTE;
                    break;
                case 1:  /* 0x1C-1F: H */
                    out->data_type = ND500_DTYPE_HALFWORD;
                    break;
                case 2:  /* 0x20-23: W */
                    out->data_type = ND500_DTYPE_WORD;
                    break;
                case 3:  /* 0x24-27: F */
                    out->data_type = ND500_DTYPE_WORD;
                    out->uses_float_registers = true;
                    break;
                case 4:  /* 0x28-2B: D */
                    out->data_type = ND500_DTYPE_DOUBLEWORD;
                    out->uses_float_registers = true;
                    break;
            }
        }

        if (opcode >= 0x60 && opcode <= 0x7F) {
            /* Word/Float/Double arithmetic range */
            uint8_t subop = (opcode - 0x60) >> 2;
            if (subop == 0) out->data_type = ND500_DTYPE_WORD;        /* 0x60-63: W- */
            else if (subop == 1) out->uses_float_registers = true;    /* 0x64-67: F- */
            else if (subop == 2) { out->data_type = ND500_DTYPE_DOUBLEWORD; out->uses_float_registers = true; }
            else if (subop == 3) out->data_type = ND500_DTYPE_WORD;   /* 0x6C-6F: W* */
            else if (subop == 4) out->uses_float_registers = true;    /* 0x70-73: F* */
            else if (subop == 5) { out->data_type = ND500_DTYPE_DOUBLEWORD; out->uses_float_registers = true; }
        } else if (opcode >= 0xE4 && opcode <= 0xEF) {
            /* Word logical range (AND, OR, XOR) */
            out->data_type = ND500_DTYPE_WORD;
        }
    } else {
        /* No metadata - defaults */
        out->target_register = 0;
        out->data_type = ND500_DTYPE_WORD;
        out->uses_float_registers = false;
    }
    uint32_t cursor = pc + out->opcode_len;
    uint32_t byte_idx = out->opcode_len;

    /* Helper function to decode a single general operand at cursor position */
    /* Returns cursor advancement */
    #define DECODE_GENERAL_OPERAND(op_ptr, cursor_ptr, byte_idx_ptr) do { \
        Nd500OperandDecoded *_op = (op_ptr); \
        uint8_t _ac; \
        if (m->cpu) { \
            _ac = mmu_read8(m->cpu, *(cursor_ptr), 0, 1); \
        } else { \
            _ac = nd500_bus_read8(m, *(cursor_ptr)); \
        } \
        _op->has_alt_prefix = 0; \
        _op->has_desc_prefix = 0; \
        while (_ac == 0xC8 || (_ac >= 0xF0 && _ac <= 0xF3)) { \
            if (*(byte_idx_ptr) < sizeof(out->bytes)) out->bytes[(*byte_idx_ptr)++] = _ac; \
            if (_ac == 0xC8) { \
                _op->has_alt_prefix = 1; \
                (*(cursor_ptr)) += 1; \
            } else { \
                _op->has_desc_prefix = 1; \
                _op->reg = _ac & 0x03; \
                (*(cursor_ptr)) += 1; \
            } \
            if (m->cpu) { \
                _ac = mmu_read8(m->cpu, *(cursor_ptr), 0, 1); \
            } else { \
                _ac = nd500_bus_read8(m, *(cursor_ptr)); \
            } \
        } \
        _op->address_code = _ac; \
        if (*(byte_idx_ptr) < sizeof(out->bytes)) out->bytes[(*byte_idx_ptr)++] = _ac; \
        _op->mode = classify_mode(_ac); \
        if (!_op->has_desc_prefix && (_op->mode == ND500_ADDR_REGISTER || _op->mode == ND500_ADDR_PREINDEXED)) { \
            _op->reg = (_ac & 0x03) + 1; /* 0xD0-D3 -> registers 1-4 (I1-I4) */ \
        } \
        (*(cursor_ptr)) += 1; \
        if (_op->mode == ND500_ADDR_CONSTANT_SHORT || \
            _op->mode == ND500_ADDR_LOCAL_SHORT || \
            _op->mode == ND500_ADDR_RECORD_SHORT || \
            _op->mode == ND500_ADDR_REGISTER) { \
            _op->data_len = 0; \
        } else if (_op->mode == ND500_ADDR_LOCAL || _op->mode == ND500_ADDR_RECORD || \
                   _op->mode == ND500_ADDR_ABSOLUTE || _op->mode == ND500_ADDR_PREINDEXED || \
                   _op->mode == ND500_ADDR_CONSTANT || _op->mode == ND500_ADDR_LOCAL_IND || \
                   _op->mode == ND500_ADDR_LOCAL_PI || _op->mode == ND500_ADDR_ABSOLUTE_PI || \
                   _op->mode == ND500_ADDR_LOCAL_IND_PI) { \
            _op->data_len = read_data_part(m, *(cursor_ptr), _ac, _op->data, sizeof(_op->data)); \
            for (uint8_t _j = 0; _j < _op->data_len && *(byte_idx_ptr) < sizeof(out->bytes); _j++) { \
                out->bytes[(*byte_idx_ptr)++] = _op->data[_j]; \
            } \
            (*(cursor_ptr)) += _op->data_len; \
        } else { \
            _op->data_len = 0; \
        } \
    } while(0)

    /* Helper macro to decode a direct operand (inline data without address code) */
    #define DECODE_DIRECT_OPERAND(op_ptr, cursor_ptr, byte_idx_ptr, op_idx) do { \
        Nd500OperandDecoded *_op = (op_ptr); \
        _op->has_alt_prefix = 0; \
        _op->has_desc_prefix = 0; \
        _op->address_code = 0xFE + (op_idx); /* Special marker for direct operands */ \
        _op->mode = ND500_ADDR_CONSTANT; \
        _op->reg = 0; \
        /* Determine size from template bits or variant */ \
        uint32_t _tmpl = lookup(opcode)->op_templates[(op_idx) < 4 ? (op_idx) : 3]; \
        uint8_t _disp_len; \
        /* Use variant to determine direct operand size (matches C# DirectOperandSizes logic) */ \
        /* variant: 0=byte, 1=halfword, 2=word, 3=float, 4=double */ \
        uint8_t _variant = nd500_instr_variant(opcode); \
        if (_variant == 0) _disp_len = 1; \
        else if (_variant == 1) _disp_len = 2; \
        else if (_variant == 4) _disp_len = 8; \
        else _disp_len = 4; \
        (void)_tmpl; /* suppress unused warning */ \
        _op->data_len = _disp_len; \
        for (uint8_t _j = 0; _j < _disp_len; _j++) { \
            if (m->cpu) { \
                _op->data[_j] = mmu_read8(m->cpu, *(cursor_ptr) + _j, 0, 1); \
            } else { \
                _op->data[_j] = nd500_bus_read8(m, *(cursor_ptr) + _j); \
            } \
            if (*(byte_idx_ptr) < sizeof(out->bytes)) out->bytes[(*byte_idx_ptr)++] = _op->data[_j]; \
        } \
        *(cursor_ptr) += _disp_len; \
    } while(0)

    /* Check if this is CALL or CALLG (variable operand instructions) */
    int is_call = (opcode == 0x00C3 || opcode == 0x00B5);  /* call=0xC3, callg=0xB5 */
    uint8_t arg_count = 0;

    /* Decode all operands IN ORDER - handle both direct and non-direct */
    for (uint8_t i = 0; i < out->operand_count && i < ND500_MAX_OPERANDS; ++i) {
        if (nd500_instr_operand_is_direct(opcode, i)) {
            /* Direct operand - read inline data without address code */
            DECODE_DIRECT_OPERAND(&out->operands[i], &cursor, &byte_idx, i);
        } else {
            /* Non-direct operand - read address code and any following data */
            DECODE_GENERAL_OPERAND(&out->operands[i], &cursor, &byte_idx);
        }

        /* For CALL/CALLG, extract arg count from operand 1 */
        if (is_call && i == 1) {
            Nd500OperandDecoded *arg_count_op = &out->operands[1];
            if (arg_count_op->mode == ND500_ADDR_CONSTANT_SHORT) {
                /* Short constant: value is in address_code lower 6 bits */
                arg_count = arg_count_op->address_code & 0x3F;
            } else if (arg_count_op->mode == ND500_ADDR_CONSTANT && arg_count_op->data_len == 1) {
                /* Extended constant byte */
                arg_count = arg_count_op->data[0];
            }
            /* Limit to what we can handle */
            if (arg_count > ND500_MAX_OPERANDS - 2) {
                arg_count = ND500_MAX_OPERANDS - 2;
            }
        }
    }

    /* For CALL/CALLG: decode additional argument operands */
    if (is_call && arg_count > 0) {
        for (uint8_t i = 0; i < arg_count && (out->operand_count + i) < ND500_MAX_OPERANDS; ++i) {
            uint8_t arg_idx = out->operand_count + i;
            DECODE_GENERAL_OPERAND(&out->operands[arg_idx], &cursor, &byte_idx);
        }
        out->operand_count += arg_count;
    }

    #undef DECODE_GENERAL_OPERAND
    #undef DECODE_DIRECT_OPERAND

    /* === Compute effective addresses for all operands === */
    /* This computes final memory addresses where operand data resides */
    /* Must be done AFTER all operands are decoded and requires CPU register state */
    if (m->cpu) {
        for (uint8_t i = 0; i < out->operand_count && i < ND500_MAX_OPERANDS; ++i) {
            out->operands[i].effective_address = compute_effective_address(m->cpu, &out->operands[i]);
        }
    } else {
        /* No CPU linked yet - zero the addresses */
        for (uint8_t i = 0; i < out->operand_count && i < ND500_MAX_OPERANDS; ++i) {
            out->operands[i].effective_address = 0;
        }
    }
    
    out->total_len = (uint32_t)(cursor - pc);
	return 0;
}

static uint32_t get_operand_value32(const Nd500OperandDecoded* op) {
    /* Big-endian decode (ND-500 is big-endian) */
    uint32_t v = 0;
    if (op->data_len == 1) v = op->data[0];
    else if (op->data_len == 2) v = ((uint32_t)op->data[0] << 8) | (uint32_t)op->data[1];
    else if (op->data_len >= 4) v = ((uint32_t)op->data[0] << 24) | ((uint32_t)op->data[1] << 16) | ((uint32_t)op->data[2] << 8) | (uint32_t)op->data[3];
    return v;
}

static uint32_t get_short_embedded(const Nd500OperandDecoded* op) {
    return (uint32_t)(op->address_code & 0x3F);
}

/* Enhanced compute_effective_address matching C# implementation */
static uint32_t compute_effective_address(Nd500Cpu* cpu, const Nd500OperandDecoded* op) {
    uint32_t address = 0;
    int32_t displacement = 0;
    
    /* Extract displacement value (signed, big-endian) */
    if (op->data_len == 1) {
        displacement = (int8_t)op->data[0];
    } else if (op->data_len == 2) {
        uint16_t raw = ((uint16_t)op->data[0] << 8) | (uint16_t)op->data[1];  /* Big-endian */
        displacement = (int16_t)raw;
    } else if (op->data_len >= 4) {
        uint32_t raw = get_operand_value32(op);
        displacement = (int32_t)raw;
    }
    
    /* STEP 1: Calculate base address based on addressing mode */
    switch (op->mode) {
        case ND500_ADDR_ABSOLUTE:
        case ND500_ADDR_ABSOLUTE_PI:
            /* Absolute addressing - use displacement as absolute address */
            address = get_operand_value32(op);
            break;
            
        case ND500_ADDR_LOCAL:
        case ND500_ADDR_LOCAL_PI:
        case ND500_ADDR_LOCAL_IND:
        case ND500_ADDR_LOCAL_IND_PI:
            /* Local addressing - B register + displacement */
            address = (uint32_t)((int32_t)cpu->B + displacement);
            break;
            
        case ND500_ADDR_LOCAL_SHORT:
            /* Local short - B + embedded value * 4 */
            address = cpu->B + (get_short_embedded(op) * 4u);
            break;
            
        case ND500_ADDR_RECORD:
            /* Record addressing - R register + displacement */
            address = (uint32_t)((int32_t)cpu->R + displacement);
            break;
            
        case ND500_ADDR_RECORD_SHORT:
            /* Record short - R + embedded value * 4 */
            address = cpu->R + (get_short_embedded(op) * 4u);
            break;
            
        case ND500_ADDR_PREINDEXED:
            /* Pre-indexed - I[n] + displacement */
            if (op->reg < 4) {
                address = (uint32_t)((int32_t)cpu->I[op->reg] + displacement);
            } else {
                address = (uint32_t)displacement;
            }
            break;
            
        case ND500_ADDR_CONSTANT:
        case ND500_ADDR_CONSTANT_SHORT:
        case ND500_ADDR_REGISTER:
            /* Non-memory operands - return 0 */
            return 0;
            
        default:
            return 0;
    }
    
    /* STEP 2: Handle indirection (@b.xxx, @b.xxx+) */
    /* Read pointer from computed address */
    switch (op->mode) {
        case ND500_ADDR_LOCAL_IND:
        case ND500_ADDR_LOCAL_IND_PI:
            /* Indirect - read 32-bit pointer from address (DATA access, not instruction) */
            address = mmu_read32(cpu, address, 0, 0); /* is_write=0, is_instruction=0 */
            break;
        default:
            break;
    }
    
    /* STEP 3: Handle post-indexing (b.xxx+, @b.xxx+) */
    /* Add index register AFTER base+displacement (and after indirection) */
    switch (op->mode) {
        case ND500_ADDR_LOCAL_PI:
        case ND500_ADDR_LOCAL_IND_PI:
        case ND500_ADDR_ABSOLUTE_PI:
            /* Post-indexed - add I[reg] value */
            if (op->reg < 4) {
                address = (uint32_t)((int32_t)address + (int32_t)cpu->I[op->reg]);
            }
            break;
        default:
            break;
    }
    
    return address;
}

uint32_t read_operand_w(Nd500Cpu* cpu, const Nd500OperandDecoded* op) {
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
            /* Use pre-computed effective address from decode (DATA access, NOT instruction fetch) */
            return mmu_read32(cpu, op->effective_address, 0, 0); /* is_write=0, is_instruction=0 */
        }
        default:
            return 0;
    }
}

void write_operand_w(Nd500Cpu* cpu, const Nd500OperandDecoded* op, uint32_t value) {
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
            /* Use pre-computed effective address from decode (DATA access, NOT instruction fetch) */
            mmu_write32(cpu, op->effective_address, value); /* is_write=1, is_instruction=0 */
            break;
        }
        default:
            break;
    }
}

void nd500_execute_decoded(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (!cpu || !cpu->machine || !fi) return;

    /* O(1) dispatch using opcode-indexed function pointer table */
    InstrExecFunc func = g_instr_exec_table[fi->opcode];

    if (func == NULL) {
        /* No implementation for this opcode - raise illegal instruction trap */
        trap_illegal_instruction(cpu, cpu->PC, fi->opcode);
        return;
    }

    /* Call the instruction implementation */
    func(cpu, fi);
}


