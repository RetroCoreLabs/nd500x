/**
 * ND500 Double Register (Dn = An:En) Test
 *
 * Dn is the pair (An, En) with An the HIGH half and En the low half.
 * nd500_read_double_register / nd500_write_double_register used to pack it
 * the other way round (An = low, En = high).
 *
 * Primary evidence for An = high (1988): the dbx opcode table
 * ucb/dbx/ops.nd500 gives "neg f" and "neg d" the SAME octal opcode 0224
 * (0x94) - one instruction serves both widths. That is only possible if a
 * single and a double keep their sign bit in the SAME physical register,
 * i.e. An. The data formats agree: an ND-500 single (sign|exp9|mant22) is
 * bit-for-bit the top 32 bits of an ND-500 double (sign|exp9|mant54) -
 * confirmed against the 1988 libc, where genlib member atof.o stores 10.0 as
 * 0x4110000000000000 while ".float 10.0" assembles to 0x41100000.
 *
 * NOTE: the repository's docs/architecture/ND500_REGISTERS.md still describes
 * the OLD (wrong) convention. It is not a primary source; the 1988 opcode
 * table and the 1988 libc data are.
 *
 * The failure the swap caused: "neg" on a negative double flipped a mantissa
 * bit instead of the sign bit, so libc's iszero_d() never took an absolute
 * value and printf("%f") printed a 170-digit run of 1000...000 for every
 * double.
 *
 * ND-500 double format (Reference Manual 2.5.3.6):
 *   (-1)^S * 2^(E-256) * 0.1F
 *   bit 63     sign
 *   bits 62-54 9-bit exponent, bias 256
 *   bits 53-0  54-bit mantissa
 *   exponent field 0 means exactly zero
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/instruction_helpers.h"

#define MEMORY_SIZE (64 * 1024)

/* NEG lives in src/cpu/instructions/ARITHMETIC/Neg.c and is dispatched from
 * the opcode table; it has no public header. */
void nd500_instr_Neg(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi);

static int tests_passed = 0;
static int tests_failed = 0;

static void test_result(const char* name, bool passed, const char* details) {
    if (passed) {
        tests_passed++;
        printf("  [PASS] %s\n", name);
    } else {
        tests_failed++;
        printf("  [FAIL] %s: %s\n", name, details);
    }
}

/* ------------------------------------------------------------------------
 * (a) VALIDATION - round-trip through the accessors
 * ------------------------------------------------------------------------ */
static void test_roundtrip(Nd500Cpu* cpu) {
    printf("\n=== Dn write/read round-trip ===\n");

    static const uint64_t values[] = {
        0x0000000000000000ULL,
        0xFFFFFFFFFFFFFFFFULL,
        0x0123456789ABCDEFULL,
        0x4110000000000000ULL,      /* 10.0, 1988 libc genlib/atof.o */
        0xC100004000000000ULL,      /* the neg regression operand (low half zero) */
        0x00000000DEADBEEFULL,      /* only the low half set */
        0xDEADBEEF00000000ULL       /* only the high half set */
    };

    for (uint8_t reg = 1; reg <= 4; reg++) {
        for (unsigned i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
            nd500_write_double_register(cpu, reg, values[i]);
            uint64_t back = nd500_read_double_register(cpu, reg);

            char name[96], d[160];
            snprintf(name, sizeof(name), "D%u round-trip 0x%016llX",
                     reg, (unsigned long long)values[i]);
            snprintf(d, sizeof(d), "got 0x%016llX", (unsigned long long)back);
            test_result(name, back == values[i], d);
        }
    }
}

/* ------------------------------------------------------------------------
 * (a) VALIDATION - An is the HIGH half, En is the low half
 * ------------------------------------------------------------------------ */
static void test_half_placement(Nd500Cpu* cpu) {
    printf("\n=== An holds the HIGH half, En the low half ===\n");

    for (uint8_t reg = 1; reg <= 4; reg++) {
        /* write -> inspect the raw registers */
        nd500_write_double_register(cpu, reg, 0x0123456789ABCDEFULL);

        char name[96], d[160];
        snprintf(name, sizeof(name), "write D%u: A%u == high half, E%u == low half", reg, reg, reg);
        snprintf(d, sizeof(d), "A%u=0x%08X (expected 0x01234567), E%u=0x%08X (expected 0x89ABCDEF)",
                 reg, cpu->A[reg - 1], reg, cpu->E[reg - 1]);
        test_result(name,
                    cpu->A[reg - 1] == 0x01234567u && cpu->E[reg - 1] == 0x89ABCDEFu,
                    d);

        /* raw registers -> read */
        cpu->A[reg - 1] = 0xAAAAAAAAu;
        cpu->E[reg - 1] = 0xEEEEEEEEu;
        uint64_t v = nd500_read_double_register(cpu, reg);

        snprintf(name, sizeof(name), "read D%u: A%u:E%u assembles high:low", reg, reg, reg);
        snprintf(d, sizeof(d), "got 0x%016llX, expected 0xAAAAAAAAEEEEEEEE",
                 (unsigned long long)v);
        test_result(name, v == 0xAAAAAAAAEEEEEEEEULL, d);
    }
}

/* ------------------------------------------------------------------------
 * (a) VALIDATION - an ND-500 single is the top 32 bits of the double
 *
 * Ground truth, 1988: the libc genlib member atof.o stores 10.0 as
 * 0x4110000000000000 and the assembler emits ".float 10.0" as 0x41100000.
 * ------------------------------------------------------------------------ */
static void test_single_is_top_half_of_double(Nd500Cpu* cpu) {
    printf("\n=== ND-500 single == top 32 bits of the ND-500 double (1988 ground truth) ===\n");

    static const struct { const char* name; uint64_t dbl; uint32_t sgl; } pairs[] = {
        { "10.0", 0x4110000000000000ULL, 0x41100000u },
        { "1.0",  0x4040000000000000ULL, 0x40400000u }
    };

    for (unsigned i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++) {
        uint64_t d = pairs[i].dbl;
        uint32_t f = pairs[i].sgl;

        {   /* the bit relation itself */
            char name[128], det[160];
            snprintf(name, sizeof(name), "%s: single 0x%08X == top 32 bits of double 0x%016llX",
                     pairs[i].name, f, (unsigned long long)d);
            snprintf(det, sizeof(det), "top half is 0x%08X", (uint32_t)(d >> 32));
            test_result(name, (uint32_t)(d >> 32) == f, det);
        }

        {   /* the double half lands in An, i.e. in the same register a single
             * of the same value occupies - which is why one NEG opcode serves
             * both widths */
            nd500_write_double_register(cpu, 1, d);
            char name[128], det[160];
            snprintf(name, sizeof(name), "%s: A1 after writing D1 equals the single bit pattern",
                     pairs[i].name);
            snprintf(det, sizeof(det), "A1=0x%08X, expected 0x%08X", cpu->A[0], f);
            test_result(name, cpu->A[0] == f, det);
        }

        {   /* the emulator's own format converters agree on the value */
            double dv = nd500_double_to_ieee754(d);
            float  fv = nd500_float_to_ieee754(f);
            char name[128], det[160];
            snprintf(name, sizeof(name), "%s: double and single decode to the same value",
                     pairs[i].name);
            snprintf(det, sizeof(det), "double=%.17g single=%.17g", dv, (double)fv);
            test_result(name, dv == (double)fv, det);
        }
    }
}

/* ------------------------------------------------------------------------
 * (b) REGRESSION - "neg" on a double must flip the SIGN bit (bug 3a6c75f)
 *
 * Operand 0xC100004000000000: its LOW 32 bits are ZERO, which is exactly why
 * the old An=low/En=high packing made the negation a silent no-op on the half
 * that mattered. With the halves swapped, NEG read 0x00000000C1000040, XORed
 * bit 63 of THAT (a mantissa bit of the real value) and wrote it back as
 * A1=0xC1000040 (unchanged!), E1=0x80000000 (garbage). libc's iszero_d() then
 * never took an absolute value and printf("%f") printed a 170-digit run of
 * 1000...000 for every double.
 *
 * The raw registers are seeded directly (NOT through the accessors) so the
 * setup cannot hide the bug it is testing.
 * ------------------------------------------------------------------------ */
static void run_neg_double(Nd500Cpu* cpu, uint8_t reg) {
    Nd500FetchedInstruction fi;
    memset(&fi, 0, sizeof(fi));
    fi.address = 0x1000;
    fi.opcode = 0x0094;                 /* NEG, the opcode shared by "neg f"/"neg d" */
    fi.opcode_len = 1;
    fi.mnemonic = "neg";
    fi.operand_count = 0;               /* register-only operation */
    fi.target_register = reg;
    fi.data_type = ND500_DTYPE_DOUBLEWORD;
    fi.uses_float_registers = true;

    nd500_instr_Neg(cpu, &fi);
}

static void test_regression_neg_double(Nd500Cpu* cpu) {
    printf("\n=== REGRESSION: D NEG flips the sign bit, not a mantissa bit (bug 3a6c75f) ===\n");

    const uint32_t start_high = 0xC1000040u;    /* An = high half */
    const uint32_t start_low  = 0x00000000u;    /* En = low half  */

    cpu->A[0] = start_high;
    cpu->E[0] = start_low;

    printf("  (informational) operand 0xC100004000000000 decodes to %.17g\n",
           nd500_double_to_ieee754(0xC100004000000000ULL));

    run_neg_double(cpu, 1);

    {
        char d[192];
        snprintf(d, sizeof(d),
                 "A1=0x%08X E1=0x%08X -> D1=0x%016llX, expected 0x4100004000000000 "
                 "(old code left A1 at 0xC1000040 and put 0x80000000 in E1)",
                 cpu->A[0], cpu->E[0],
                 (unsigned long long)(((uint64_t)cpu->A[0] << 32) | cpu->E[0]));
        test_result("neg 0xC100004000000000 -> 0x4100004000000000",
                    cpu->A[0] == 0x41000040u && cpu->E[0] == 0x00000000u, d);
    }

    run_neg_double(cpu, 1);

    {
        char d[192];
        snprintf(d, sizeof(d), "A1=0x%08X E1=0x%08X, expected 0x%08X 0x%08X",
                 cpu->A[0], cpu->E[0], start_high, start_low);
        test_result("negating twice returns the original value",
                    cpu->A[0] == start_high && cpu->E[0] == start_low, d);
    }

    /* The same must hold for a positive operand and for the other registers. */
    for (uint8_t reg = 1; reg <= 4; reg++) {
        cpu->A[reg - 1] = 0x41000040u;
        cpu->E[reg - 1] = 0x00000000u;
        run_neg_double(cpu, reg);

        char name[96], d[160];
        snprintf(name, sizeof(name), "D%u NEG of a positive double sets the sign bit in A%u", reg, reg);
        snprintf(d, sizeof(d), "A%u=0x%08X E%u=0x%08X, expected 0xC1000040 0x00000000",
                 reg, cpu->A[reg - 1], reg, cpu->E[reg - 1]);
        test_result(name,
                    cpu->A[reg - 1] == 0xC1000040u && cpu->E[reg - 1] == 0x00000000u, d);
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("ND500 Double Register (Dn = An:En) Tests\n");
    printf("========================================\n");

    nd500_quiet = 1;

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);

    test_roundtrip(&cpu);
    test_half_placement(&cpu);
    test_single_is_top_half_of_double(&cpu);
    test_regression_neg_double(&cpu);

    nd500_machine_free(&m);

    printf("\n=== Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
