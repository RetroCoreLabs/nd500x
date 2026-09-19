/*
 * test_corpus_coverage.c - coverage ratchet for the ND500 Conformance Corpus
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * Measures how much of the instruction set test/nd500-conformance.json
 * exercises, along four dimensions that RetroCore's trap-condition gate
 * (Emulated.Tests.ND500/Validation/TestND500_TrapCoverage.cs) does not see:
 *
 *   instructions  instructions (mnemonics) and implemented opcodes with at
 *                 least one vector;
 *   flag seeding  instructions whose vectors start with each data flag
 *                 (Z, C, S, O, K) both clear and set. With only one of the
 *                 two, an instruction that must preserve a flag and one that
 *                 clears it produce the same results;
 *   trap enables  instructions with a vector that seeds a trap-enable
 *                 register (ote1/ote2). Without one, an enabled ignorable
 *                 trap is never exercised;
 *   float range   float instructions with an operand register near the top
 *                 (exponent field >= 500 of 511) and near the bottom
 *                 (exponent field 1..10) of the native range.
 *
 * Only positive vectors count; isNegativeTest vectors are skipped. Every
 * vector is decoded with the emulator's own decoder (nd500_decode_at), so the
 * opcode, mnemonic and float class are exactly what the CPU sees. Float
 * operands are judged from the An registers only (An is also the
 * sign/exponent half of Dn); operands in memory are not examined, so the two
 * float counts can only under-report.
 *
 * Every count must EQUAL its baseline below. Lower is a coverage regression.
 * Higher means the corpus improved: raise the baseline in the same commit so
 * the new level cannot be lost again. Every hole is printed by name
 * (the first 20 per list; -v prints all of them).
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cjson/cJSON.h>

#include "conformance_corpus.h"
#include "cpu_protos.h"
#include "instruction_helpers.h"
#include "machine_protos.h"
#include "nd500_instructions.h"

// clang-format off
/* Baselines, measured on the 76,120-vector corpus of 18-SEP-2026 (flag-preset
 * twins included). See the file header for when and how to change them. */
enum
{
    BASE_INSTRUCTIONS = 236,
    BASE_OPCODES      = 482,
    BASE_SEED_Z       = 106,
    BASE_SEED_C       = 104,
    BASE_SEED_S       = 103,
    BASE_SEED_O       = 96,
    BASE_SEED_K       = 96,
    BASE_TRAP_ENABLE  = 2,
    BASE_FLOAT_HIGH   = 19,
    BASE_FLOAT_LOW    = 19
};
// clang-format on

enum
{
    MAX_MNEMONICS     = 1024,
    SCRATCH_MEMORY    = 1 << 20,
    SCRATCH_PC        = 0x1000,
    FLOAT_HIGH_EFIELD = 500,
    FLOAT_LOW_EFIELD  = 10,
    SHOWN_PER_LIST    = 20
};

/* The data flags whose seeding is measured, in report order. */
static const struct
{
    const char *name;
    uint32_t mask;
} s_flags[] = {
    {"Z", ND500_FLAG_Z}, {"C", ND500_FLAG_C}, {"S", ND500_FLAG_S},
    {"O", ND500_FLAG_O}, {"K", ND500_FLAG_K},
};
enum
{
    FLAG_COUNT = sizeof s_flags / sizeof s_flags[0]
};

typedef struct
{
    const char *name;     /* mnemonic, as the instruction table spells it */
    bool is_float;        /* at least one opcode decodes with float registers */
    bool covered;         /* at least one positive vector */
    uint32_t seen_set;    /* flag masks some vector starts with set */
    uint32_t seen_clear;  /* flag masks some vector starts with clear */
    bool trap_enable;     /* some vector seeds ote1 or ote2 */
    bool float_high;      /* some float vector has an An exponent >= 500 */
    bool float_low;       /* some float vector has an An exponent 1..10 */
} MnemonicCoverage;

static MnemonicCoverage s_mnem[MAX_MNEMONICS];
static int s_mnem_count;
static bool s_implemented[65536];
static bool s_covered[65536];

static MnemonicCoverage *find_mnemonic(const char *name)
{
    for (int i = 0; i < s_mnem_count; i++)
    {
        if (strcmp(s_mnem[i].name, name) == 0)
        {
            return &s_mnem[i];
        }
    }
    return NULL;
}

/* Decode the bytes at SCRATCH_PC with the emulator's decoder. */
static int decode_bytes(Nd500Machine *m, const uint8_t *bytes, int len,
                        Nd500FetchedInstruction *fi)
{
    for (int i = 0; i < 32; i++)
    {
        nd500_bus_write8(m, SCRATCH_PC + (uint32_t)i, (uint8_t)(i < len ? bytes[i] : 0));
    }
    memset(fi, 0, sizeof *fi);
    return nd500_decode_at(m, SCRATCH_PC, fi);
}

/* The universe: every implemented opcode in the instruction table, grouped by
 * mnemonic, with each mnemonic's float class taken from the decoder. */
static int build_universe(Nd500Machine *m)
{
    for (unsigned i = 0; i < g_nd500_instrs_count; i++)
    {
        uint16_t op = g_nd500_instrs[i].opcode;
        if (g_instr_exec_table[op] == NULL)
        {
            continue;
        }
        s_implemented[op] = true;
        MnemonicCoverage *mc = find_mnemonic(g_nd500_instrs[i].mnemonic);
        if (mc == NULL)
        {
            if (s_mnem_count == MAX_MNEMONICS)
            {
                fprintf(stderr, "more than %d mnemonics - raise MAX_MNEMONICS\n",
                        MAX_MNEMONICS);
                return -1;
            }
            mc = &s_mnem[s_mnem_count++];
            memset(mc, 0, sizeof *mc);
            mc->name = g_nd500_instrs[i].mnemonic;
        }
        uint8_t code[2] = {(uint8_t)(op >> 8), (uint8_t)op};
        Nd500FetchedInstruction fi;
        if (op < 0x100)
        {
            code[0] = (uint8_t)op;
        }
        if (decode_bytes(m, code, op < 0x100 ? 1 : 2, &fi) == 0 && fi.uses_float_registers)
        {
            mc->is_float = true;
        }
    }
    return 0;
}

static bool has_number(const cJSON *obj, const char *key, uint32_t *out)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsNumber(item))
    {
        return false;
    }
    if (out != NULL)
    {
        *out = (uint32_t)item->valuedouble;
    }
    return true;
}

/* Fold one positive vector into the per-mnemonic records. Returns false if
 * the vector could not be decoded. */
static bool account_vector(Nd500Machine *m, const cJSON *test)
{
    const cJSON *bytes = cJSON_GetObjectItemCaseSensitive(test, "bytes");
    const cJSON *initial = cJSON_GetObjectItemCaseSensitive(test, "initial");
    const cJSON *regs = cJSON_GetObjectItemCaseSensitive(initial, "regs");
    if (!cJSON_IsArray(bytes) || !cJSON_IsObject(regs))
    {
        return false;
    }
    uint8_t code[32];
    int len = 0;
    const cJSON *b;
    cJSON_ArrayForEach(b, bytes)
    {
        if (len < (int)sizeof code)
        {
            code[len++] = (uint8_t)b->valuedouble;
        }
    }
    Nd500FetchedInstruction fi;
    if (len == 0 || decode_bytes(m, code, len, &fi) != 0 || fi.mnemonic == NULL)
    {
        return false;
    }
    MnemonicCoverage *mc = find_mnemonic(fi.mnemonic);
    if (mc == NULL)
    {
        return false;
    }
    mc->covered = true;
    s_covered[fi.opcode] = true;

    uint32_t st = 0;
    (void)has_number(regs, "st", &st);   /* absent means 0 */
    for (int f = 0; f < FLAG_COUNT; f++)
    {
        if ((st & s_flags[f].mask) != 0u)
        {
            mc->seen_set |= s_flags[f].mask;
        }
        else
        {
            mc->seen_clear |= s_flags[f].mask;
        }
    }
    if (has_number(regs, "ote1", NULL) || has_number(regs, "ote2", NULL))
    {
        mc->trap_enable = true;
    }
    if (fi.uses_float_registers)
    {
        static const char *const an[] = {"a1", "a2", "a3", "a4"};
        for (int r = 0; r < 4; r++)
        {
            uint32_t v;
            if (!has_number(regs, an[r], &v))
            {
                continue;
            }
            uint32_t e = (v & ND500_FLOAT_EXPONENT_MASK) >> ND500_FLOAT_EXPONENT_SHIFT;
            if (e >= FLOAT_HIGH_EFIELD)
            {
                mc->float_high = true;
            }
            if (e >= 1u && e <= FLOAT_LOW_EFIELD)
            {
                mc->float_low = true;
            }
        }
    }
    return true;
}

/* Print the mnemonics a predicate selects, wrapped, capped unless verbose. */
static void print_holes(const char *title, bool (*is_hole)(const MnemonicCoverage *, uint32_t),
                        uint32_t arg, bool verbose)
{
    int n = 0;
    for (int i = 0; i < s_mnem_count; i++)
    {
        if (is_hole(&s_mnem[i], arg))
        {
            n++;
        }
    }
    printf("\n%s: %d\n", title, n);
    int shown = 0;
    int col = 0;
    for (int i = 0; i < s_mnem_count && n > 0; i++)
    {
        if (!is_hole(&s_mnem[i], arg))
        {
            continue;
        }
        if (!verbose && shown == SHOWN_PER_LIST)
        {
            printf("%s... %d more (-v lists all)", col ? " " : "  ", n - shown);
            break;
        }
        int w = (int)strlen(s_mnem[i].name) + 1;
        if (col + w > 96)
        {
            printf("\n");
            col = 0;
        }
        printf("%s%s", col ? " " : "  ", s_mnem[i].name);
        col += w + (col ? 0 : 2);
        shown++;
    }
    if (n > 0)
    {
        printf("\n");
    }
}

static bool hole_uncovered(const MnemonicCoverage *mc, uint32_t unused)
{
    (void)unused;
    return !mc->covered;
}

static bool hole_flag(const MnemonicCoverage *mc, uint32_t mask)
{
    return mc->covered && !((mc->seen_set & mask) && (mc->seen_clear & mask));
}

static bool hole_float_high(const MnemonicCoverage *mc, uint32_t unused)
{
    (void)unused;
    return mc->is_float && !mc->float_high;
}

static bool hole_float_low(const MnemonicCoverage *mc, uint32_t unused)
{
    (void)unused;
    return mc->is_float && !mc->float_low;
}

/* Compare one count with its baseline; print the line; return 1 on mismatch. */
static int check(const char *what, int value, int total, int baseline)
{
    const char *verdict = value == baseline ? "ok"
                          : value < baseline ? "REGRESSION"
                                             : "IMPROVED - raise the baseline";
    printf("  %-52s %5d / %-5d baseline %5d  %s\n", what, value, total, baseline, verdict);
    return value == baseline ? 0 : 1;
}

int main(int argc, char **argv)
{
    bool verbose = argc > 1 && (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--verbose") == 0);

    cJSON *root = conformance_corpus_load(argv[0], "nd500-conformance.json");
    if (root == NULL)
    {
        return 1;
    }

    Nd500Machine machine;
    memset(&machine, 0, sizeof machine);
    nd500_machine_init(&machine, SCRATCH_MEMORY);
    if (build_universe(&machine) != 0)
    {
        cJSON_Delete(root);
        return 1;
    }

    int positive = 0;
    int negative = 0;
    int undecodable = 0;
    const cJSON *test;
    cJSON_ArrayForEach(test, root)
    {
        if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(test, "isNegativeTest")))
        {
            negative++;
            continue;
        }
        positive++;
        if (!account_vector(&machine, test))
        {
            undecodable++;
        }
    }
    cJSON_Delete(root);

    int n_cov = 0, n_float = 0, n_te = 0, n_fh = 0, n_fl = 0, n_ops = 0, n_ops_cov = 0;
    int n_seed[FLAG_COUNT] = {0};
    for (int i = 0; i < s_mnem_count; i++)
    {
        const MnemonicCoverage *mc = &s_mnem[i];
        n_cov += mc->covered;
        n_float += mc->is_float;
        n_te += mc->covered && mc->trap_enable;
        n_fh += mc->is_float && mc->float_high;
        n_fl += mc->is_float && mc->float_low;
        for (int f = 0; f < FLAG_COUNT; f++)
        {
            n_seed[f] += !hole_flag(mc, s_flags[f].mask) && mc->covered;
        }
    }
    for (int op = 0; op < 65536; op++)
    {
        n_ops += s_implemented[op];
        n_ops_cov += s_implemented[op] && s_covered[op];
    }

    printf("\nND500 Conformance Corpus coverage: %d positive vectors (%d negative skipped, "
           "%d undecodable)\n\n", positive, negative, undecodable);
    int bad = 0;
    bad += check("instructions with at least one vector", n_cov, s_mnem_count, BASE_INSTRUCTIONS);
    bad += check("implemented opcodes with at least one vector", n_ops_cov, n_ops, BASE_OPCODES);
    static const int base_seed[FLAG_COUNT] = {BASE_SEED_Z, BASE_SEED_C, BASE_SEED_S,
                                              BASE_SEED_O, BASE_SEED_K};
    for (int f = 0; f < FLAG_COUNT; f++)
    {
        char what[64];
        snprintf(what, sizeof what, "instructions seeding %s both clear and set", s_flags[f].name);
        bad += check(what, n_seed[f], n_cov, base_seed[f]);
    }
    bad += check("instructions with a trap-enable (ote1/ote2) vector", n_te, n_cov,
                 BASE_TRAP_ENABLE);
    bad += check("float instructions with an operand exponent >= 500", n_fh, n_float,
                 BASE_FLOAT_HIGH);
    bad += check("float instructions with an operand exponent 1..10", n_fl, n_float,
                 BASE_FLOAT_LOW);

    print_holes("Instructions with no vector", hole_uncovered, 0, verbose);
    for (int f = 0; f < FLAG_COUNT; f++)
    {
        char title[80];
        snprintf(title, sizeof title, "Covered instructions never seeding %s both ways",
                 s_flags[f].name);
        print_holes(title, hole_flag, s_flags[f].mask, verbose);
    }
    print_holes("Float instructions without an operand exponent >= 500", hole_float_high, 0,
                verbose);
    print_holes("Float instructions without an operand exponent 1..10", hole_float_low, 0,
                verbose);

    nd500_machine_free(&machine);
    if (undecodable > 0)
    {
        printf("\nFAIL: %d positive vectors did not decode\n", undecodable);
        return 1;
    }
    printf("\n%s\n", bad ? "FAIL: coverage differs from the baselines above"
                         : "PASS: coverage matches every baseline");
    return bad ? 1 : 0;
}
