/**
 * nd500_disasm.c - ND-500 Instruction Disassembler
 *
 * This module provides disassembly output for ND-500 instructions,
 * formatting operands according to ND-500 assembly syntax conventions.
 *
 * CRITICAL IMPLEMENTATION NOTE - Unsigned Displacements (2024-12-12):
 * ===================================================================
 * Per ND-05.009.4 Section 8.4 "Local addressing":
 *   "Displacement values are treated as unsigned."
 *
 * This applies to the following addressing modes:
 *   - LOCAL (b.offset)        - base B register + unsigned displacement
 *   - LOCAL_SHORT (b.offset)  - short form, offset = low6 * 4
 *   - LOCAL_PI (b.offset(rN)) - with post-indexing
 *   - LOCAL_IND (IND(b.offset)) - indirect
 *   - LOCAL_IND_PI            - indirect with post-indexing
 *   - RECORD (r.offset)       - base R register + unsigned displacement
 *   - RECORD_SHORT (r.offset) - short form
 *   - PREINDEXED (rN.offset)  - indexed register + unsigned displacement
 *
 * Example of correct interpretation:
 *   Encoding: 4A C1 AC  (W STZ with LOCAL mode, 1-byte displacement 0xAC)
 *   Correct:  W STZ b.172   (0xAC = 172 unsigned)
 *   Wrong:    W STZ b.-84   (0xAC as signed = -84)
 *
 * This module uses fmt_unsigned() for all displacement values in these
 * modes to match the correct ND-500 behavior.
 *
 * Reference: ND-05.009.4 ND-500 CPU Programmer's Reference Manual
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include "nd500_disasm.h"
#include "../cpu/cpu_protos.h"
#include "../machine/machine_protos.h"
#include "../ndlib/ndlib.h"

/* Format unsigned value based on radix */
static int fmt_unsigned(char* buf, size_t cap, uint32_t val) {
    int radix = nd500_dbg_get_radix();
    switch (radix) {
        case 1:  return snprintf(buf, cap, "0x%X", val);      /* hex */
        case 2:  return snprintf(buf, cap, "%o", val);        /* octal */
        default: return snprintf(buf, cap, "%u", val);        /* decimal */
    }
}

/* Format signed value based on radix - preserves sign for negative values */
static int fmt_signed(char* buf, size_t cap, int32_t val) {
    int radix = nd500_dbg_get_radix();
    switch (radix) {
        case 1:  /* hex */
            if (val < 0)
                return snprintf(buf, cap, "-0x%X", (unsigned)(-val));
            else
                return snprintf(buf, cap, "0x%X", (unsigned)val);
        case 2:  /* octal */
            if (val < 0)
                return snprintf(buf, cap, "-%o", (unsigned)(-val));
            else
                return snprintf(buf, cap, "%o", (unsigned)val);
        default: /* decimal */
            return snprintf(buf, cap, "%d", val);
    }
}

/**
 * Get the fixed destination data type for conversion instructions.
 * Uses PURELY NUMERIC checks - no string comparison.
 *
 * CONV instructions occupy opcodes 0xFD44-0xFD61 (30 opcodes).
 * Each CONV instruction's prefixes_mask is missing exactly one bit - the destination type.
 *
 * | Instruction | prefixes_mask | Missing Bit | Destination Type |
 * |-------------|---------------|-------------|------------------|
 * | BICONV      | 0x3E          | 0x01 (BI)   | BYTE             |
 * | BYCONV      | 0x3D          | 0x02 (BY)   | BYTE             |
 * | HCONV       | 0x3B          | 0x04 (H)    | HALFWORD         |
 * | WCONV       | 0x37          | 0x08 (W)    | WORD             |
 * | FCONV       | 0x2F          | 0x10 (F)    | FLOAT            |
 * | DCONV       | 0x1F          | 0x20 (D)    | DOUBLEWORD       |
 *
 * Formula: missing_bit = 0x3F ^ prefixes_mask
 *
 * @param opcode    The instruction opcode
 * @param out_dtype Output: the fixed destination data type
 * @return 1 if this is a CONV instruction, 0 otherwise
 */
static int get_conv_dest_dtype(uint16_t opcode, Nd500DataType* out_dtype) {
    if (!out_dtype) return 0;

    /* CONV instructions are in range 0xFD44-0xFD61 (purely numeric check) */
    if (opcode < 0xFD44 || opcode > 0xFD61) return 0;

    /* Inverse prefix mask: missing bit = destination type */
    uint8_t prefixes = nd500_instr_prefixes_mask(opcode);
    uint8_t missing = 0x3F ^ prefixes;

    switch (missing) {
        case 0x01: /* BI missing -> BICONV */
        case 0x02: /* BY missing -> BYCONV */
            *out_dtype = ND500_DTYPE_BYTE;
            return 1;
        case 0x04: /* H missing -> HCONV */
            *out_dtype = ND500_DTYPE_HALFWORD;
            return 1;
        case 0x08: /* W missing -> WCONV */
            *out_dtype = ND500_DTYPE_WORD;
            return 1;
        case 0x10: /* F missing -> FCONV */
            *out_dtype = ND500_DTYPE_FLOAT;
            return 1;
        case 0x20: /* D missing -> DCONV */
            *out_dtype = ND500_DTYPE_DOUBLEWORD;
            return 1;
        default:
            return 0;
    }
}

/**
 * Authoritative operand formatter for disassembly output.
 * Handles all 15 ND-500 addressing modes with correct syntax.
 *
 * For REGISTER mode, shows correct register bank based on data type:
 * - BYTE/HALFWORD/WORD: W1-W4 (integer registers I1-I4)
 * - FLOAT: F1-F4 (float registers A1-A4)
 * - DOUBLEWORD: D1-D4 (double registers, A+E pairs)
 *
 * @param buf       Output buffer
 * @param cap       Buffer capacity
 * @param op        Decoded operand
 * @param dtype     Data type (determines register bank for REGISTER mode)
 * @param use_color Whether to include ANSI color codes (not yet implemented)
 * @return Number of characters written (excluding NUL terminator)
 */
int nd500_format_operand(char* buf, size_t cap, const Nd500OperandDecoded* op, Nd500DataType dtype, bool use_color) {
    (void)use_color; /* Reserved for future use */
    if (!buf || cap == 0 || !op) return 0;
    char* p = buf; char* e = buf + cap;
    
    /* Extract value from data bytes (BIG-ENDIAN - ND-500 native byte order) */
    uint32_t val = 0; int32_t sval = 0;
    if (op->data_len == 1) { val = op->data[0]; sval = (int8_t)op->data[0]; }
    else if (op->data_len == 2) { val = ((uint32_t)op->data[0] << 8) | (uint32_t)op->data[1]; sval = (int16_t)val; }
    else if (op->data_len >= 4) { val = ((uint32_t)op->data[0] << 24) | ((uint32_t)op->data[1] << 16) | ((uint32_t)op->data[2] << 8) | (uint32_t)op->data[3]; sval = (int32_t)val; }

    /* Handle prefix bytes - add DESC() or ALT() wrapper */
    int has_desc = op->has_desc_prefix;
    int has_alt = op->has_alt_prefix;
    
    if (has_alt && p < e) {
        int n = snprintf(p, (size_t)(e-p), "ALT(");
        if (n>0) p += (n < (e-p)? n : (int)(e-p));
    }
    if (has_desc && p < e) {
        int n = snprintf(p, (size_t)(e-p), "DESC%d(", (int)op->reg + 1);
        if (n>0) p += (n < (e-p)? n : (int)(e-p));
    }

    /* Inline/direct markers from decoder: 0xFE/0xFF */
    if (op->address_code == 0xFE || op->address_code == 0xFF) {
        if (p < e) *p++ = '$';
        int n = (op->address_code == 0xFF && op->data_len <= 2)
            ? fmt_signed(p, (size_t)(e-p), sval)
            : fmt_unsigned(p, (size_t)(e-p), val);
        if (n > 0) p += (n < (e-p) ? n : (int)(e-p));
        goto close_wrappers;
    }

    /* Short-forms embed value in AC low 6 bits */
    uint8_t low6 = op->address_code & 0x3F;
    
    switch (op->mode) {
        case ND500_ADDR_CONSTANT_SHORT: {
            /* 6-bit signed constant sign-extended */
            uint32_t val32 = (low6 & 0x20) ? (uint32_t)(low6 | 0xFFFFFFC0) : (uint32_t)low6;
            if (p < e) *p++ = '$';
            int n = fmt_signed(p, (size_t)(e-p), (int32_t)val32);
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            break;
        }
        case ND500_ADDR_LOCAL_SHORT: {
            /* b.offset where offset = low6 * 4 */
            int n = snprintf(p, (size_t)(e-p), "b.");
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            n = fmt_unsigned(p, (size_t)(e-p), (unsigned)(low6 * 4u));
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            break;
        }
        case ND500_ADDR_RECORD_SHORT: {
            /* r.offset where offset = low6 * 4 */
            int n = snprintf(p, (size_t)(e-p), "r.");
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            n = fmt_unsigned(p, (size_t)(e-p), (unsigned)(low6 * 4u));
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            break;
        }
        case ND500_ADDR_REGISTER: {
            /* F1. REGISTER: use address_code bits 0-1 to get register 1-4
             * Show correct register name based on data type:
             * - FLOAT: F1-F4 (A registers)
             * - DOUBLEWORD: D1-D4 (A+E pairs)
             * - Otherwise: W1-W4 (I registers) */
            int regnum = (op->address_code & 0x03) + 1;
            const char* prefix;
            switch (dtype) {
                case ND500_DTYPE_FLOAT:      prefix = "F"; break;
                case ND500_DTYPE_DOUBLEWORD: prefix = "D"; break;
                default:                     prefix = "W"; break;
            }
            int n = snprintf(p, (size_t)(e-p), "%s%d", prefix, regnum);
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            break;
        }
        case ND500_ADDR_PREINDEXED: {
            /* F2. PREINDEXED: rN.offset (not disp(rN)) */
            /* Per ND-05.009.4 Section 8.4: displacements are UNSIGNED */
            int regnum = (op->address_code & 0x03) + 1;
            int n = snprintf(p, (size_t)(e-p), "r%d.", regnum);
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            n = fmt_unsigned(p, (size_t)(e-p), val);
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            break;
        }
        case ND500_ADDR_LOCAL: {
            /* b.offset - displacements are UNSIGNED per manual */
            int n = snprintf(p, (size_t)(e-p), "b.");
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            n = fmt_unsigned(p, (size_t)(e-p), val);
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            break;
        }
        case ND500_ADDR_LOCAL_PI: {
            /* F5. LOCAL_PI: b.offset(rN) not b.offset+ */
            int regnum = (op->address_code & 0x03) + 1;
            int n = snprintf(p, (size_t)(e-p), "b.");
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            n = fmt_unsigned(p, (size_t)(e-p), val);
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            n = snprintf(p, (size_t)(e-p), "(r%d)", regnum);
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            break;
        }
        case ND500_ADDR_LOCAL_IND: {
            /* F3. LOCAL_IND: IND(b.offset) not @b.offset */
            int n = snprintf(p, (size_t)(e-p), "IND(b.");
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            n = fmt_unsigned(p, (size_t)(e-p), val);
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            if (p < e) *p++ = ')';
            break;
        }
        case ND500_ADDR_LOCAL_IND_PI: {
            /* F4. LOCAL_IND_PI: IND(b.offset)(rN) */
            int regnum = (op->address_code & 0x03) + 1;
            int n = snprintf(p, (size_t)(e-p), "IND(b.");
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            n = fmt_unsigned(p, (size_t)(e-p), val);
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            n = snprintf(p, (size_t)(e-p), ")(r%d)", regnum);
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            break;
        }
        case ND500_ADDR_RECORD: {
            /* r.offset - displacements are UNSIGNED per manual */
            int n = snprintf(p, (size_t)(e-p), "r.");
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            n = fmt_unsigned(p, (size_t)(e-p), val);
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            break;
        }
        case ND500_ADDR_ABSOLUTE: {
            /* $address */
            if (p < e) *p++ = '$';
            int n = fmt_unsigned(p, (size_t)(e-p), val);
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            break;
        }
        case ND500_ADDR_ABSOLUTE_PI: {
            /* F6. ABSOLUTE_PI: $address(rN) not $address+ */
            int regnum = (op->address_code & 0x03) + 1;
            if (p < e) *p++ = '$';
            int n = fmt_unsigned(p, (size_t)(e-p), val);
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            n = snprintf(p, (size_t)(e-p), "(r%d)", regnum);
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            break;
        }
        case ND500_ADDR_CONSTANT: {
            /* #value (immediate constant) */
            if (p < e) *p++ = '#';
            int n = fmt_unsigned(p, (size_t)(e-p), val);
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            break;
        }
        case ND500_ADDR_DESCRIPTOR:
        case ND500_ADDR_ALTERNATIVE:
        default: {
            /* Fallback for unknown modes - show raw address code and any data */
            int n = snprintf(p, (size_t)(e-p), "?AC%02X", op->address_code);
            if (n>0) p += (n < (e-p)? n : (int)(e-p));
            if (op->data_len > 0) {
                n = snprintf(p, (size_t)(e-p), "($");
                if (n>0) p += (n < (e-p)? n : (int)(e-p));
                n = fmt_unsigned(p, (size_t)(e-p), val);
                if (n>0) p += (n < (e-p)? n : (int)(e-p));
                if (p < e) *p++ = ')';
            }
            break;
        }
    }

close_wrappers:
    /* Close DESC() and ALT() wrappers in reverse order */
    if (has_desc && p < e) *p++ = ')';
    if (has_alt && p < e) *p++ = ')';

    if (p < e) *p = '\0';
    return (int)(p - buf);
}

/* Internal wrapper - calls nd500_format_operand with dtype and no color */
static size_t fmt_operand(char* dst, size_t cap, const Nd500OperandDecoded* op, Nd500DataType dtype) {
    int ret = nd500_format_operand(dst, cap, op, dtype, false);
    return ret > 0 ? (size_t)ret : 0;
}

/**
 * Calculate branch target address from PC and displacement.
 * Shared function to avoid duplicate branch target calculation logic.
 * 
 * @param pc                 Current instruction address
 * @param displacement       Signed displacement value from operand
 * @return Absolute target address
 */
uint32_t nd500_calc_branch_target(uint32_t pc, int32_t displacement) {
    return (uint32_t)((int32_t)pc + displacement);
}

/**
 * Extract displacement from operand data bytes (big-endian).
 * 
 * @param op  Decoded operand
 * @return Signed displacement value
 */
int32_t nd500_get_operand_displacement(const Nd500OperandDecoded* op) {
    if (!op) return 0;
    if (op->data_len == 1) {
        return (int8_t)op->data[0];
    } else if (op->data_len == 2) {
        uint16_t raw = ((uint16_t)op->data[0] << 8) | (uint16_t)op->data[1];
        return (int16_t)raw;
    } else if (op->data_len >= 4) {
        uint32_t raw = ((uint32_t)op->data[0] << 24) | ((uint32_t)op->data[1] << 16) |
                       ((uint32_t)op->data[2] << 8) | (uint32_t)op->data[3];
        return (int32_t)raw;
    }
    return 0;
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

size_t nd500_format_instruction(char* buf, size_t cap, const Nd500FetchedInstruction* fi) {
    if (!buf || cap == 0 || !fi) return 0;
    size_t pos = 0;

    /* Build display mnemonic with optional dtype/register prefix */
    const char* mnem = fi->mnemonic ? fi->mnemonic : "???";
    char regprefix[16];
    regprefix[0] = '\0';

    if (nd500_instr_has_rn(fi->opcode)) {
        int dreg = nd500_instr_dest_reg(fi->opcode);
        if (dreg >= 0 && dreg < 4) {
            const char* dts = nd500_instr_dtype_prefix(fi->opcode);
            snprintf(regprefix, sizeof(regprefix), "%s%d ", dts, dreg + 1);
        }
    } else {
        uint8_t dtype_mask = (uint8_t)(nd500_instr_prefixes_mask(fi->opcode) & 0x3F);
        if (dtype_mask != 0) {
            const char* dts = nd500_instr_dtype_prefix(fi->opcode);
            snprintf(regprefix, sizeof(regprefix), "%s ", dts);
        }
    }

    /* Print prefix + mnemonic + operands */
    if (fi->operand_count > 0) {
        pos = buf_append(buf, cap, pos, "%s%-12s ", regprefix, mnem);
        for (uint8_t oi = 0; oi < fi->operand_count; ++oi) {
            char obuf[64];
            size_t ol = fmt_operand(obuf, sizeof(obuf), &fi->operands[oi], fi->data_type);
            if (ol > 0) {
                pos = buf_append(buf, cap, pos, "%s%s", (oi > 0) ? "," : "", obuf);
            }
        }
    } else {
        pos = buf_append(buf, cap, pos, "%s%s", regprefix, mnem);
    }

    return pos;
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
            pos = buf_append(out, out_cap, pos, "%08X: %02X            ???\n", a, b);
            a += 1;
            continue;
        }

        /* Check for symbol at this address and print label */
        const char* symbol_at_addr = ndlib_symbols_name_for_addr(fi.address);
        if (symbol_at_addr && *symbol_at_addr) {
            pos = buf_append(out, out_cap, pos, "%08X:                  %s:\n", fi.address, symbol_at_addr);
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

            /* Check if this is a conversion instruction (BICONV, BYCONV, HCONV, etc.)
             * Conversion instructions have fixed destination type different from source.
             * Format: t1 t2CONV {source/t1}, {dest/t2}
             * - Operand[0] (source): uses fi.data_type (from prefix)
             * - Operand[1] (dest): uses fixed type based on instruction name */
            Nd500DataType conv_dest_dtype;
            int is_conv = get_conv_dest_dtype(fi.opcode, &conv_dest_dtype);

            for (uint8_t oi = 0; oi < fi.operand_count; ++oi) {
                char obuf[64];
                /* Use correct dtype: source uses instruction prefix, dest uses fixed type */
                Nd500DataType op_dtype = fi.data_type;
                if (is_conv && oi == 1) {
                    op_dtype = conv_dest_dtype;
                }
                size_t ol = fmt_operand(obuf, sizeof(obuf), &fi.operands[oi], op_dtype);
                if (ol > 0) {
                    pos = buf_append(out, out_cap, pos, "%s%s", (oi > 0) ? "," : "", obuf);
                }
            }
            /* Also show extra operands for variable-operand instructions (CALL/CALLG/POLY) */
            /* Show effective addresses for CALL arguments (what actually gets passed) */
            if (m->cpu && m->cpu->extra_operand_count > 0) {
                for (uint16_t ei = 0; ei < m->cpu->extra_operand_count; ++ei) {
                    pos = buf_append(out, out_cap, pos, ",0x%X",
                                     m->cpu->extra_operands[ei].effective_address);
                }
            }
        } else {
            pos = buf_append(out, out_cap, pos, "%s%s", regprefix, mnem);
        }

        /* Check for call/branch target symbols */
        if (nd500_instr_is_branch(fi.opcode) && fi.operand_count > 0) {
            uint32_t target = 0;
            int found_target = 0;

            if (fi.operands[0].address_code == 0xFF || fi.operands[0].address_code == 0xFE) {
                int is_pc_relative = (fi.operand_count == 1);

                if (is_pc_relative) {
                    /* PC-relative branch: target = PC + displacement */
                    int32_t displacement = nd500_get_operand_displacement(&fi.operands[0]);
                    target = nd500_calc_branch_target(fi.address, displacement);
                    found_target = 1;
                } else {
                    /* Absolute call: target is first operand value (big-endian) */
                    if (fi.operands[0].data_len >= 4) {
                        target = ((uint32_t)fi.operands[0].data[0] << 24) | ((uint32_t)fi.operands[0].data[1] << 16) |
                                 ((uint32_t)fi.operands[0].data[2] << 8) | (uint32_t)fi.operands[0].data[3];
                        found_target = 1;
                    }
                }
            }

            if (found_target) {
                const char* target_sym = ndlib_symbols_name_for_addr(target);
                if (target_sym && *target_sym) {
                    pos = buf_append(out, out_cap, pos, "  ; -> <%s>", target_sym);
                }
            }
        }

        pos = buf_append(out, out_cap, pos, "\n");

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
            /* Check if this is a conversion instruction */
            Nd500DataType conv_dest_dtype;
            int is_conv = get_conv_dest_dtype(fi.opcode, &conv_dest_dtype);

            for (uint8_t oi = 0; oi < fi.operand_count; ++oi) {
                char obuf[64];
                Nd500DataType op_dtype = fi.data_type;
                if (is_conv && oi == 1) {
                    op_dtype = conv_dest_dtype;
                }
                size_t ol = fmt_operand(obuf, sizeof(obuf), &fi.operands[oi], op_dtype);
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
                    /* PC-relative branch: target = PC + displacement */
                    int32_t displacement = nd500_get_operand_displacement(&fi.operands[0]);
                    target = nd500_calc_branch_target(fi.address, displacement);
                    found_target = 1;
                } else {
                    /* Absolute call: target is first operand value (big-endian) */
                    if (fi.operands[0].data_len >= 4) {
                        target = ((uint32_t)fi.operands[0].data[0] << 24) | ((uint32_t)fi.operands[0].data[1] << 16) |
                                 ((uint32_t)fi.operands[0].data[2] << 8) | (uint32_t)fi.operands[0].data[3];
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


