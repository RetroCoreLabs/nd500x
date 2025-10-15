#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "nd500_disasm.h"
#include "../cpu/cpu_protos.h"
#include "../machine/machine_protos.h"
#include "../ndlib/ndlib.h"

/* Minimal operand formatter for plain-text disassembly (no colors) */
static size_t fmt_operand(char* dst, size_t cap, const Nd500OperandDecoded* op) {
    if (!dst || cap == 0 || !op) return 0;
    char* p = dst; char* e = dst + cap;
    uint32_t val = 0; int32_t sval = 0;
    if (op->data_len == 1) { val = op->data[0]; sval = (int8_t)op->data[0]; }
    else if (op->data_len == 2) { val = (uint32_t)op->data[0] | ((uint32_t)op->data[1] << 8); sval = (int16_t)val; }
    else if (op->data_len >= 4) { val = (uint32_t)op->data[0] | ((uint32_t)op->data[1] << 8) | ((uint32_t)op->data[2] << 16) | ((uint32_t)op->data[3] << 24); sval = (int32_t)val; }

    /* Inline/direct markers from decoder: 0xFE/0xFF */
    if (op->address_code == 0xFE || op->address_code == 0xFF) {
        int n = snprintf(p, (size_t)(e-p), (op->address_code == 0xFF && op->data_len <= 2) ? "$%d" : "$%u", (op->address_code == 0xFF ? sval : (int)val));
        if (n > 0) p += (n < (e-p) ? n : (int)(e-p));
        if (p < e) *p = '\0';
        return (size_t)(p - dst);
    }

    /* Short-forms embed value in AC low 6 bits */
    uint8_t low6 = op->address_code & 0x3F;
    switch (op->mode) {
        case ND500_ADDR_CONSTANT_SHORT: {
            int n = snprintf(p, (size_t)(e-p), "$%u", (unsigned)low6);
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            break;
        }
        case ND500_ADDR_LOCAL_SHORT: {
            int n = snprintf(p, (size_t)(e-p), "b.%u", (unsigned)(low6 * 4u));
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            break;
        }
        case ND500_ADDR_RECORD_SHORT: {
            int n = snprintf(p, (size_t)(e-p), "r.%u", (unsigned)(low6 * 4u));
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            break;
        }
        case ND500_ADDR_REGISTER: {
            static const char* regs[] = {"r1","r2","r3","r4"};
            if (op->reg < 4) {
                int n = snprintf(p, (size_t)(e-p), "%s", regs[op->reg]);
                if (n>0) p += (n < (e-p)? n : (int)(e-p));
            }
            break;
        }
        case ND500_ADDR_ABSOLUTE:
        case ND500_ADDR_ABSOLUTE_PI: {
            int n = snprintf(p, (size_t)(e-p), "$%u", (unsigned)val);
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            break;
        }
        case ND500_ADDR_CONSTANT: {
            int n = snprintf(p, (size_t)(e-p), "#%u", (unsigned)val);
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            break;
        }
        case ND500_ADDR_LOCAL: {
            int n = snprintf(p, (size_t)(e-p), "b.%d", sval);
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            break;
        }
        case ND500_ADDR_RECORD: {
            int n = snprintf(p, (size_t)(e-p), "r.%d", sval);
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            break;
        }
        case ND500_ADDR_PREINDEXED: {
            static const char* regs[] = {"r1","r2","r3","r4"};
            if (op->reg < 4) {
                int n = snprintf(p, (size_t)(e-p), "%d(%s)", sval, regs[op->reg]);
                if (n>0) p += (n < (e-p)? n : (int)(e-p));
            }
            break;
        }
        default: {
            /* Fallback to showing immediate value if present */
            if (op->data_len > 0) {
                int n = snprintf(p, (size_t)(e-p), "$%u", (unsigned)val);
                if (n>0) p += (n < (e-p)? n : (int)(e-p));
            }
            break;
        }
    }
    if (p < e) *p = '\0';
    return (size_t)(p - dst);
}

static size_t buf_append(char* out, size_t cap, size_t pos, const char* fmt, ...) {
    if (!out || cap == 0) return pos;
    if (pos >= cap) return pos;
    va_list ap;
    va_start(ap, fmt);
    int wrote = vsnprintf(out + pos, cap - pos, fmt, ap);
    va_end(ap);
    if (wrote < 0) return pos;
    size_t inc = (size_t)wrote;
    if (pos + inc >= cap) { out[cap - 1] = '\0'; return cap - 1; }
    return pos + inc;
}

size_t nd500_disasm_format_range(struct Nd500Machine* m,
                                 uint32_t addr,
                                 uint32_t len,
                                 char* out,
                                 size_t out_cap) {
    if (!m || !m->cpu || !out || out_cap == 0) return 0;
    size_t pos = 0;
    uint32_t a = addr;
    uint32_t end = addr + len;
    while (a < end) {
        Nd500FetchedInstruction fi;
        memset(&fi, 0, sizeof(fi));
        int rc = nd500_decode_at(m, a, &fi);
        /* nd500_decode_at returns 0 on success, non-zero on failure */
        if (rc != 0 || fi.total_len == 0) {
            /* Fallback: show single byte unknown */
            uint8_t b = nd500_bus_read8(m, a);
            pos = buf_append(out, out_cap, pos, "%08X: %02X                       ??? ; opcode 0x%04X\n", a, b, (unsigned)b);
            a += 1;
            continue;
        }

        /* Address */
        pos = buf_append(out, out_cap, pos, "%08X:", fi.address);
        /* Bytes */
        for (uint32_t i = 0; i < fi.total_len; ++i) {
            pos = buf_append(out, out_cap, pos, " %02X", (unsigned)(fi.bytes[i] & 0xFF));
        }
        /* Pad to a fixed column for mnemonic readability */
        int bytes_field = (int)(fi.total_len * 3); /* ' XX' per byte */
        int pad = (bytes_field < 15) ? (15 - bytes_field) : 1;
        pos = buf_append(out, out_cap, pos, "%*s", pad, "");

        /* Build display mnemonic with optional dtype/register prefix for R_N */
        const char* mnem = fi.mnemonic ? fi.mnemonic : "???";
        char regprefix[16]; regprefix[0] = '\0';
        if (nd500_instr_has_rn(fi.opcode)) {
            int dreg = nd500_instr_dest_reg(fi.opcode);
            if (dreg >= 0 && dreg < 4) {
                const char* dts = nd500_instr_dtype_prefix(fi.opcode);
                snprintf(regprefix, sizeof(regprefix), "%s%d ", dts, dreg + 1);
            }
        } else {
            uint8_t dtype_mask = (uint8_t)(nd500_instr_prefixes_mask(fi.opcode) & 0x3F);
            if (dtype_mask != 0) {
                const char* dts = nd500_instr_dtype_prefix(fi.opcode);
                snprintf(regprefix, sizeof(regprefix), "%s ", dts);
            }
        }

        /* Print mnemonic (with prefix) and operands */
        if (fi.operand_count > 0) {
            pos = buf_append(out, out_cap, pos, "%s%-12s ", regprefix, mnem);
            for (uint8_t oi = 0; oi < fi.operand_count; ++oi) {
                char obuf[64];
                size_t ol = fmt_operand(obuf, sizeof(obuf), &fi.operands[oi]);
                if (ol > 0) {
                    pos = buf_append(out, out_cap, pos, "%s%s", (oi > 0) ? "," : "", obuf);
                }
            }
            pos = buf_append(out, out_cap, pos, "\n");
        } else {
            pos = buf_append(out, out_cap, pos, "%s%s\n", regprefix, mnem);
        }

        a += (uint32_t)fi.total_len;
    }
    return pos;
}

size_t nd500_disasm_format_range_json(struct Nd500Machine* m,
                                      uint32_t addr,
                                      uint32_t len,
                                      char* out,
                                      size_t out_cap) {
    if (!m || !m->cpu || !out || out_cap == 0) return 0;
    size_t pos = 0;
    uint32_t a = addr;
    uint32_t end = addr + len;
    
    pos = buf_append(out, out_cap, pos, "{\"instructions\":[");
    int first = 1;
    
    while (a < end) {
        Nd500FetchedInstruction fi;
        memset(&fi, 0, sizeof(fi));
        int rc = nd500_decode_at(m, a, &fi);
        
        if (!first) {
            pos = buf_append(out, out_cap, pos, ",");
        }
        first = 0;
        
        if (rc != 0 || fi.total_len == 0) {
            /* Fallback: show single byte unknown */
            uint8_t b = nd500_bus_read8(m, a);
            pos = buf_append(out, out_cap, pos, 
                "{\"address\":\"%08X\",\"bytes\":\"%02X\",\"mnemonic\":\"???\",\"operands\":\"\",\"is_unknown\":true}", 
                a, (unsigned)b);
            a += 1;
            continue;
        }

        /* Build address */
        pos = buf_append(out, out_cap, pos, "{\"address\":\"%08X\",", fi.address);
        
        /* Build bytes array */
        pos = buf_append(out, out_cap, pos, "\"bytes\":\"");
        for (uint32_t i = 0; i < fi.total_len; ++i) {
            pos = buf_append(out, out_cap, pos, "%s%02X", (i > 0) ? " " : "", (unsigned)(fi.bytes[i] & 0xFF));
        }
        pos = buf_append(out, out_cap, pos, "\",");

        /* Build mnemonic with optional prefix */
        const char* mnem = fi.mnemonic ? fi.mnemonic : "???";
        char regprefix[16]; regprefix[0] = '\0';
        if (nd500_instr_has_rn(fi.opcode)) {
            int dreg = nd500_instr_dest_reg(fi.opcode);
            if (dreg >= 0 && dreg < 4) {
                const char* dts = nd500_instr_dtype_prefix(fi.opcode);
                snprintf(regprefix, sizeof(regprefix), "%s%d", dts, dreg + 1);
            }
        } else {
            uint8_t dtype_mask = (uint8_t)(nd500_instr_prefixes_mask(fi.opcode) & 0x3F);
            if (dtype_mask != 0) {
                const char* dts = nd500_instr_dtype_prefix(fi.opcode);
                snprintf(regprefix, sizeof(regprefix), "%s", dts);
            }
        }

        /* Escape and output mnemonic */
        pos = buf_append(out, out_cap, pos, "\"mnemonic\":\"%s%s\",", regprefix, mnem);

        /* Build operands */
        pos = buf_append(out, out_cap, pos, "\"operands\":\"");
        if (fi.operand_count > 0) {
            for (uint8_t oi = 0; oi < fi.operand_count; ++oi) {
                char obuf[64];
                size_t ol = fmt_operand(obuf, sizeof(obuf), &fi.operands[oi]);
                if (ol > 0) {
                    pos = buf_append(out, out_cap, pos, "%s%s", (oi > 0) ? "," : "", obuf);
                }
            }
        }
        pos = buf_append(out, out_cap, pos, "\",\"is_unknown\":false");

        /* === Add symbol information === */
        const char* symbol_at_addr = ndlib_symbols_name_for_addr(fi.address);
        if (symbol_at_addr && *symbol_at_addr) {
            /* Escape any quotes in symbol name for JSON */
            pos = buf_append(out, out_cap, pos, ",\"symbol\":\"");
            for (const char* p = symbol_at_addr; *p; p++) {
                if (*p == '"' || *p == '\\') {
                    pos = buf_append(out, out_cap, pos, "\\%c", *p);
                } else {
                    pos = buf_append(out, out_cap, pos, "%c", *p);
                }
            }
            pos = buf_append(out, out_cap, pos, "\"");
        }

        /* Check for relocations/unresolved externals */
        uint8_t is_undefined = 0;
        const char* reloc_symbol = ndlib_symbols_reloc_for_range(fi.address, fi.address + fi.total_len, &is_undefined);

        if (reloc_symbol && *reloc_symbol) {
            pos = buf_append(out, out_cap, pos, ",\"reloc_symbol\":\"");
            for (const char* p = reloc_symbol; *p; p++) {
                if (*p == '"' || *p == '\\') {
                    pos = buf_append(out, out_cap, pos, "\\%c", *p);
                } else {
                    pos = buf_append(out, out_cap, pos, "%c", *p);
                }
            }
            pos = buf_append(out, out_cap, pos, "\",\"is_unresolved\":%s", is_undefined ? "true" : "false");
        } else if (nd500_instr_is_branch(fi.opcode) && fi.operand_count > 0) {
            /* Calculate branch/call target and look up symbol */
            uint32_t target = 0;
            int found_target = 0;

            if (fi.operands[0].address_code == 0xFF || fi.operands[0].address_code == 0xFE) {
                int is_pc_relative = (fi.operand_count == 1);

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
                /* Indirect calls/branches - use effective address */
                target = fi.operands[0].effective_address;
                found_target = 1;
            }

            if (found_target) {
                const char* target_sym = ndlib_symbols_name_for_addr(target);
                if (target_sym && *target_sym) {
                    pos = buf_append(out, out_cap, pos, ",\"target_symbol\":\"");
                    for (const char* p = target_sym; *p; p++) {
                        if (*p == '"' || *p == '\\') {
                            pos = buf_append(out, out_cap, pos, "\\%c", *p);
                        } else {
                            pos = buf_append(out, out_cap, pos, "%c", *p);
                        }
                    }
                    pos = buf_append(out, out_cap, pos, "\",\"target_address\":\"%08X\"", target);
                }
            }
        }

        pos = buf_append(out, out_cap, pos, "}");

        a += (uint32_t)fi.total_len;
    }
    
    pos = buf_append(out, out_cap, pos, "]}");
    return pos;
}


