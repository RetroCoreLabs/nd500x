#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include "machine_protos.h"
#include "breakpoints.h"
#include "../disasm/nd500_disasm.h"
#include "../cpu/cpu_protos.h"
#include "../ndlib/ndlib.h"
#include "../ndlib/ndlib_color.h"

static const char* reg_names[] = {"r1", "r2", "r3", "r4"};

size_t nd500_dbg_mem_dump(Nd500Machine* m, uint32_t addr, uint32_t len, uint8_t* out, size_t out_cap) {
	if (!m || !out || out_cap == 0) return 0;
	if (addr >= m->memory_size) return 0;
	uint32_t max = (uint32_t)((addr + len) > m->memory_size ? (m->memory_size - addr) : len);
	if (max > out_cap) max = (uint32_t)out_cap;
	for (uint32_t i = 0; i < max; ++i) {
		out[i] = nd500_bus_read8(m, addr + i);
	}
	return max;
}

static int g_show_ea = -1;       /* -1: uninitialized, 0: off, 1: on */
static int g_show_hex = -1;      /* -1: uninitialized (default on), 0: off, 1: on */
static int g_radix = 0;          /* 0=decimal, 1=hex, 2=octal */
static int g_demangle = -1;      /* demangle C-style symbols (strip leading _) */
static int g_trace_mode = -1;   /* instruction trace mode */
static int g_profiling = -1;    /* instruction profiling mode */
static int g_trap_invalid = -1; /* trap on invalid instruction 0x00 */
static int g_show_source = 0;   /* 0: off, 1: asm only, 2: c only, 3: both */
static int g_mmu_log_level = MMU_LOG_ERRORS;  /* MMU logging: 0=off, 1=errors, 2=trace, 3=all */
static int g_memtrace_flags = 0;  /* memory access trace flags (bitmask) */

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

int nd500_dbg_set_show_hex(int onoff) {
    g_show_hex = onoff ? 1 : 0;
    return g_show_hex;
}

int nd500_dbg_get_show_hex(void) {
    if (g_show_hex < 0) {
        const char* env = getenv("ND500X_SHOW_HEX");
        /* Default to ON if not set */
        g_show_hex = (env && *env == '0') ? 0 : 1;
    }
    return g_show_hex;
}

int nd500_dbg_set_radix(int mode) {
    if (mode < 0 || mode > 2) return g_radix;
    g_radix = mode;
    return g_radix;
}

int nd500_dbg_get_radix(void) {
    return g_radix;
}

/* Get base value for strtoul: 10, 16, or 8 */
int nd500_dbg_get_radix_base(void) {
    switch (g_radix) {
        case 1: return 16;  /* hex */
        case 2: return 8;   /* octal */
        default: return 10; /* decimal */
    }
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

int nd500_dbg_set_show_source(int mode) {
    if (mode < 0 || mode > 3) return -1;
    g_show_source = mode;
    return g_show_source;
}

int nd500_dbg_get_show_source(void) {
    return g_show_source;
}

int nd500_dbg_set_mmu_log_level(int level) {
    if (level < MMU_LOG_OFF) level = MMU_LOG_OFF;
    if (level > MMU_LOG_ALL) level = MMU_LOG_ALL;
    g_mmu_log_level = level;
    return g_mmu_log_level;
}

int nd500_dbg_get_mmu_log_level(void) {
    return g_mmu_log_level;
}

/* REMOVED: duplicate format_operand_impl() and helpers (fmt_signed/fmt_unsigned).
 * Now using the authoritative nd500_format_operand() from nd500_disasm.h */

static const char* map_mnemonic_symbol(const char* mnem) {
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

void nd500_dbg_disasm_print(Nd500Machine* m, uint32_t addr, uint32_t len) {
    /* Print each instruction immediately - no buffer needed */
	if (!m) return;
    uint32_t end_addr = addr + len;
    /* Only clamp to physical memory size when NOT using MMU.
     * With MMU enabled, virtual addresses can exceed physical memory size. */
    if (!m->mmu_enabled && m->memory_size > 0 && end_addr > m->memory_size) {
        end_addr = m->memory_size;
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

        /* Check for source line mapping */
        int source_mode = nd500_dbg_get_show_source();
        if (source_mode > 0) {
            const char* c_file = NULL;
            const char* s_file = NULL;
            int c_line = 0;
            int s_line = 0;

            /* Query both C and assembly mappings */
            int has_c = ndlib_symbols_get_c_mapping(fi.address, &c_file, &c_line);
            int has_s = ndlib_symbols_get_s_mapping(fi.address, &s_file, &s_line);

            /* Display based on mode */
            if (source_mode == 3) {
                /* Both: show C first, then asm */
                if (has_c) {
                    printf("%s  %s:%d%s\n", color_comment(), c_file, c_line, color_reset());
                }
                if (has_s) {
                    printf("%s  %s:%d%s\n", color_comment(), s_file, s_line, color_reset());
                }
            } else if (source_mode == 2) {
                /* C only */
                if (has_c) {
                    printf("%s  %s:%d%s\n", color_comment(), c_file, c_line, color_reset());
                }
            } else if (source_mode == 1) {
                /* Assembly only */
                if (has_s) {
                    printf("%s  %s:%d%s\n", color_comment(), s_file, s_line, color_reset());
                }
            }
        }
        
        /* If opcode is 0 or unknown, show ??? */
        if (fi.opcode == 0 || !fi.mnemonic || strcmp(fi.mnemonic, "???") == 0) {
            /* Print breakpoint marker if present */
            if (nd500_dbg_has_breakpoint_at(m, a)) {
                printf("%s●%s ", color_branch(), color_reset());  /* Red bullet for breakpoint */
            } else {
                printf("  ");  /* Two spaces for alignment */
            }

            /* Show hex bytes for unknown opcodes (if enabled) */
            printf("%s%08X:%s ", color_address(), a, color_reset());
            uint32_t unk_len = fi.total_len > 0 ? fi.total_len : 1;
            if (nd500_dbg_get_show_hex()) {
                printf("%s", color_bytes());
                for (uint32_t b = 0; b < unk_len && b < 8; b++) {
                    printf("%02X ", fi.bytes[b]);
                }
                printf("%s%-*s", color_reset(), (int)(24 - unk_len * 3), "");
            }
            printf("%s???%s %s; opcode 0x%04X%s\n",
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

        /* Print breakpoint marker if present */
        if (nd500_dbg_has_breakpoint_at(m, fi.address)) {
            printf("%s●%s ", color_branch(), color_reset());  /* Red bullet for breakpoint */
        } else {
            printf("  ");  /* Two spaces for alignment */
        }

        /* Print address in gray */
        printf("%s%08X:%s ", color_address(), fi.address, color_reset());

        /* Print hex bytes in yellow (if enabled) */
        if (nd500_dbg_get_show_hex()) {
            printf("%s", color_bytes());
            for (uint32_t b = 0; b < fi.total_len && b < 16; b++) {
                printf("%02X ", fi.bytes[b]);
            }
            printf("%s%-*s", color_reset(), (int)(24 - fi.total_len * 3), "");
        }

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
            int ol = nd500_format_operand(obuf, sizeof(obuf), &fi.operands[oi], fi.data_type, false);
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
                            snprintf(basebuf, sizeof(basebuf), "I%d", (int)fi.operands[oi].reg); base = basebuf; break;
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
        /* Also show extra operands for variable-operand instructions (CALL/CALLG/POLY) */
        /* Show effective addresses for CALL arguments (what actually gets passed) */
        if (m->cpu && m->cpu->extra_operand_count > 0) {
            for (uint16_t ei = 0; ei < m->cpu->extra_operand_count; ++ei) {
                printf(",0x%X", m->cpu->extra_operands[ei].effective_address);
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
                    int32_t displacement = nd500_get_operand_displacement(&fi.operands[0]);
                    target = nd500_calc_branch_target(fi.address + fi.total_len, displacement);
                    found_target = 1;
                } else {
                    /* Absolute call: target is first operand value (BIG-ENDIAN) */
                    if (fi.operands[0].data_len >= 4) {
                        target = ((uint32_t)fi.operands[0].data[0] << 24) | ((uint32_t)fi.operands[0].data[1] << 16) |
                                 ((uint32_t)fi.operands[0].data[2] << 8) | (uint32_t)fi.operands[0].data[3];
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

static size_t buf_append(char* out, size_t cap, size_t pos, const char* fmt, ...) {
    if (!out || cap == 0) return pos;
    if (pos >= cap) return pos;
    va_list ap;
    va_start(ap, fmt);
    int wrote = vsnprintf(out + pos, cap - pos, fmt, ap);
    va_end(ap);
    if (wrote < 0) return pos; /* encoding error: ignore */
    size_t inc = (size_t)wrote;
    if (pos + inc >= cap) {
        /* ensure NUL termination */
        out[cap - 1] = '\0';
        return cap - 1;
    }
    return pos + inc;
}

size_t nd500_dbg_disasm(Nd500Machine* m, uint32_t addr, uint32_t len, char* out, size_t out_cap) {
    return nd500_disasm_format_range(m, addr, len, out, out_cap);
}

void nd500_dbg_step(Nd500Machine* m, uint32_t count) {
	if (!m || !m->cpu) return;
	for (uint32_t i = 0; i < count; ++i) {
		nd500_cpu_step(m->cpu);
		/* Stop stepping if CPU was halted (e.g., MON 0B LEAVE) */
		if (m->run_flag == 0 && m->stop_reason != STOP_NONE) {
			break;
		}
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

/* NOTE: nd500_dbg_load_aout_file and nd500_dbg_load_aout_buffer are now in machine_loader.c */

int nd500_dbg_load_aout_buffer_OLD_UNUSED(Nd500Machine* m, const uint8_t* buf, size_t size, uint32_t* out_entry_pc) {
    if (!m || !buf || size < sizeof(unsigned int) * 8) return -1;
    /* Parse nd500 a.out header (32-bit fields, little-endian in our toolchain outputs) */
    unsigned int a_magic   = (unsigned int)(buf[0]  | (buf[1]  << 8) | (buf[2]  << 16) | (buf[3]  << 24));
    unsigned int a_text    = (unsigned int)(buf[4]  | (buf[5]  << 8) | (buf[6]  << 16) | (buf[7]  << 24));
    unsigned int a_data    = (unsigned int)(buf[8]  | (buf[9]  << 8) | (buf[10] << 16) | (buf[11] << 24));
    unsigned int a_bss     = (unsigned int)(buf[12] | (buf[13] << 8) | (buf[14] << 16) | (buf[15] << 24));
    unsigned int a_syms    = (unsigned int)(buf[16] | (buf[17] << 8) | (buf[18] << 16) | (buf[19] << 24));
    unsigned int a_entry   = (unsigned int)(buf[20] | (buf[21] << 8) | (buf[22] << 16) | (buf[23] << 24));
    unsigned int a_trsize  = (unsigned int)(buf[24] | (buf[25] << 8) | (buf[26] << 16) | (buf[27] << 24));
    unsigned int a_drsize  = (unsigned int)(buf[28] | (buf[29] << 8) | (buf[30] << 16) | (buf[31] << 24));

    /* Validate magic using same set as ndlib */
    if (!(a_magic == 0407 || a_magic == 0410 || a_magic == 0413 || a_magic == 0411 ||
          a_magic == 0x0107 || a_magic == 0x0108 || a_magic == 0x0109 || a_magic == 0x010B)) {
        /* Not an a.out; fallback: blunt copy at 0 */
        size_t to_copy = size;
        if (to_copy > m->memory_size) to_copy = m->memory_size;
        for (size_t i = 0; i < to_copy; ++i) nd500_bus_write8(m, (uint32_t)i, buf[i]);
        if (out_entry_pc) *out_entry_pc = 0;
        if (m->cpu) m->cpu->PC = 0;
        return 0;
    }

    /* Determine object vs executable (match ndlib logic) */
    int has_reloc = (a_trsize > 0 || a_drsize > 0);
    int is_placeholder_entry = (a_entry == 4);
    int is_object = has_reloc || is_placeholder_entry;

    /* Layout immediately after 32-byte header: text, then data, then reloc, then symbols/strings */
    const unsigned int hdr_size = 32;
    if (size < hdr_size) return -1;

    /* Load text */
    if (a_text && size >= hdr_size + a_text) {
        for (unsigned int i = 0; i < a_text; ++i) {
            nd500_bus_write8(m, i, buf[hdr_size + i]);
        }
    }
    /* Load data */
    if (a_data && size >= hdr_size + a_text + a_data) {
        unsigned int data_off = hdr_size + a_text;
        for (unsigned int i = 0; i < a_data; ++i) {
            nd500_bus_write8(m, a_text + i, buf[data_off + i]);
        }
    }
    /* Zero BSS */
    for (unsigned int i = 0; i < a_bss; ++i) {
        nd500_bus_write8(m, a_text + a_data + i, 0);
    }

    unsigned int entry = is_object ? 0u : a_entry;
    if (out_entry_pc) *out_entry_pc = entry;
    if (m->cpu) m->cpu->PC = entry;
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

/* Memory trace mode functions */
int nd500_dbg_set_memtrace(int flags) {
    g_memtrace_flags = flags;
    return g_memtrace_flags;
}

int nd500_dbg_get_memtrace(void) {
    return g_memtrace_flags;
}

/* Layout constants for trace output (matching C# implementation) */
#define TRACE_BYTES_PER_LINE 6
#define TRACE_PC_WIDTH       10   /* "0x0802D467" */
#define TRACE_BYTES_WIDTH    18   /* 6 bytes * 3 chars */
#define TRACE_DISASM_WIDTH   40   /* fixed width for disassembly */

/* Format flags as string: PDZSCKO (uppercase=set, lowercase=clear) */
static void format_flags(uint32_t st1, char* out) {
    /* Flag bit positions in ST1 - matching RetroCore format */
    out[0] = (st1 & (1u << 1)) ? 'P' : 'p';  /* PIA - Privileged Instructions Allowed */
    out[1] = (st1 & (1u << 4)) ? 'D' : 'd';  /* PSD - Process Switch Disabled */
    out[2] = (st1 & (1u << 5)) ? 'Z' : 'z';  /* Zero */
    out[3] = (st1 & (1u << 7)) ? 'S' : 's';  /* Sign */
    out[4] = (st1 & (1u << 6)) ? 'C' : 'c';  /* Carry */
    out[5] = (st1 & (1u << 8)) ? 'K' : 'k';  /* K (destination full) */
    out[6] = (st1 & (1u << 9)) ? 'O' : 'o';  /* Overflow */
    out[7] = '\0';
}

/*
 * Two-phase trace: before execution
 * Format: PC BYTES(max 6) MNEMONIC(40 wide) | I1[x] I2[x] I3[x] I4[x] [flags] B[x] L[x] R[x] TOS[x]
 *         (continuation bytes if >6)
 */
void nd500_dbg_trace_before(uint32_t pc, const char* mnemonic,
                            const uint8_t* instr_bytes, int instr_len,
                            uint32_t* before_regs) {
    if (!nd500_dbg_get_trace_mode()) return;

    int nbytes = (instr_len > 32) ? 32 : instr_len;

    /* Format flags string */
    char flag_str[8];
    format_flags(before_regs[8], flag_str);

    /* Build register state string */
    char regs_str[256];
    snprintf(regs_str, sizeof(regs_str),
             "I1[%08X] I2[%08X] I3[%08X] I4[%08X] [%s] B[%08X] L[%08X] R[%08X] TOS[%08X]",
             before_regs[1], before_regs[2], before_regs[3], before_regs[4],
             flag_str,
             before_regs[6], before_regs[5], before_regs[7], before_regs[9]);

    /* Format first 6 bytes */
    char bytes_str[64];
    int bytes_pos = 0;
    int first_chunk = (nbytes > TRACE_BYTES_PER_LINE) ? TRACE_BYTES_PER_LINE : nbytes;
    for (int i = 0; i < first_chunk; i++) {
        if (i > 0) bytes_str[bytes_pos++] = ' ';
        bytes_pos += snprintf(bytes_str + bytes_pos, sizeof(bytes_str) - bytes_pos, "%02X", instr_bytes[i]);
    }
    bytes_str[bytes_pos] = '\0';

    /* Use mnemonic directly */
    const char* mnem = mnemonic ? mnemonic : "???";

    /* Print first line: PC + bytes + mnemonic + registers */
    printf("0x%08X %-*s %-*s | %s\n",
           pc,
           TRACE_BYTES_WIDTH, bytes_str,
           TRACE_DISASM_WIDTH, mnem,
           regs_str);

    /* Print continuation lines for remaining bytes (>6) */
    int offset = TRACE_BYTES_PER_LINE;
    while (offset < nbytes) {
        int chunk = (nbytes - offset > TRACE_BYTES_PER_LINE) ? TRACE_BYTES_PER_LINE : (nbytes - offset);
        bytes_pos = 0;
        for (int i = 0; i < chunk; i++) {
            if (i > 0) bytes_str[bytes_pos++] = ' ';
            bytes_pos += snprintf(bytes_str + bytes_pos, sizeof(bytes_str) - bytes_pos, "%02X", instr_bytes[offset + i]);
        }
        bytes_str[bytes_pos] = '\0';
        /* Indent continuation: PC_WIDTH + 1 space */
        printf("%*s%s\n", TRACE_PC_WIDTH + 1, "", bytes_str);
        offset += TRACE_BYTES_PER_LINE;
    }
}

/*
 * Two-phase trace: after execution
 * Prints register changes: "  -> I1=xxx B=xxx L=xxx ..."
 */
void nd500_dbg_trace_after(uint32_t* before_regs, uint32_t* after_regs) {
    if (!nd500_dbg_get_trace_mode()) return;

    int any_changed = 0;
    char changes[512];
    int pos = 0;

    /* Start with arrow prefix */
    pos += snprintf(changes + pos, sizeof(changes) - pos, "  -> ");

    /* Check I1-I4 */
    if (after_regs[1] != before_regs[1]) {
        pos += snprintf(changes + pos, sizeof(changes) - pos, "I1=%08X ", after_regs[1]);
        any_changed = 1;
    }
    if (after_regs[2] != before_regs[2]) {
        pos += snprintf(changes + pos, sizeof(changes) - pos, "I2=%08X ", after_regs[2]);
        any_changed = 1;
    }
    if (after_regs[3] != before_regs[3]) {
        pos += snprintf(changes + pos, sizeof(changes) - pos, "I3=%08X ", after_regs[3]);
        any_changed = 1;
    }
    if (after_regs[4] != before_regs[4]) {
        pos += snprintf(changes + pos, sizeof(changes) - pos, "I4=%08X ", after_regs[4]);
        any_changed = 1;
    }

    /* Check flags (ST1) */
    if (after_regs[8] != before_regs[8]) {
        char flag_str[8];
        format_flags(after_regs[8], flag_str);
        pos += snprintf(changes + pos, sizeof(changes) - pos, "[%s] ", flag_str);
        any_changed = 1;
    }

    /* Check B, L, R, TOS */
    if (after_regs[6] != before_regs[6]) {
        pos += snprintf(changes + pos, sizeof(changes) - pos, "B=%08X ", after_regs[6]);
        any_changed = 1;
    }
    if (after_regs[5] != before_regs[5]) {
        pos += snprintf(changes + pos, sizeof(changes) - pos, "L=%08X ", after_regs[5]);
        any_changed = 1;
    }
    if (after_regs[7] != before_regs[7]) {
        pos += snprintf(changes + pos, sizeof(changes) - pos, "R=%08X ", after_regs[7]);
        any_changed = 1;
    }
    if (after_regs[9] != before_regs[9]) {
        pos += snprintf(changes + pos, sizeof(changes) - pos, "TOS=%08X ", after_regs[9]);
        any_changed = 1;
    }

    /* Only print if something changed */
    if (any_changed) {
        /* Remove trailing space */
        if (pos > 0 && changes[pos-1] == ' ') changes[pos-1] = '\0';
        printf("%s\n", changes);
    }
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

/* Breakpoint helper functions */
bool nd500_dbg_has_breakpoint_at(Nd500Machine* m, uint32_t addr) {
    if (!m || !m->bp_mgr) return false;

    for (int i = 0; i < m->bp_mgr->bp_count; i++) {
        if (m->bp_mgr->breakpoints[i].enabled &&
            m->bp_mgr->breakpoints[i].address == addr) {
            return true;
        }
    }
    return false;
}


