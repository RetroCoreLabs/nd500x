#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "machine_protos.h"
#include "../cpu/cpu_protos.h"
#include "../ndlib/ndlib.h"
#include "../ndlib/ndlib_color.h"

#include "../cpu/cpu_protos.h"

size_t nd500_dbg_mem_dump(Nd500Machine* m, uint32_t addr, uint32_t len, uint8_t* out, size_t out_cap) {
	if (!m || !out || out_cap == 0) return 0;
	if (addr >= m->memory_size) return 0;
	uint32_t max = (uint32_t)((addr + len) > m->memory_size ? (m->memory_size - addr) : len);
	if (max > out_cap) max = (uint32_t)out_cap;
	memcpy(out, m->memory + addr, max);
	return max;
}

static int g_show_ea = -1;       /* -1: uninitialized, 0: off, 1: on */
static int g_demangle = -1;      /* demangle C-style symbols (strip leading _) */
static int g_trace_mode = -1;   /* instruction trace mode */
static int g_profiling = -1;    /* instruction profiling mode */
static int g_trap_invalid = -1; /* trap on invalid instruction 0x00 */

/* Profiling data structures */
#define MAX_PROFILE_ENTRIES 256
typedef struct {
    char mnemonic[16];
    uint32_t count;
    uint32_t total_cycles;  /* Placeholder for cycle counting */
} ProfileEntry;

static ProfileEntry g_profile_entries[MAX_PROFILE_ENTRIES];
static int g_profile_count = 0;
static uint32_t g_total_instructions = 0;

/* Call stack tracking */
#define MAX_CALL_STACK 64
typedef struct {
    uint32_t pc;           /* Address where call was made */
    uint32_t return_addr;  /* Return address */
    const char* symbol;    /* Function symbol name */
} CallStackEntry;

static CallStackEntry g_call_stack[MAX_CALL_STACK];
static int g_call_stack_depth = 0;

int nd500_dbg_set_show_ea(int onoff) {
    g_show_ea = onoff ? 1 : 0;
    return g_show_ea;
}

int nd500_dbg_get_show_ea(void) {
    if (g_show_ea < 0) {
        const char* env = getenv("ND500X_SHOW_EA");
        g_show_ea = (env && *env == '1') ? 1 : 0;
    }
    return g_show_ea;
}

static const char* maybe_demangle(const char* sym) {
    if (!sym) return NULL;
    if (g_demangle < 0) {
        const char* env = getenv("ND500X_DEMANGLE");
        g_demangle = (env && *env == '1') ? 1 : 0;
    }
    if (!g_demangle) return sym;
    /* Simple C demangle: strip single leading underscore */
    if (sym[0] == '_' && sym[1] != '\0') return sym + 1;
    return sym;
}

int nd500_dbg_set_demangle(int onoff) {
    g_demangle = onoff ? 1 : 0;
    return g_demangle;
}

int nd500_dbg_get_demangle(void) {
    if (g_demangle < 0) {
        const char* env = getenv("ND500X_DEMANGLE");
        g_demangle = (env && *env == '1') ? 1 : 0;
    }
    return g_demangle;
}

void nd500_dbg_disasm_print(Nd500Machine* m, uint32_t addr, uint32_t len) {
    /* Print each instruction immediately - no buffer needed */
	if (!m) return;
    uint32_t end_addr = addr + len;
    /* Clamp to actual text loaded if available */
    if (m->memory_size > 0 && end_addr > m->memory_size) end_addr = m->memory_size;
    size_t (*format_operand)(char*, size_t, const Nd500OperandDecoded*) = NULL;
    size_t format_impl(char* dst, size_t cap, const Nd500OperandDecoded* op) {
        if (!dst || cap == 0) return (size_t)0;
        uint32_t val = 0;
        int32_t sval = 0;
        if (op->data_len == 1) { val = op->data[0]; sval = (int8_t)op->data[0]; }
        else if (op->data_len == 2) { 
            val = (uint32_t)op->data[0] | ((uint32_t)op->data[1] << 8);
            sval = (int16_t)val;
        }
        else if (op->data_len >= 4) {
            val = (uint32_t)op->data[0] | ((uint32_t)op->data[1] << 8) | ((uint32_t)op->data[2] << 16) | ((uint32_t)op->data[3] << 24);
            sval = (int32_t)val;
        }
        uint8_t low6 = op->address_code & 0x3F;
        char* p = dst; char* e = dst + cap;
        if (op->has_alt_prefix && p < e) { int n = snprintf(p, (size_t)(e-p), "ALT "); if (n>0) p+= (n < (e-p) ? n : (int)(e-p)); }
        if (op->has_desc_prefix && p < e) { int n = snprintf(p, (size_t)(e-p), "DESC%d ", (int)op->reg+1); if (n>0) p+= (n < (e-p) ? n : (int)(e-p)); }
        
        /* Special handling for inline operands (AC=0xFF for branch disp, AC=0xFE for call nargs) */
        if (op->address_code == 0xFE || op->address_code == 0xFF) {
            /* Branch displacements (0xFF) should be signed, call args (0xFE) are unsigned */
            if (op->address_code == 0xFF && op->data_len <= 2) {
                /* Signed displacement for branches */
                int n = snprintf(p, (size_t)(e-p), "$%d", sval);
                p += (n>0 && n < (e-p)? n : (e-p));
            } else {
                /* Unsigned for everything else */
                int n = snprintf(p, (size_t)(e-p), "$%u", (unsigned)val);
                p += (n>0 && n < (e-p)? n : (e-p));
            }
            if (p < e) *p = '\0';
            return (size_t)(p - dst);
        }
        
        switch (op->mode) {
            case ND500_ADDR_CONSTANT_SHORT: { int n = snprintf(p, (size_t)(e-p), "$%u", (unsigned)low6); p += (n>0 && n < (e-p)? n : (e-p)); break; }
            case ND500_ADDR_LOCAL_SHORT: { int n = snprintf(p, (size_t)(e-p), "b.%u", (unsigned)(low6*4u)); p += (n>0 && n < (e-p)? n : (e-p)); break; }
            case ND500_ADDR_RECORD_SHORT: { int n = snprintf(p, (size_t)(e-p), "r.%u", (unsigned)(low6*4u)); p += (n>0 && n < (e-p)? n : (e-p)); break; }
            case ND500_ADDR_REGISTER: { int n = snprintf(p, (size_t)(e-p), "r%d", (int)op->reg+1); p += (n>0 && n < (e-p)? n : (e-p)); break; }
            case ND500_ADDR_ABSOLUTE: { int n = snprintf(p, (size_t)(e-p), "$0x%08X", val); p += (n>0 && n < (e-p)? n : (e-p)); break; }
            case ND500_ADDR_LOCAL: { int n = snprintf(p, (size_t)(e-p), "b.%d", sval); p += (n>0 && n < (e-p)? n : (e-p)); break; }
            case ND500_ADDR_RECORD: { int n = snprintf(p, (size_t)(e-p), "r.%d", sval); p += (n>0 && n < (e-p)? n : (e-p)); break; }
            case ND500_ADDR_PREINDEXED: { int n = snprintf(p, (size_t)(e-p), "r%d.(%d)", (int)op->reg+1, sval); p += (n>0 && n < (e-p)? n : (e-p)); break; }
            case ND500_ADDR_LOCAL_IND: { int n = snprintf(p, (size_t)(e-p), "@b.%u", (unsigned)val); p += (n>0 && n < (e-p)? n : (e-p)); break; }
            case ND500_ADDR_LOCAL_PI: { int n = snprintf(p, (size_t)(e-p), "b.%u+", (unsigned)val); p += (n>0 && n < (e-p)? n : (e-p)); break; }
            case ND500_ADDR_LOCAL_IND_PI: { int n = snprintf(p, (size_t)(e-p), "@b.%u+", (unsigned)val); p += (n>0 && n < (e-p)? n : (e-p)); break; }
            case ND500_ADDR_ABSOLUTE_PI: { int n = snprintf(p, (size_t)(e-p), "$0x%08X+", val); p += (n>0 && n < (e-p)? n : (e-p)); break; }
            case ND500_ADDR_CONSTANT: {
                if (op->data_len == 8) { int n = snprintf(p, (size_t)(e-p), "$<double>"); p += (n>0 && n < (e-p)? n : (e-p)); }
                else { int n = snprintf(p, (size_t)(e-p), "$%u", (unsigned)val); p += (n>0 && n < (e-p)? n : (e-p)); }
                break; }
            case ND500_ADDR_DESCRIPTOR: { int n = snprintf(p, (size_t)(e-p), "DESC(r%d)", (int)op->reg+1); p += (n>0 && n < (e-p)? n : (e-p)); break; }
            case ND500_ADDR_ALTERNATIVE:
            case ND500_ADDR_UNKNOWN:
            default: { int n = snprintf(p, (size_t)(e-p), "<ac=0x%02X>", op->address_code); p += (n>0 && n < (e-p)? n : (e-p)); break; }
        }
        if (p < e) *p = '\0';
        return (size_t)(p - dst);
    }
    format_operand = &format_impl;

    const char* map_mnemonic_symbol(const char* mnem) {
        if (!mnem) return "???";
        /* Don't map "move" - it's used for non-R_N variants; ":=" already correct */
        if (strcmp(mnem, "add") == 0) return "+";
        if (strcmp(mnem, "sub") == 0) return "-";
        if (strcmp(mnem, "mul") == 0) return "*";
        if (strcmp(mnem, "div") == 0) return "/";
        if (strcmp(mnem, "and") == 0) return "&";
        if (strcmp(mnem, "or") == 0) return "|";
        if (strcmp(mnem, "xor") == 0) return "^";
        if (strcmp(mnem, "comp") == 0) return "comp";
        if (strcmp(mnem, "neg") == 0) return "neg";
        if (strcmp(mnem, "not") == 0) return "not";
        return mnem;
    }

    for (uint32_t a = addr; a < end_addr;) {
        Nd500FetchedInstruction fi;
        if (nd500_decode_at(m, a, &fi) != 0) break;
        
        /* Check if there's a symbol at this address - show it on its own line first */
        const char* sym_at_addr = ndlib_symbols_name_for_addr(fi.address);
        sym_at_addr = maybe_demangle(sym_at_addr);
        if (sym_at_addr && *sym_at_addr) {
            printf("%s%08X:%s                                   %s%s:%s\n", 
                   color_address(), fi.address, color_reset(),
                   color_label(), sym_at_addr, color_reset());
        }
        
        /* If opcode is 0 or unknown, show ??? */
        if (fi.opcode == 0 || !fi.mnemonic || strcmp(fi.mnemonic, "???") == 0) {
            /* Show hex bytes for unknown opcodes too */
            printf("%s%08X:%s ", color_address(), a, color_reset());
            uint32_t unk_len = fi.total_len > 0 ? fi.total_len : 1;
            printf("%s", color_bytes());
            for (uint32_t b = 0; b < unk_len && b < 8; b++) {
                printf("%02X ", fi.bytes[b]);
            }
            printf("%s%-*s %s???%s %s; opcode 0x%04X%s\n", 
                   color_reset(), (int)(24 - unk_len * 3), "", 
                   color_instr(), color_reset(),
                   color_comment(), fi.opcode, color_reset());
            a += unk_len;
            continue;
        }
        const char* sy = NULL;  /* Don't show symbol inline anymore, already shown above */
        int n = 0;
        /* Build prefix: for R_N show "wN ", for non-R_N with dtype show "w " */
        char regprefix[16] = {0};
        if (nd500_instr_has_rn(fi.opcode)) {
            int dreg = nd500_instr_dest_reg(fi.opcode);
            if (dreg >= 0 && dreg < 4) {
                const char* dts = nd500_instr_dtype_prefix(fi.opcode);
                snprintf(regprefix, sizeof(regprefix), "%s%d ", dts, dreg + 1);
            }
        } else {
            /* For non-R_N instructions with dtype prefixes (excluding R_N bit), show just dtype */
            uint8_t dtype_mask = nd500_instr_prefixes_mask(fi.opcode) & 0x3F; /* Exclude R_N (0x40) */
            if (dtype_mask != 0) {
                const char* dts = nd500_instr_dtype_prefix(fi.opcode);
                snprintf(regprefix, sizeof(regprefix), "%s ", dts);
            }
        }
        const char* disp_mn = map_mnemonic_symbol(fi.mnemonic);
        char full_mn[32];
        snprintf(full_mn, sizeof(full_mn), "%s%s", regprefix, disp_mn);
        
        /* Determine instruction color based on opcode type */
        const char* instr_color = color_instr();
        if (nd500_instr_is_branch(fi.opcode)) {
            instr_color = color_branch();
        }
        
        /* Print address in gray */
        printf("%s%08X:%s ", color_address(), fi.address, color_reset());
        
        /* Print hex bytes in yellow */
        printf("%s", color_bytes());
        for (uint32_t b = 0; b < fi.total_len && b < 16; b++) {
            printf("%02X ", fi.bytes[b]);
        }
        printf("%s%-*s", color_reset(), (int)(24 - fi.total_len * 3), "");
        
        /* Print symbol in cyan and mnemonic with appropriate color */
        if (sy && *sy) {
            printf("%s<%s>%s ", color_label(), sy, color_reset());
        }
        if (fi.operand_count > 0) {
            printf("%s%-12s%s ", instr_color, full_mn, color_reset());
        } else {
            printf("%s%s%s", instr_color, full_mn, color_reset());
        }
        
        /* Append operands in white */
        printf("%s", color_oper());
        for (uint8_t oi = 0; oi < fi.operand_count; ++oi) {
            char obuf[64];
            size_t ol = format_operand(obuf, sizeof(obuf), &fi.operands[oi]);
            if (ol > 0) {
                printf("%s%s", (oi > 0) ? "," : "", obuf);
                /* EA breakdown when enabled and EA present */
                if (nd500_dbg_get_show_ea() && fi.operands[oi].effective_address != 0) {
                    /* Decode base for human-readable breakdown */
                    const char* base = NULL; char basebuf[8]; basebuf[0] = '\0';
                    switch (fi.operands[oi].mode) {
                        case ND500_ADDR_LOCAL:
                        case ND500_ADDR_LOCAL_PI:
                        case ND500_ADDR_LOCAL_IND:
                        case ND500_ADDR_LOCAL_IND_PI:
                        case ND500_ADDR_LOCAL_SHORT:
                            base = "B"; break;
                        case ND500_ADDR_RECORD:
                        case ND500_ADDR_RECORD_SHORT:
                            base = "R"; break;
                        case ND500_ADDR_PREINDEXED:
                            snprintf(basebuf, sizeof(basebuf), "I%d", (int)fi.operands[oi].reg+1); base = basebuf; break;
                        case ND500_ADDR_ABSOLUTE:
                        case ND500_ADDR_ABSOLUTE_PI:
                            base = "$"; break;
                        default:
                            base = NULL; break;
                    }
                    int32_t disp = 0;
                    if (fi.operands[oi].data_len == 1) disp = (int8_t)fi.operands[oi].data[0];
                    else if (fi.operands[oi].data_len == 2) disp = (int16_t)((uint16_t)fi.operands[oi].data[0] | ((uint16_t)fi.operands[oi].data[1] << 8));
                    else if (fi.operands[oi].data_len >= 4) disp = (int32_t)((uint32_t)fi.operands[oi].data[0] | ((uint32_t)fi.operands[oi].data[1] << 8) | ((uint32_t)fi.operands[oi].data[2] << 16) | ((uint32_t)fi.operands[oi].data[3] << 24));
                    printf(" %s[", color_comment());
                    if (base) printf("%s", base);
                    if (disp != 0) printf("%+d", disp);
                    printf("]→0x%08X%s", fi.operands[oi].effective_address, color_reset());
                }
            }
        }
        printf("%s", color_reset());
        
        /* === Check for relocations/unresolved externals in this instruction === */
        uint8_t is_undefined = 0;
        const char* reloc_symbol = ndlib_symbols_reloc_for_range(fi.address, fi.address + fi.total_len, &is_undefined);
        
        if (reloc_symbol && *reloc_symbol) {
            reloc_symbol = maybe_demangle(reloc_symbol);
            /* Show relocation comment */
            if (is_undefined) {
                /* Unresolved in red note */
                printf(" %s; %s %s(UNRESOLVED)%s", color_comment(), reloc_symbol, color_branch(), color_reset());
            } else {
                /* Resolved relocation in meta color */
                printf(" %s; -> %s%s", color_meta(), reloc_symbol, color_reset());
            }
        } else if (nd500_instr_is_branch(fi.opcode) && fi.operand_count > 0) {
            /* Show target symbols for branch/call instructions using effective addresses */
            uint32_t target = 0;
            int found_target = 0;
            
            /* For direct operands (branches/calls), calculate the target address */
            if (fi.operands[0].address_code == 0xFF || fi.operands[0].address_code == 0xFE) {
                int is_pc_relative = (fi.operand_count == 1);  /* Single operand = PC-relative branch */
                
                if (is_pc_relative) {
                    /* PC-relative branch: target = PC + length + displacement */
                    int32_t displacement = 0;
                    if (fi.operands[0].data_len == 1) {
                        displacement = (int8_t)fi.operands[0].data[0];
                    } else if (fi.operands[0].data_len == 2) {
                        uint16_t raw = (uint16_t)fi.operands[0].data[0] | ((uint16_t)fi.operands[0].data[1] << 8);
                        displacement = (int16_t)raw;
                    } else if (fi.operands[0].data_len == 4) {
                        uint32_t raw = (uint32_t)fi.operands[0].data[0] | ((uint32_t)fi.operands[0].data[1] << 8) |
                                       ((uint32_t)fi.operands[0].data[2] << 16) | ((uint32_t)fi.operands[0].data[3] << 24);
                        displacement = (int32_t)raw;
                    }
                    target = (uint32_t)((int32_t)fi.address + (int32_t)fi.total_len + displacement);
                    found_target = 1;
                } else {
                    /* Absolute call: target is first operand value */
                    if (fi.operands[0].data_len >= 4) {
                        target = (uint32_t)fi.operands[0].data[0] | ((uint32_t)fi.operands[0].data[1] << 8) |
                                 ((uint32_t)fi.operands[0].data[2] << 16) | ((uint32_t)fi.operands[0].data[3] << 24);
                        found_target = 1;
                    }
                }
            } else {
                /* For indirect calls/branches, use pre-computed effective_address */
                /* e.g., "call r1.0", "go b.4" */
                target = fi.operands[0].effective_address;
                found_target = 1;
            }
            
            /* Look up symbol for target address */
            if (found_target) {
                const char* target_sym = ndlib_symbols_name_for_addr(target);
                target_sym = maybe_demangle(target_sym);
                if (target_sym && *target_sym) {
                    /* Show an explicit jump/call arrow to the symbol */
                    printf(" %s-> <%s>%s", color_meta(), target_sym, color_reset());
                }
            }
        }
        printf("\n");
        a += (uint32_t)fi.total_len;
    }
}

size_t nd500_dbg_disasm(Nd500Machine* m, uint32_t addr, uint32_t len, char* out, size_t out_cap) {
    /* Legacy buffer-based API - just call print version and return dummy */
    nd500_dbg_disasm_print(m, addr, len);
    if (out && out_cap > 0) out[0] = '\0';
    return 0;
}

void nd500_dbg_step(Nd500Machine* m, uint32_t count) {
	if (!m || !m->cpu) return;
	for (uint32_t i = 0; i < count; ++i) {
		nd500_cpu_step(m->cpu);
	}
}

#ifndef __unix__
void nd500_dbg_run(Nd500Machine* m) {
	if (!m) return;
	m->run_flag = 1;
}

void nd500_dbg_stop(Nd500Machine* m) {
	if (!m) return;
	m->run_flag = 0;
}
#endif

int nd500_dbg_is_running(Nd500Machine* m) {
	return m ? m->run_flag : 0;
}

/* Optional: expose regs snapshot through machine */
void nd500_dbg_regs(struct Nd500Cpu* cpu, Nd500Regs* out_regs) {
	nd500_cpu_get_regs(cpu, out_regs);
}

int nd500_dbg_load_aout_file(Nd500Machine* m, const char* path, uint32_t* out_entry_pc) {
	/* TODO: integrate libsymbols; placeholder just zero entry */
	if (out_entry_pc) *out_entry_pc = 0;
	(void)m; (void)path;
	return 0;
}

int nd500_dbg_load_aout_buffer(Nd500Machine* m, const uint8_t* data, size_t size, uint32_t* out_entry_pc) {
	/* TODO: integrate libsymbols; placeholder copy to base */
	if (!m || !data || size == 0) return -1;
	if (size > m->memory_size) size = m->memory_size;
	memcpy(m->memory, data, size);
	if (out_entry_pc) *out_entry_pc = 0;
	return 0;
}

/* Trace mode functions */
int nd500_dbg_set_trace_mode(int onoff) {
    g_trace_mode = onoff ? 1 : 0;
    return g_trace_mode;
}

int nd500_dbg_get_trace_mode(void) {
    if (g_trace_mode < 0) {
        const char* env = getenv("ND500X_TRACE");
        g_trace_mode = (env && strcmp(env, "1") == 0) ? 1 : 0;
    }
    return g_trace_mode;
}

void nd500_dbg_trace_instruction(uint32_t pc, const char* mnemonic, uint32_t* registers) {
    if (!nd500_dbg_get_trace_mode()) return;
    
    printf("%s[TRACE]%s PC=0x%08X %s", 
           color_meta(), color_reset(), pc, mnemonic ? mnemonic : "unknown");
    
    if (registers) {
        printf(" I1=0x%08X I2=0x%08X I3=0x%08X I4=0x%08X", 
               registers[1], registers[2], registers[3], registers[4]);
    }
    printf("\n");
}

/* Profiling functions */
int nd500_dbg_set_profiling(int onoff) {
    g_profiling = onoff ? 1 : 0;
    return g_profiling;
}

int nd500_dbg_get_profiling(void) {
    if (g_profiling < 0) {
        const char* env = getenv("ND500X_PROFILE");
        g_profiling = (env && strcmp(env, "1") == 0) ? 1 : 0;
    }
    return g_profiling;
}

void nd500_dbg_profile_instruction(const char* mnemonic) {
    if (!nd500_dbg_get_profiling() || !mnemonic) return;
    
    g_total_instructions++;
    
    /* Find existing entry or create new one */
    for (int i = 0; i < g_profile_count; i++) {
        if (strcmp(g_profile_entries[i].mnemonic, mnemonic) == 0) {
            g_profile_entries[i].count++;
            return;
        }
    }
    
    /* Add new entry if we have space */
    if (g_profile_count < MAX_PROFILE_ENTRIES) {
        strncpy(g_profile_entries[g_profile_count].mnemonic, mnemonic, sizeof(g_profile_entries[g_profile_count].mnemonic) - 1);
        g_profile_entries[g_profile_count].mnemonic[sizeof(g_profile_entries[g_profile_count].mnemonic) - 1] = '\0';
        g_profile_entries[g_profile_count].count = 1;
        g_profile_entries[g_profile_count].total_cycles = 1; /* Placeholder */
        g_profile_count++;
    }
}

void nd500_dbg_show_profile(void) {
    if (g_total_instructions == 0) {
        printf("No profiling data available\n");
        return;
    }
    
    printf("\n=== INSTRUCTION PROFILE ===\n");
    printf("Total instructions executed: %u\n", g_total_instructions);
    printf("\nInstruction frequency:\n");
    printf("%-12s %8s %8s\n", "Mnemonic", "Count", "Percent");
    printf("%-12s %8s %8s\n", "---------", "-----", "-------");
    
    for (int i = 0; i < g_profile_count; i++) {
        float percent = (float)g_profile_entries[i].count * 100.0f / g_total_instructions;
        printf("%-12s %8u %7.1f%%\n", 
               g_profile_entries[i].mnemonic, 
               g_profile_entries[i].count, 
               percent);
    }
    printf("\n");
}

void nd500_dbg_reset_profile(void) {
    g_profile_count = 0;
    g_total_instructions = 0;
    memset(g_profile_entries, 0, sizeof(g_profile_entries));
    printf("Profiling data reset\n");
}

/* Call stack functions */
void nd500_dbg_call_stack_push(uint32_t pc, uint32_t return_addr) {
    if (g_call_stack_depth >= MAX_CALL_STACK) {
        printf("Warning: Call stack overflow (max %d levels)\n", MAX_CALL_STACK);
        return;
    }
    
    g_call_stack[g_call_stack_depth].pc = pc;
    g_call_stack[g_call_stack_depth].return_addr = return_addr;
    g_call_stack[g_call_stack_depth].symbol = ndlib_symbols_name_for_addr(pc);
    g_call_stack_depth++;
}

void nd500_dbg_call_stack_pop(void) {
    if (g_call_stack_depth > 0) {
        g_call_stack_depth--;
    }
}

void nd500_dbg_show_backtrace(void) {
    if (g_call_stack_depth == 0) {
        printf("Call stack is empty\n");
        return;
    }
    
    printf("\n=== CALL STACK ===\n");
    printf("Depth  PC        Return   Symbol\n");
    printf("-----  --------  -------- ------\n");
    
    for (int i = g_call_stack_depth - 1; i >= 0; i--) {
        const char* symbol = g_call_stack[i].symbol;
        if (!symbol || !*symbol) symbol = "unknown";
        
        printf("%5d  0x%08X 0x%08X %s\n", 
               g_call_stack_depth - i,
               g_call_stack[i].pc,
               g_call_stack[i].return_addr,
               symbol);
    }
    printf("\n");
}

void nd500_dbg_call_stack_reset(void) {
    g_call_stack_depth = 0;
    memset(g_call_stack, 0, sizeof(g_call_stack));
    printf("Call stack reset\n");
}

/* Invalid instruction trap functions */
int nd500_dbg_set_trap_invalid(int onoff) {
    g_trap_invalid = onoff;
    return 0;
}

int nd500_dbg_get_trap_invalid(void) {
    if (g_trap_invalid == -1) {
        /* Check environment variable */
        const char* env = getenv("ND500X_TRAP_INVALID");
        if (env && (strcmp(env, "1") == 0 || strcasecmp(env, "on") == 0 || strcasecmp(env, "true") == 0)) {
            g_trap_invalid = 1;
        } else {
            g_trap_invalid = 1; /* Default to enabled */
        }
    }
    return g_trap_invalid;
}

void nd500_dbg_toggle_trap_invalid(void) {
    int current = nd500_dbg_get_trap_invalid();
    nd500_dbg_set_trap_invalid(!current);
    printf("Invalid instruction trap: %s\n", !current ? "ON" : "OFF");
}

/* Trap state management functions */
void nd500_dbg_clear_traps(void) {
    nd500_trap_clear();
}

int nd500_dbg_trap_occurred(void) {
    return nd500_trap_occurred();
}

const char* nd500_dbg_get_trap_description(void) {
    const Nd500TrapState* trap = nd500_trap_get_state();
    if (trap && trap->trap_occurred) {
        return trap->trap_description;
    }
    return NULL;
}


