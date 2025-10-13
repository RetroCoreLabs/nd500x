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
            }
        }
        printf("%s", color_reset());
        
        /* === Check for relocations/unresolved externals in this instruction === */
        uint8_t is_undefined = 0;
        const char* reloc_symbol = ndlib_symbols_reloc_for_range(fi.address, fi.address + fi.total_len, &is_undefined);
        
        if (reloc_symbol && *reloc_symbol) {
            /* Show relocation comment */
            if (is_undefined) {
                printf(" %s; %s (UNRESOLVED)%s", color_comment(), reloc_symbol, color_reset());
            } else {
                printf(" %s; -> %s%s", color_comment(), reloc_symbol, color_reset());
            }
        } else if (nd500_instr_is_branch(fi.opcode) && fi.operand_count > 0 && fi.operands[0].address_code == 0xFF) {
            /* Fallback: For branches without relocations, still show target symbols */
            uint32_t target = 0;
            int is_pc_relative = (fi.operand_count == 1);
            
            if (is_pc_relative) {
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
            } else {
                if (fi.operands[0].data_len >= 4) {
                    target = (uint32_t)fi.operands[0].data[0] | ((uint32_t)fi.operands[0].data[1] << 8) |
                             ((uint32_t)fi.operands[0].data[2] << 16) | ((uint32_t)fi.operands[0].data[3] << 24);
                }
            }
            
            const char* target_sym = ndlib_symbols_name_for_addr(target);
            if (target_sym && *target_sym) {
                printf(" %s; <%s>%s", color_comment(), target_sym, color_reset());
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


