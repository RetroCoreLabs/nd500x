#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include "cpu_protos.h"
#include "../machine/machine_protos.h"
#include "nd500_instructions.h"
#include "nd500_mmu.h"

/* ═══════════════════════════════════════════════════════
 * INSTRUCTION PREFIX FLAGS
 * ═══════════════════════════════════════════════════════
 * Matches C# InstructionPrefixes enum in Enums.cs
 * Used to determine which data types an instruction supports
 */
#define ND500_PREFIX_BI   0x01   /* Bit field */
#define ND500_PREFIX_BY   0x02   /* Byte (8-bit) */
#define ND500_PREFIX_H    0x04   /* Halfword (16-bit) */
#define ND500_PREFIX_W    0x08   /* Word (32-bit) */
#define ND500_PREFIX_F    0x10   /* Float (32-bit) */
#define ND500_PREFIX_D    0x20   /* Double (64-bit) */
#define ND500_PREFIX_R_N  0x40   /* Register number in opcode */

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
/* Opt-in write-watch (ND500X_NC_WWATCH) for the NC crash record range. Logs any
 * store whose virtual address falls in [WWATCH_LO, WWATCH_HI). Off by default. */
#define NC_WWATCH_LO 0x1802A1B0u
#define NC_WWATCH_HI 0x1802A1D0u
static inline void nc_wwatch(Nd500Cpu* cpu, uint32_t vaddr, int width, uint32_t val) {
	static int mode = -1;
	if (mode < 0) { const char* e = getenv("ND500X_NC_WWATCH"); mode = (e && e[0] && e[0] != '0') ? 1 : 0; }
	if (!mode) return;
	if (vaddr >= NC_WWATCH_LO && vaddr < NC_WWATCH_HI) {
		fprintf(stderr, "[WWATCH] instr=%llu PC~=%08X w%d [%08X] <- %0*X\n",
		        (unsigned long long)cpu->instruction_count, cpu->PC, width,
		        vaddr, width/4, val);
		fflush(stderr);
	}
}

static inline void mmu_write8(Nd500Cpu* cpu, uint32_t vaddr, uint8_t val) {
	if (!cpu || !cpu->machine) return;
	nc_wwatch(cpu, vaddr, 8, val);

	uint32_t paddr = vaddr;
	if (cpu->machine->mmu_enabled) {
		paddr = nd500_mmu_translate(cpu, vaddr, 1, 0); /* is_write=1, is_instruction=0 */
		/* Translation fault: MUST NOT commit - paddr == vaddr here, so the
		 * store would land at the untranslated address AS PHYSICAL memory.
		 * (Root cause of the exec-argv byte corrupting a context block:
		 * a demand-fault mid arg-copy fell through and wrote the argument
		 * byte at phys=vaddr.) Handler clears trap state synchronously,
		 * so check the per-instruction abort flag too. */
		if (nd500_trap_occurred() || cpu->instr_aborted) return;
	}

	{ extern void nd500_ptewatch_wr(uint32_t,uint32_t,uint32_t,uint32_t,int);
	  nd500_ptewatch_wr(cpu->PC, vaddr, paddr, val, 8); }
	/* Kernel path-copy tracer (env ND500X_SLASHDBG) - see write_memory_8. */
	{
		static int sld = -1;
		if (sld < 0) { const char* e = getenv("ND500X_SLASHDBG"); sld = (e && e[0] && e[0] != '0') ? 1 : 0; }
		if (sld && cpu->CED == 0 && val == 0x2F) {
			static unsigned n = 0;
			if (n++ < 60)
				fprintf(stderr, "[SLASH] mmu_w8 vaddr=0x%08X paddr=0x%08X PC=0x%08X\n",
				        vaddr, paddr, cpu->PC);
		}
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

	if (cpu->CED == 0 && vaddr >= 0xF0000000u && getenv("ND500X_UDATADBG"))
		printf("[UDATA16] PC=0x%08X CED=0 read16 vaddr=0x%08X paddr=0x%08X\n",
		       cpu->PC, vaddr, paddr);
	return nd500_bus_read16(cpu->machine, paddr);
}

/**
 * Write 16-bit value with MMU translation
 */
static inline void mmu_write16(Nd500Cpu* cpu, uint32_t vaddr, uint16_t val) {
	if (!cpu || !cpu->machine) return;
	nc_wwatch(cpu, vaddr, 16, val);

	uint32_t paddr = vaddr;
	if (cpu->machine->mmu_enabled) {
		paddr = nd500_mmu_translate(cpu, vaddr, 1, 0);
		if (nd500_trap_occurred() || cpu->instr_aborted) return;  /* see mmu_write8 */
	}

	{ extern void nd500_ptewatch_wr(uint32_t,uint32_t,uint32_t,uint32_t,int);
	  nd500_ptewatch_wr(cpu->PC, vaddr, paddr, val, 16); }
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

	if (cpu->CED == 0 && vaddr >= 0xF0000000u && getenv("ND500X_UDATADBG"))
		printf("[UDATA32] PC=0x%08X CED=0 read32 vaddr=0x%08X paddr=0x%08X\n",
		       cpu->PC, vaddr, paddr);
	return nd500_bus_read32(cpu->machine, paddr);
}

/**
 * Write 32-bit value with MMU translation
 */
static inline void mmu_write32(Nd500Cpu* cpu, uint32_t vaddr, uint32_t val) {
	if (!cpu || !cpu->machine) return;
	nc_wwatch(cpu, vaddr, 32, val);

	uint32_t paddr = vaddr;
	if (cpu->machine->mmu_enabled) {
		paddr = nd500_mmu_translate(cpu, vaddr, 1, 0);
		if (nd500_trap_occurred() || cpu->instr_aborted) return;  /* see mmu_write8 */
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
    uint8_t has_variable_operands;  /* 1 if instruction accepts variable operands (CALL, CALLG, POLY) */
    uint32_t op_templates[4];
} InstrMeta;

static InstrMeta* g_table = NULL;
static size_t g_table_count = 0;

/**
 * Determine data type from prefixes_mask and variant number.
 * Matches C# DetermineDataType() algorithm exactly.
 *
 * The algorithm builds a list of supported data types from the prefix bits,
 * then indexes into that list using (variant % count).
 *
 * @param prefixes_mask  Bitmask of supported data types (BI|BY|H|W|F|D)
 * @param variant        Variant number from dispatch table
 * @param uses_float     Output: true if this is a float/double type (uses A/E registers)
 * @return The data type enum value
 */
static Nd500DataType determine_datatype_from_prefixes(uint8_t prefixes_mask, uint8_t variant, bool *uses_float) {
    Nd500DataType types[6];
    int count = 0;

    /* Build type list in order: BI, BY, H, W, F, D */
    if (prefixes_mask & ND500_PREFIX_BI) types[count++] = ND500_DTYPE_BIT;        /* BI uses BIT addressing */
    if (prefixes_mask & ND500_PREFIX_BY) types[count++] = ND500_DTYPE_BYTE;
    if (prefixes_mask & ND500_PREFIX_H)  types[count++] = ND500_DTYPE_HALFWORD;
    if (prefixes_mask & ND500_PREFIX_W)  types[count++] = ND500_DTYPE_WORD;
    if (prefixes_mask & ND500_PREFIX_F)  types[count++] = ND500_DTYPE_FLOAT;      /* Float uses A registers */
    if (prefixes_mask & ND500_PREFIX_D)  types[count++] = ND500_DTYPE_DOUBLEWORD;

    if (count == 0) {
        *uses_float = false;
        return ND500_DTYPE_WORD;  /* Default */
    }

    int idx = variant % count;

    /* Determine if this is a float type */
    /* Float is present when F bit is set, and we've cycled past integer types */
    int int_count = 0;
    if (prefixes_mask & ND500_PREFIX_BI) int_count++;
    if (prefixes_mask & ND500_PREFIX_BY) int_count++;
    if (prefixes_mask & ND500_PREFIX_H)  int_count++;
    if (prefixes_mask & ND500_PREFIX_W)  int_count++;

    *uses_float = (idx >= int_count) && (prefixes_mask & (ND500_PREFIX_F | ND500_PREFIX_D));

    return types[idx];
}

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

/* Direct-mapped opcode -> metadata index, the same shape as the exec dispatch
 * table g_instr_exec_table[65536] in nd500_instructions.c.
 *
 * lookup() used to scan all 1078 rows linearly, and it is called about four
 * times per emulated instruction (mnemonic, operand count, has_rn, ...). A
 * callgrind profile of an NDIX boot put lookup at 64.9% of ALL executed host
 * instructions - 20.8 billion of 32 billion - far and away the hottest thing
 * in the emulator.
 *
 * Measured on the real table, 20M lookups, against the linear scan:
 *
 *   pattern    scan       this      binary search   compact 2-range index
 *   skewed     39.08 ns   1.51 ns   37.88 ns        4.25 ns
 *   uniform   189.31 ns   1.64 ns   62.45 ns        4.06 ns
 *
 * The 512 KB of pointers costs nothing in practice: only the opcodes real code
 * executes are ever touched, so the resident working set is a few cache lines.
 * A 2 KB index covering just the two live opcode ranges (0x00xx and
 * 0xFCxx-0xFFxx, which is where all 1048 of them are) was 2.8x SLOWER despite
 * fitting in L1, because it needs a range branch that mispredicts, while this
 * is a single branch-free load.
 *
 * FIRST row wins, exactly as the linear scan did. 22 opcodes appear more than
 * once in the table (clr, neg, laddr, rladdr); the duplicate rows differ only
 * in the variant field, so this preserves the previous behaviour byte for
 * byte. */
static const InstrMeta* g_meta_index[65536];
static int g_meta_index_built;

static void build_meta_index(void) {
    for (size_t i = 0; i < g_table_count; ++i) {
        uint16_t op = g_table[i].opcode;
        if (!g_meta_index[op]) g_meta_index[op] = &g_table[i];
    }
    g_meta_index_built = 1;
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
            g_table[i].prefixes_mask = g_nd500_instrs[i].prefixes_mask;
            g_table[i].variant = g_nd500_instrs[i].variant;
            g_table[i].has_variable_operands = g_nd500_instrs[i].has_variable_operands;
            for (int j = 0; j < 4; j++) g_table[i].op_templates[j] = g_nd500_instrs[i].op_templates[j];
        }
    }
    if (!g_meta_index_built) build_meta_index();

    const InstrMeta* m = g_meta_index[opcode];
    return m ? m : &g_fallback_unknown;
}

const char* nd500_instr_mnemonic(uint16_t opcode) {
	return lookup(opcode)->mnemonic;
}

int nd500_instr_opcode_length(uint16_t opcode) {
    /* Determine opcode byte length:
     * - Opcodes 0x00xx (0-255) are 1-byte in memory (just the low byte)
     * - Opcodes 0xFCxx-0xFFxx are 2-byte in memory
     */
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
static uint32_t compute_effective_address(Nd500Cpu* cpu, Nd500OperandDecoded* op, Nd500DataType dtype);
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
    /* Operand data bytes are part of the instruction stream - use program memory access */
    if (m->cpu) {
        for (uint8_t i = 0; i < len; ++i) {
            out[i] = mmu_read8(m->cpu, base + i, 0, 1); /* is_write=0, is_instruction=1 */
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

	/* Zeroing all of *out per decode was 55.96% of the emulator's entire host
	 * instruction count in a callgrind profile of an NDIX boot: 1.42 million
	 * decodes x 7392 bytes. 97.7% of the struct is operands[258], sized for
	 * CALL's worst case, while ordinary instructions have at most three
	 * operands - so nearly all of that zeroing was of slots nothing would read.
	 *
	 * Clear the fixed fields on either side of the operand array here, and only
	 * the operand slots this opcode actually uses further down, once
	 * operand_count is known. Slots appended later by the variable-operand path
	 * are whole-struct assignments (operands[n] = temp_op), so they carry no
	 * stale bytes and need no pre-clearing. */
	{
		const size_t ops_off = offsetof(Nd500FetchedInstruction, operands);
		const size_t ops_end = ops_off + sizeof(out->operands);
		memset(out, 0, ops_off);
		memset((unsigned char*)out + ops_end, 0, sizeof(*out) - ops_end);
	}
	out->address = pc;

	/* Always reset extra_operand_count - prevents stale operands from previous CALL/CALLG */
	if (m->cpu) {
		m->cpu->extra_operand_count = 0;
	}

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
    /* Determine opcode length:
     * 0x00-0xFB: 1-byte opcodes (stored as single byte, internally mapped to 0x00xx)
     * 0xFC-0xFF: 2-byte opcode prefix (next byte is low part of opcode)
     *
     * Note: The opcode table uses 16-bit values like 0x0041 for TEST,
     * but in memory this is encoded as single byte 0x41.
     * The dispatch table is indexed by the full 16-bit opcode value.
     */
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

    /* Clear the operand slots this instruction will use (see the note at the
     * top of this function). Always clear at least four, so a handler that
     * reaches for a fixed slot without consulting operand_count still sees
     * zeros rather than the previous instruction's operand - that costs 112
     * bytes against the 7224 the unconditional memset used to write. */
    {
        size_t nclr = out->operand_count < 4u ? 4u : (size_t)out->operand_count;
        if (nclr > ND500_MAX_OPERANDS) nclr = ND500_MAX_OPERANDS;
        memset(out->operands, 0, nclr * sizeof(out->operands[0]));
    }
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

        /* Determine data type using unified algorithm (matches C# DetermineDataType)
         * The prefixes_mask tells us which data types are supported (BI|BY|H|W|F|D),
         * and the variant tells us which one to use (variant % type_count).
         * This unified algorithm handles ALL instruction classes correctly.
         */
        bool uses_float = false;
        out->data_type = determine_datatype_from_prefixes(
            instr_meta->prefixes_mask,
            instr_meta->variant,
            &uses_float
        );
        out->uses_float_registers = uses_float;
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
        /* Determine size from template bits or variant */ \
        /* If exactly ONE size bit is set in template, use that fixed size */ \
        /* If multiple or zero size bits, use variant for size (variable-sized operand) */ \
        uint8_t _size_bits = (_tmpl & 0x1E); /* O_BS=0x02, O_HS=0x04, O_WS=0x08, O_DS=0x10 */ \
        int _popcount = ((_size_bits & 0x02) ? 1 : 0) + ((_size_bits & 0x04) ? 1 : 0) + \
                        ((_size_bits & 0x08) ? 1 : 0) + ((_size_bits & 0x10) ? 1 : 0); \
        if (_popcount == 1) { \
            /* Exactly one size bit - use that fixed size (call/init/entm/entf operand 0) */ \
            if (_size_bits & 0x10) _disp_len = 8;      /* O_DS - double */ \
            else if (_size_bits & 0x08) _disp_len = 4; /* O_WS - word */ \
            else if (_size_bits & 0x04) _disp_len = 2; /* O_HS - halfword */ \
            else _disp_len = 1;                        /* O_BS - byte */ \
        } else { \
            /* Multiple or zero size bits - use variant: 0=byte, 1=half, 2+=word, 4=double */ \
            uint8_t _variant = nd500_instr_variant(opcode); \
            if (_variant == 0) _disp_len = 1; \
            else if (_variant == 1) _disp_len = 2; \
            else if (_variant == 4) _disp_len = 8; \
            else _disp_len = 4; \
        } \
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

    /* Check if this is a variable operand instruction (CALL, CALLG, POLY) using metadata */
    int is_var_op_instr = lookup(opcode)->has_variable_operands;
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

        /* For variable operand instructions, extract arg count from operand 1 */
        if (is_var_op_instr && i == 1) {
            Nd500OperandDecoded *arg_count_op = &out->operands[1];
            if (arg_count_op->mode == ND500_ADDR_CONSTANT_SHORT) {
                /* Short constant: value is in address_code lower 6 bits */
                arg_count = arg_count_op->address_code & 0x3F;
            } else if (arg_count_op->mode == ND500_ADDR_CONSTANT && arg_count_op->data_len == 1) {
                /* Extended constant byte */
                arg_count = arg_count_op->data[0];
            }
            /* arg_count can be 0-255, no truncation needed */
        }
    }

    /* For variable operand instructions (CALL/CALLG/POLY): decode additional argument operands */
    /* Store in BOTH fi->operands (for disassembly) and cpu->extra_operands (for execution) */
    if (is_var_op_instr) {
        /* POLY (0xFCE0-0xFCE7): operand 1 is the polynomial DEGREE m, and
         * m+1 coefficients follow (c(m)..c(0)). CALL/CALLG: operand 1 is
         * the argument count itself. */
        uint16_t extra_count = arg_count;
        if (opcode >= 0xFCE0 && opcode <= 0xFCE7) {
            extra_count = (uint16_t)arg_count + 1;
        }
        if (m->cpu) {
            m->cpu->extra_operand_count = 0;
        }
        for (uint16_t i = 0; i < extra_count && i < ND500_MAX_OPERANDS; ++i) {
            /* ALWAYS decode operand to advance cursor (required for correct instruction length) */
            /* This fixes a bug where CALL with >16 operands would have wrong return address */
            Nd500OperandDecoded temp_op;
            DECODE_GENERAL_OPERAND(&temp_op, &cursor, &byte_idx);

            /* Store in fi->operands for disassembly (if space available) */
            if (out->operand_count < ND500_MAX_OPERANDS) {
                out->operands[out->operand_count] = temp_op;
                out->operand_count++;
            }
            /* Also store in cpu->extra_operands for execution */
            if (m->cpu && m->cpu->extra_operand_count < ND500_MAX_OPERANDS) {
                m->cpu->extra_operands[m->cpu->extra_operand_count] = temp_op;
                m->cpu->extra_operand_count++;
            }
        }
    }

    #undef DECODE_GENERAL_OPERAND
    #undef DECODE_DIRECT_OPERAND

    /* === Compute effective addresses for all operands === */
    /* This computes final memory addresses where operand data resides */
    /* Must be done AFTER all operands are decoded and requires CPU register state */
    /* data_type is used for post-index scaling in PI modes */
    if (m->cpu) {
        for (uint8_t i = 0; i < out->operand_count && i < ND500_MAX_OPERANDS; ++i) {
            out->operands[i].effective_address = compute_effective_address(m->cpu, &out->operands[i], out->data_type);
        }
        /* Compute effective addresses for extra operands (CALL/CALLG/POLY arguments) */
        for (uint16_t i = 0; i < m->cpu->extra_operand_count && i < ND500_MAX_OPERANDS; ++i) {
            m->cpu->extra_operands[i].effective_address =
                compute_effective_address(m->cpu, &m->cpu->extra_operands[i], out->data_type);
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

/**
 * compute_effective_address - Calculate memory address for an operand
 *
 * This function computes the effective address for LOCAL, RECORD, PREINDEXED,
 * and ABSOLUTE addressing modes according to the ND-500 architecture.
 *
 * CRITICAL BUG FIX (2024-12-12):
 * ==============================
 * Displacements in LOCAL, RECORD, and PREINDEXED modes are UNSIGNED.
 * Previously this code incorrectly treated them as signed, causing addresses
 * like B.172 (encoded as 0xAC) to be interpreted as B.-84, resulting in
 * invalid memory accesses that underflowed segment boundaries.
 *
 * Reference: ND-05.009.4 Section 8.4 "Local addressing"
 * Quote: "Displacement values are treated as unsigned."
 *
 * This applies to:
 *   - LOCAL (0xC1-0xC3): B + unsigned_displacement
 *   - LOCAL_PI (0xD4-0xDF): B + unsigned_displacement + (I[n] * scale)
 *   - LOCAL_IND (0xC5-0xC7): @(B + unsigned_displacement)
 *   - LOCAL_IND_PI (0xE4-0xEF): @(B + unsigned_displacement) + (I[n] * scale)
 *   - RECORD (0xC9-0xCB): R + unsigned_displacement
 *   - PREINDEXED (0xF4-0xFF): I[n] + unsigned_displacement
 *
 * NOTE: Branch displacements (GO, IF*GO, LOOP*) ARE signed and are handled
 * separately in nd500_get_operand_displacement() in nd500_disasm.c.
 *
 * @param cpu   CPU state containing register values (B, R, I[1-4])
 * @param op    Decoded operand with addressing mode and displacement data
 * @param dtype Data type for post-index scaling (BYTE=1, HALF=2, WORD=4, DOUBLE=8)
 * @return      Computed effective address
 */
static uint32_t compute_effective_address(Nd500Cpu* cpu, Nd500OperandDecoded* op, Nd500DataType dtype) {
    uint32_t address = 0;
    uint32_t displacement = 0;

    /* Initialize bit_position to 0 (LSB) for non-indexed BIT addressing */
    op->bit_position = 0;

    /*
     * Extract displacement value (UNSIGNED, big-endian)
     * Per ND-05.009.4 Section 8.4: "Displacement values are treated as unsigned."
     */
    if (op->data_len == 1) {
        displacement = (uint8_t)op->data[0];
    } else if (op->data_len == 2) {
        displacement = ((uint32_t)op->data[0] << 8) | (uint32_t)op->data[1];  /* Big-endian */
    } else if (op->data_len >= 4) {
        displacement = get_operand_value32(op);
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
            /* Local addressing - B register + displacement (unsigned) */
            address = cpu->B + displacement;
            break;
            
        case ND500_ADDR_LOCAL_SHORT:
            /* Local short - B + embedded value * 4 */
            address = cpu->B + (get_short_embedded(op) * 4u);
            break;
            
        case ND500_ADDR_RECORD:
            /* Record addressing - R register + displacement (unsigned) */
            address = cpu->R + displacement;
            break;

        case ND500_ADDR_RECORD_SHORT:
            /* Record short - R + embedded value * 4 */
            address = cpu->R + (get_short_embedded(op) * 4u);
            break;

        case ND500_ADDR_PREINDEXED:
            /* Pre-indexed - I[n] + displacement (unsigned) */
            /* op->reg is 1-4, cpu->I[] is 0-indexed (I[0]=I1, I[1]=I2, etc.) */
            if (op->reg >= 1 && op->reg <= 4) {
                address = cpu->I[op->reg - 1] + displacement;
            } else {
                address = displacement;
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
    
    /* STEP 3: Handle post-indexing (b.xxx(rN), IND(b.xxx)(rN), $xxx(rN)) */
    /* Add scaled index register AFTER base+displacement (and after indirection) */
    /* CRITICAL: Index register value is multiplied by data type size (scale factor) */
    /* Note: For post-indexed modes, reg is derived from address_code bits 0-1 */
    switch (op->mode) {
        case ND500_ADDR_LOCAL_PI:
        case ND500_ADDR_LOCAL_IND_PI:
        case ND500_ADDR_ABSOLUTE_PI: {
            /* Post-indexed - add I[reg] * scale */
            /* reg from address_code: bits 0-1 give 0-3, maps to I1-I4 */
            uint8_t pi_reg = op->address_code & 0x03;
            int32_t index_value = (int32_t)cpu->I[pi_reg];
            
            /* Determine scale factor based on data type */
            int scale;
            switch (dtype) {
                case ND500_DTYPE_BIT: {
                    /* BIT addressing: index is bit offset, not byte offset
                     * Per ND-500 Reference Manual page 133-134:
                     * "Post indexing always counts the data elements from the left"
                     * bn = 7 - REM(index/8) = 7 - (index % 8)
                     *
                     * Effective address = base + (bit_index / 8)
                     * Bit position = 7 - (bit_index % 8), counting from MSB
                     */
                    op->bit_position = 7 - (uint8_t)(index_value & 0x07);  /* bit position, counted from left */
                    address = (uint32_t)((int32_t)address + (index_value >> 3));  /* byte offset = bit_index / 8 */
                    return address;  /* Early return - no additional scaling */
                }
                case ND500_DTYPE_BYTE:       scale = 1; break;
                case ND500_DTYPE_HALFWORD:   scale = 2; break;
                case ND500_DTYPE_WORD:       scale = 4; break;
                case ND500_DTYPE_DOUBLEWORD: scale = 8; break;
                default:                     scale = 1; break;
            }

            address = (uint32_t)((int32_t)address + scale * index_value);
            break;
        }
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
            /* op->reg is 1-4, cpu->I[] is 0-indexed */
            if (op->reg >= 1 && op->reg <= 4) return cpu->I[op->reg - 1];
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
            /* op->reg is 1-4, cpu->I[] is 0-indexed */
            if (op->reg >= 1 && op->reg <= 4) cpu->I[op->reg - 1] = value;
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


