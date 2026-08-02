/*
 * nrf_validate.c - Validator / dumper for ND Relocatable Format (NRF) object files.
 *
 * FULL PATH: test/nrf_validate.c
 *
 * WHY THIS EXISTS
 * ---------------
 * nd500-dis does NOT understand NRF (it only decodes DOM/a.out). nd100-dis's
 * "relocation" support is a.out16 relocation, NOT the ND control-group NRF
 * format. So there was no tool to answer "did NC actually emit a valid object?".
 * This standalone tool walks the NRF control-group stream and reports whether
 * the object is well-formed and COMPLETE (see the IHB check below), which is the
 * validation gate for Phase 1 of
 *   docs/MON_TO_BINARY_PLAN.md
 * ("NC compiles C to a valid NRF object").
 *
 * The format tables below are transcribed byte-for-byte from the reference
 * manual so this file can later be lifted into nd500-dis / nd500-dump as the
 * canonical NRF decoder:
 *   docs/ND-860289-2-EN ND Linker User Guide and
 *   Reference Manual.md  -> APPENDIX D "The ND Relocatable Format",
 *   pages 233-243 (markdown lines ~8102-8489). Page citations are on each item.
 *
 * BUILD (standalone, no project deps):
 *   gcc -std=c11 -Wall -Wextra -o build/bin/nrf_validate \
 *       test/nrf_validate.c
 * USE:
 *   nrf_validate <file.nrf>          # validate, exit 0 = valid+complete
 *   nrf_validate -d <file.nrf>       # also dump every control group
 * Exit codes: 0 = valid & complete; 1 = malformed/incomplete; 2 = usage/IO error.
 *
 * REFERENCE OBJECTS to study (genuine NRFs; all begin with a BEG control byte 0x0A):
 *   $ND500_TESTDATA/FraTor/test-real/test-real.nrf
 *   $ND500_TESTDATA/ND-500 Symbolic Debugger/debugger-b.nrf
 * NOTE: test/nc_fixtures/expected/*.NRF are NOT objects -
 *       they contain C SOURCE text and must not be used as golden references.
 *
 * ASSUME-NOTHING CAVEATS (not fully pinned by the manual, flagged for future work):
 *  - Numeric field byte order: the manual (Page 233) says the numeric field is
 *    "up to a 7-byte numeric value ... signed using 2's complement" but does not
 *    state endianness. ND-500 is big-endian, so we read MSB-first. If a real NRF
 *    desyncs on an LDN/LDI length, revisit this (marked NRF_NUM_BIG_ENDIAN below).
 *  - LDI(21): the NL "numeric" bytes ARE the payload loaded to (BP) (Page 239);
 *    we consume NL bytes but do not interpret them as a count.
 *  - LDN(33): numeric field (NL bytes) gives N, then N raw data bytes FOLLOW with
 *    no symbolic field (Page 241). We consume both.
 *  - REP(20) repeats the FOLLOWING group at load time; in the file the following
 *    group still appears once, so parsing is unaffected.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

/* Read the numeric field MSB-first (ND-500 big-endian). See caveat above. */
#define NRF_NUM_BIG_ENDIAN 1

/* ------------------------------------------------------------------ *
 * Control Field encoding.  Manual Page 233 ("NRF-FORMAT DESCRIPTION").
 *
 *   An NRF group = Control Field (mandatory)
 *                + Numeric Field  (NL bytes, 0..7)
 *                + Symbolic Field (SL byte + SL chars, only when the control
 *                                  number implies it).
 *
 *   Control Field is ONE byte:  [ 5-bit control number | 3-bit NL ]
 *     control number = byte >> 3   (high 5 bits, value 0..37 OCTAL = 0..31 dec)
 *     NL             = byte & 0x7   (low 3 bits, numeric length 0..7)
 *
 *   Verified against the manual's worked example (Page 233): the first LBB of a
 *   LBB vector is (LBB,4 <NULL> 0) and appears as control byte 304 OCTAL:
 *     304o = 0xC4 = 1100 0100b -> ctrl = 0xC4>>3 = 30 OCTAL (LBB), NL = 4.
 * ------------------------------------------------------------------ */
#define NRF_CTRL(byte)  ((uint8_t)((byte) >> 3))
#define NRF_NL(byte)    ((uint8_t)((byte) & 0x07))

/* ------------------------------------------------------------------ *
 * Control numbers.  Values are OCTAL exactly as printed in the manual's
 * "SUMMARY OF NRF-CONTROL NUMBERS" (Page 242, lines ~8438-8465). The 5-bit
 * field spans 0..37 octal (0..31 decimal).
 * ------------------------------------------------------------------ */
typedef enum {
    NRF_NUL = 000,  /* Group ignored; NL must be 0.               Page 234 */
    NRF_BEG = 001,  /* Begin module (priority/lang/addr-len/TMa). Page 234 */
    NRF_END = 002,  /* End module; NL = checksum size (0 or 2).   Page 236 */
    NRF_MSA = 003,  /* Main start address (= current BP).         Page 236 */
    NRF_LIB = 004,  /* (S) Library symbol.                        Page 236 */
    NRF_DEF = 005,  /* (S) Program symbol definition.             Page 237 */
    NRF_REF = 006,  /* (S) Program symbol reference.              Page 237 */
    NRF_LRF = 007,  /* (S) Library reference.                     Page 237 */
    NRF_DDF = 010,  /* (S) Data symbol definition.                Page 238 */
    NRF_DRF = 011,  /* (S) Data symbol reference.                 Page 238 */
    NRF_RMV = 012,  /* (S) Remove symbol.                         Page 238 */
    NRF_SLA = 013,  /* (S) Set load address (BP=).                Page 238 */
    NRF_AJS = 014,  /* Adjust (BP += N).                          Page 238 */
    NRF_PMO = 015,  /* Set program mode (BP=PP=PP+N).             Page 239 */
    NRF_DMO = 016,  /* Set data mode (BP=DP=DP+N).                Page 239 */
    NRF_FMO = 017,  /* (S) Set free mode.                         Page 239 */
    NRF_REP = 020,  /* Repeat following group N times.            Page 239 */
    NRF_LDI = 021,  /* Load NL immediate bytes to (BP).           Page 239 */
    NRF_ADI = 022,  /* Add numeric value into NL bytes at (BP).   Page 239 */
    NRF_APA = 023,  /* Add program address (PP+N -> 4 bytes).     Page 239 */
    NRF_ADA = 024,  /* Add data address (DP+N -> 4 bytes).        Page 240 */
    NRF_IHB = 025,  /* EXECUTION INHIBIT - NRF incomplete due to
                     * COMPILER ERRORS. Presence => object invalid. Page 240 */
    NRF_EOF = 026,  /* End of file.                               Page 240 */
    NRF_DBG = 027,  /* Debug info begin/end marker.               Page 240 */
    NRF_LBB = 030,  /* (S) Library module byte pointer (fast load vector). Page 240 */
    NRF_MSG = 031,  /* (S) Message - ASCII printed during load.   Page 240 */
    NRF_MIS = 032,  /* Miscellaneous; numeric value = subcontrol. Page 241 */
    NRF_LDN = 033,  /* Load N bytes immediately (N from numeric). Page 241 */
    NRF_IL1 = 034,  /* Illegal control number.                    Page 241/242 */
    NRF_IL2 = 035,  /* Illegal control number.                    Page 242 */
    NRF_IL3 = 036,  /* Illegal control number.                    Page 242 */
    NRF_IL4 = 037   /* Illegal control number.                    Page 242 */
} NrfControlNumber;

/* MIS(32) subcontrol numbers - the numeric value selects one. Page 241/243. */
typedef enum {
    NRF_MIS_CGR0 = 0,  /* Start of compound group.                */
    NRF_MIS_CGR1 = 1,  /* End of compound group (innermost nest). */
    NRF_MIS_ADD  = 2,  /* Add next reference symbol value at (BP).*/
    NRF_MIS_SUB  = 3,  /* Subtract next reference symbol value.   */
    NRF_MIS_MUL  = 4,  /* Multiply by next referenced value.      */
    NRF_MIS_DIV  = 5   /* Divide by next referenced value.        */
} NrfMisSubcontrol;

/* ------------------------------------------------------------------ *
 * BEG(1) numeric-field layout.  Manual Page 234-235 (lines ~8155-8214).
 * The numeric value's bytes (when present) mean, in order:
 * ------------------------------------------------------------------ */
typedef struct {
    uint8_t realtime_priority;  /* 1st byte                                    */
    uint8_t language_code;      /* 2nd byte - see NrfLanguage                  */
    uint8_t address_length;     /* 3rd byte - must be 4 if used                */
    uint8_t tma;                /* 4th byte - target machine/type (bits 765=machine,
                                 *            bit4 reserved, bits 3210=type)    */
    uint8_t osid;               /* 5th byte - Operating System id (see below)  */
} NrfBegHeader;

/* BEG 2nd byte - source language. Page 234 (lines ~8160-8171). */
typedef enum {
    NRF_LANG_ASSEMBLY = 0,
    NRF_LANG_FORTRAN  = 1,
    NRF_LANG_PLANC    = 2,
    NRF_LANG_COBOL    = 3,
    NRF_LANG_PASCAL   = 4,
    NRF_LANG_SIMULA   = 5,
    NRF_LANG_ADA      = 6,
    NRF_LANG_CORAL    = 7,
    NRF_LANG_C        = 8,   /* <- NC output should carry this */
    NRF_LANG_BASIC    = 9
} NrfLanguage;

/* BEG 4th byte (TMa) target-machine-type, when machine = 0 (Norsk Data).
 * Page 235 (lines ~8181-8185). */
#define NRF_TMA_MACHINE(tma)  (((tma) >> 5) & 0x07)  /* bits 765 */
#define NRF_TMA_TYPE(tma)     ((tma) & 0x0F)         /* bits 3210 */
#define NRF_TMA_MACHINE_NORSK_DATA 0
#define NRF_TMA_TYPE_ND500         1                 /* 0=not used, 1=ND-500(0) */

/* BEG 5th byte OSID ranges. Page 235 (lines ~8210-8214). */
#define NRF_OSID_IS_ND_OS(o)  ((o) <= 9)     /* 0-9  ND-OS (SINTRAN-III) */
#define NRF_OSID_IS_UNIX(o)   ((o) >= 10 && (o) <= 19)
#define NRF_OSID_IS_MSDOS(o)  ((o) >= 20 && (o) <= 29)

/* ------------------------------------------------------------------ *
 * Per-control-number descriptor: does it carry a symbolic field, and a name.
 * "(S)" groups (Page 234-240 + summary Page 242): LIB DEF REF LRF DDF DRF RMV
 * SLA FMO LBB MSG. All others carry only the numeric field, EXCEPT LDN(33)
 * which is followed by N raw data bytes (handled specially).
 * ------------------------------------------------------------------ */
typedef struct {
    const char* mnemonic;
    int         has_symbolic;   /* 1 if a Symbolic Field (SL + name) follows the numeric field */
} NrfCtrlInfo;

static const NrfCtrlInfo NRF_INFO[32] = {
    [NRF_NUL] = {"NUL", 0}, [NRF_BEG] = {"BEG", 0}, [NRF_END] = {"END", 0},
    [NRF_MSA] = {"MSA", 0}, [NRF_LIB] = {"LIB", 1}, [NRF_DEF] = {"DEF", 1},
    [NRF_REF] = {"REF", 1}, [NRF_LRF] = {"LRF", 1}, [NRF_DDF] = {"DDF", 1},
    [NRF_DRF] = {"DRF", 1}, [NRF_RMV] = {"RMV", 1}, [NRF_SLA] = {"SLA", 1},
    [NRF_AJS] = {"AJS", 0}, [NRF_PMO] = {"PMO", 0}, [NRF_DMO] = {"DMO", 0},
    [NRF_FMO] = {"FMO", 1}, [NRF_REP] = {"REP", 0}, [NRF_LDI] = {"LDI", 0},
    [NRF_ADI] = {"ADI", 0}, [NRF_APA] = {"APA", 0}, [NRF_ADA] = {"ADA", 0},
    [NRF_IHB] = {"IHB", 0}, [NRF_EOF] = {"EOF", 0}, [NRF_DBG] = {"DBG", 0},
    [NRF_LBB] = {"LBB", 1}, [NRF_MSG] = {"MSG", 1}, [NRF_MIS] = {"MIS", 0},
    [NRF_LDN] = {"LDN", 0}, [NRF_IL1] = {"IL1", 0}, [NRF_IL2] = {"IL2", 0},
    [NRF_IL3] = {"IL3", 0}, [NRF_IL4] = {"IL4", 0}
};

/* ------------------------------------------------------------------ *
 * Walk state + accumulated validation flags.
 * ------------------------------------------------------------------ */
typedef struct {
    const uint8_t* buf;
    size_t         len;
    size_t         pos;
    int            dump;
    /* observations */
    int  begin_count;    /* BEG seen */
    int  end_count;      /* END seen */
    int  eof_seen;       /* EOF seen */
    int  ihb_seen;       /* execution-inhibit => compiler errors => INVALID */
    int  illegal_seen;   /* IL1..IL4 => INVALID */
    int  in_module;      /* inside a BEG..END pair */
    int  nesting_error;  /* BEG nested inside BEG (illegal, Page 235) */
    int  code_groups;    /* LDN/LDI/ADI/APA/ADA -> object carries payload */
    int  sym_groups;     /* DEF/DDF/LIB -> object defines symbols */
    int  language;       /* language_code from first BEG (-1 = unknown) */
    int  truncated;      /* ran off the end mid-group */
} NrfWalk;

/* Read an unsigned value from NL numeric bytes (MSB-first). Returns 0 on
 * truncation and sets w->truncated. Also used as a byte-count for LDN. */
static uint64_t read_numeric(NrfWalk* w, uint8_t nl) {
    if (w->pos + nl > w->len) { w->truncated = 1; w->pos = w->len; return 0; }
    uint64_t v = 0;
#if NRF_NUM_BIG_ENDIAN
    for (uint8_t i = 0; i < nl; i++) v = (v << 8) | w->buf[w->pos + i];
#else
    for (int i = (int)nl - 1; i >= 0; i--) v = (v << 8) | w->buf[w->pos + i];
#endif
    w->pos += nl;
    return v;
}

/* Consume the symbolic field: 1 SL byte + SL chars. Copies name (truncated) out. */
static void read_symbolic(NrfWalk* w, char* name, size_t name_sz) {
    name[0] = '\0';
    if (w->pos >= w->len) { w->truncated = 1; return; }
    uint8_t sl = w->buf[w->pos++];
    if (w->pos + sl > w->len) { w->truncated = 1; w->pos = w->len; return; }
    size_t n = (sl < name_sz - 1) ? sl : name_sz - 1;
    memcpy(name, &w->buf[w->pos], n);
    name[n] = '\0';
    w->pos += sl;
}

static void skip_bytes(NrfWalk* w, uint64_t n) {
    if (w->pos + n > w->len) { w->truncated = 1; w->pos = w->len; return; }
    w->pos += (size_t)n;
}

/* Walk the whole stream. Returns when EOF group seen or input exhausted. */
static void nrf_walk(NrfWalk* w) {
    w->language = -1;
    while (w->pos < w->len && !w->eof_seen && !w->truncated) {
        size_t group_off = w->pos;
        uint8_t cf = w->buf[w->pos++];
        uint8_t ctrl = NRF_CTRL(cf);
        uint8_t nl   = NRF_NL(cf);
        const NrfCtrlInfo* info = &NRF_INFO[ctrl];

        /* NUL requires NL==0 (Page 234). */
        if (ctrl == NRF_NUL && nl != 0) w->illegal_seen = 1;

        /* Numeric field first. For LDI the NL bytes are payload (still skipped
         * via read_numeric's advance); for LDN NL bytes give the follow count. */
        uint64_t num = read_numeric(w, nl);

        /* Symbolic field, if this control number implies one. */
        char name[256] = {0};
        if (info->has_symbolic) read_symbolic(w, name, sizeof(name));

        /* LDN(33): N raw data bytes follow the numeric field (Page 241). */
        uint64_t extra = 0;
        if (ctrl == NRF_LDN) { extra = num; skip_bytes(w, extra); }

        /* Classify / accumulate. */
        switch (ctrl) {
            case NRF_BEG:
                if (w->in_module) w->nesting_error = 1;
                w->in_module = 1; w->begin_count++;
                if (w->language < 0 && nl >= 2)  /* 2nd numeric byte = language */
                    w->language = (int)((num >> (8 * (nl - 2))) & 0xFF);
                break;
            case NRF_END: w->in_module = 0; w->end_count++; break;
            case NRF_EOF: w->eof_seen = 1; break;
            case NRF_IHB: w->ihb_seen = 1; break;
            case NRF_IL1: case NRF_IL2: case NRF_IL3: case NRF_IL4:
                w->illegal_seen = 1; break;
            case NRF_DEF: case NRF_DDF: case NRF_LIB: w->sym_groups++; break;
            case NRF_LDN: case NRF_LDI: case NRF_ADI:
            case NRF_APA: case NRF_ADA: w->code_groups++; break;
            default: break;
        }

        if (w->dump) {
            printf("  @%06zu  %-3s (ctrl=%02o NL=%u) num=%" PRIu64,
                   group_off, info->mnemonic, ctrl, nl, num);
            if (info->has_symbolic) printf(" sym=\"%s\"", name);
            if (ctrl == NRF_LDN)    printf(" +%" PRIu64 " data bytes", extra);
            putchar('\n');
        }
    }
}

static uint8_t* slurp(const char* path, size_t* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) { perror(path); return NULL; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    uint8_t* b = malloc((size_t)n + 1);
    if (!b) { fclose(f); return NULL; }
    size_t got = fread(b, 1, (size_t)n, f);
    fclose(f);
    *out_len = got;
    return b;
}

int main(int argc, char** argv) {
    int dump = 0;
    const char* path = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) dump = 1;
        else path = argv[i];
    }
    if (!path) {
        fprintf(stderr, "usage: %s [-d] <file.nrf>\n"
                        "  -d  dump every control group\n", argv[0]);
        return 2;
    }

    size_t len = 0;
    uint8_t* buf = slurp(path, &len);
    if (!buf) return 2;

    NrfWalk w = {0};
    w.buf = buf; w.len = len; w.dump = dump;

    if (dump) printf("NRF dump of %s (%zu bytes):\n", path, len);
    nrf_walk(&w);
    free(buf);

    /* A valid, COMPLETE object: has >=1 BEG..END module with BALANCED BEG/END,
     * carries no IHB (compiler-error inhibit) and no illegal control numbers,
     * did not run off the end, and had no BEG nesting. (Manual Pages 234-242.)
     *
     * NOTE: EOF(26) is NOT required. Verified against genuine objects - both
     * $ND500_TESTDATA/FraTor/test-real/test-real.nrf (1 module) and
     * $ND500_TESTDATA/ND-500 Symbolic Debugger/debugger-b.nrf (23-module library)
     * simply END and stop; neither emits an EOF group. EOF presence is reported
     * as informational only, not a validity requirement. */
    int valid = (w.begin_count > 0) && (w.end_count == w.begin_count) &&
                !w.ihb_seen && !w.illegal_seen && !w.truncated && !w.nesting_error;

    printf("\n=== NRF validation: %s ===\n", path);
    printf("  BEG modules : %d\n", w.begin_count);
    printf("  END modules : %d\n", w.end_count);
    printf("  EOF group   : %s (informational; not required)\n",
           w.eof_seen ? "yes" : "absent");
    printf("  symbol defs : %d (DEF/DDF/LIB)\n", w.sym_groups);
    printf("  code groups : %d (LDN/LDI/ADI/APA/ADA)\n", w.code_groups);
    if (w.language >= 0)
        printf("  language    : %d%s\n", w.language,
               w.language == NRF_LANG_C ? " (C)" : "");
    if (w.ihb_seen)      printf("  ** IHB present: NRF INCOMPLETE due to compiler errors\n");
    if (w.illegal_seen)  printf("  ** illegal control number encountered\n");
    if (w.truncated)     printf("  ** truncated / ran off end mid-group\n");
    if (w.nesting_error) printf("  ** illegal BEG nesting\n");
    printf("  RESULT      : %s\n", valid ? "VALID + COMPLETE" : "INVALID / INCOMPLETE");

    return valid ? 0 : 1;
}
