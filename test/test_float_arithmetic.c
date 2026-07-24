/**
 * ND500 Float Arithmetic Test
 *
 * Tests floating point instructions:
 * - FCONV (W FCONV): Integer to float conversion
 * - WCONV (F WCONV): Float to integer conversion
 * - Fn + : Float addition
 * - Fn * : Float multiplication
 * - Fn / : Float division
 *
 * Test pattern:
 * 1. Load integer value into W1 (I1)
 * 2. Convert W1 to float -> F1 (W FCONV)
 * 3. Perform float arithmetic (add/multiply/divide)
 * 4. Convert F1 back to integer -> W1 (F WCONV)
 * 5. Verify result
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/instruction_helpers.h"

/* Test memory size */
#define MEMORY_SIZE (64 * 1024)  /* 64KB */

/* Test result tracking */
static int tests_passed = 0;
static int tests_failed = 0;

/* Helper to print test result */
static void test_result(const char* name, bool passed, const char* details) {
    if (passed) {
        tests_passed++;
        printf("  [PASS] %s\n", name);
    } else {
        tests_failed++;
        printf("  [FAIL] %s: %s\n", name, details);
    }
}

/* Test integer to float and back conversion */
static void test_int_to_float_roundtrip(void) {
    printf("\n=== Int to Float Round-trip Tests ===\n");

    int32_t test_values[] = {0, 1, -1, 100, -100, 1000, -1000, 1000000, -1000000};
    int num_tests = sizeof(test_values) / sizeof(test_values[0]);

    for (int i = 0; i < num_tests; i++) {
        int32_t val = test_values[i];
        uint32_t nd_float = nd500_float_from_int32(val);
        int32_t back = nd500_float_to_int32(nd_float);

        char name[64];
        snprintf(name, sizeof(name), "roundtrip(%d)", val);

        char details[128];
        snprintf(details, sizeof(details), "expected %d, got %d (float bits: 0x%08X)", val, back, nd_float);

        test_result(name, val == back, details);
    }
}

/* Test float to IEEE754 conversion and back */
static void test_ieee754_conversion(void) {
    printf("\n=== IEEE754 Conversion Tests ===\n");

    int32_t test_values[] = {0, 1, -1, 100, -100, 1000000};
    int num_tests = sizeof(test_values) / sizeof(test_values[0]);

    for (int i = 0; i < num_tests; i++) {
        int32_t val = test_values[i];
        uint32_t nd_float = nd500_float_from_int32(val);
        float ieee = nd500_float_to_ieee754(nd_float);
        uint32_t nd_back = nd500_float_from_ieee754(ieee);
        int32_t back = nd500_float_to_int32(nd_back);

        char name[64];
        snprintf(name, sizeof(name), "ieee754_roundtrip(%d)", val);

        char details[128];
        snprintf(details, sizeof(details), "expected %d, got %d (ieee=%.6f)", val, back, ieee);

        test_result(name, val == back, details);
    }
}

/* Test float multiplication using helper functions */
static void test_float_multiply(void) {
    printf("\n=== Float Multiplication Tests ===\n");

    struct {
        int32_t a, b, expected;
    } test_cases[] = {
        {2, 3, 6},
        {10, 10, 100},
        {100, 100, 10000},
        {-5, 4, -20},
        {-6, -7, 42},
        {1000, 1000, 1000000},
        {0, 12345, 0},
        {1, 999, 999},
    };
    int num_tests = sizeof(test_cases) / sizeof(test_cases[0]);

    for (int i = 0; i < num_tests; i++) {
        int32_t a = test_cases[i].a;
        int32_t b = test_cases[i].b;
        int32_t expected = test_cases[i].expected;

        /* Convert to ND500 float */
        uint32_t f1 = nd500_float_from_int32(a);
        uint32_t f2 = nd500_float_from_int32(b);

        /* Convert to IEEE754 for multiplication */
        float ieee1 = nd500_float_to_ieee754(f1);
        float ieee2 = nd500_float_to_ieee754(f2);
        float ieee_result = ieee1 * ieee2;

        /* Convert back to ND500 float */
        uint32_t f_result = nd500_float_from_ieee754(ieee_result);
        int32_t result = nd500_float_to_int32(f_result);

        char name[64];
        snprintf(name, sizeof(name), "%d * %d", a, b);

        char details[128];
        snprintf(details, sizeof(details), "expected %d, got %d", expected, result);

        test_result(name, expected == result, details);
    }
}

/* Test float division using helper functions */
static void test_float_divide(void) {
    printf("\n=== Float Division Tests ===\n");

    struct {
        int32_t a, b, expected;
    } test_cases[] = {
        {10, 2, 5},
        {100, 10, 10},
        {99, 10, 9},      /* truncates toward zero */
        {-20, 4, -5},
        {-21, 4, -5},     /* truncates toward zero */
        {1000000, 1000, 1000},
        {7, 2, 3},        /* 7/2 = 3.5 -> 3 */
        {-7, 2, -3},      /* -7/2 = -3.5 -> -3 */
    };
    int num_tests = sizeof(test_cases) / sizeof(test_cases[0]);

    for (int i = 0; i < num_tests; i++) {
        int32_t a = test_cases[i].a;
        int32_t b = test_cases[i].b;
        int32_t expected = test_cases[i].expected;

        /* Convert to ND500 float */
        uint32_t f1 = nd500_float_from_int32(a);
        uint32_t f2 = nd500_float_from_int32(b);

        /* Convert to IEEE754 for division */
        float ieee1 = nd500_float_to_ieee754(f1);
        float ieee2 = nd500_float_to_ieee754(f2);
        float ieee_result = ieee1 / ieee2;

        /* Convert back to ND500 float */
        uint32_t f_result = nd500_float_from_ieee754(ieee_result);
        int32_t result = nd500_float_to_int32(f_result);

        char name[64];
        snprintf(name, sizeof(name), "%d / %d", a, b);

        char details[128];
        snprintf(details, sizeof(details), "expected %d, got %d", expected, result);

        test_result(name, expected == result, details);
    }
}

/* Test float add/subtract using helper functions */
static void test_float_add_sub(void) {
    printf("\n=== Float Add/Subtract Tests ===\n");

    struct {
        int32_t a, b, expected_add, expected_sub;
    } test_cases[] = {
        {100, 200, 300, -100},
        {-50, 100, 50, -150},
        {1000000, 1000000, 2000000, 0},
        {0, 42, 42, -42},
        {-100, -100, -200, 0},
    };
    int num_tests = sizeof(test_cases) / sizeof(test_cases[0]);

    for (int i = 0; i < num_tests; i++) {
        int32_t a = test_cases[i].a;
        int32_t b = test_cases[i].b;

        /* Convert to ND500 float */
        uint32_t f1 = nd500_float_from_int32(a);
        uint32_t f2 = nd500_float_from_int32(b);

        /* Test addition */
        float ieee1 = nd500_float_to_ieee754(f1);
        float ieee2 = nd500_float_to_ieee754(f2);
        float ieee_add = ieee1 + ieee2;
        uint32_t f_add = nd500_float_from_ieee754(ieee_add);
        int32_t result_add = nd500_float_to_int32(f_add);

        char name_add[64];
        snprintf(name_add, sizeof(name_add), "%d + %d", a, b);
        char details_add[128];
        snprintf(details_add, sizeof(details_add), "expected %d, got %d", test_cases[i].expected_add, result_add);
        test_result(name_add, test_cases[i].expected_add == result_add, details_add);

        /* Test subtraction */
        float ieee_sub = ieee1 - ieee2;
        uint32_t f_sub = nd500_float_from_ieee754(ieee_sub);
        int32_t result_sub = nd500_float_to_int32(f_sub);

        char name_sub[64];
        snprintf(name_sub, sizeof(name_sub), "%d - %d", a, b);
        char details_sub[128];
        snprintf(details_sub, sizeof(details_sub), "expected %d, got %d", test_cases[i].expected_sub, result_sub);
        test_result(name_sub, test_cases[i].expected_sub == result_sub, details_sub);
    }
}

/* Test double precision operations */
static void test_double_operations(void) {
    printf("\n=== Double Precision Tests ===\n");

    struct {
        int64_t a, b, expected_mul, expected_div;
    } test_cases[] = {
        {1000000LL, 1000000LL, 1000000000000LL, 1LL},
        {-123456LL, 789LL, -97406784LL, -156LL},
        {2LL, 3LL, 6LL, 0LL},  /* 2/3 truncates to 0 */
        {100LL, 3LL, 300LL, 33LL},
    };
    int num_tests = sizeof(test_cases) / sizeof(test_cases[0]);

    for (int i = 0; i < num_tests; i++) {
        int64_t a = test_cases[i].a;
        int64_t b = test_cases[i].b;

        /* Convert to ND500 double */
        uint64_t d1 = nd500_double_from_int64(a);
        uint64_t d2 = nd500_double_from_int64(b);

        /* Test multiplication */
        double ieee1 = nd500_double_to_ieee754(d1);
        double ieee2 = nd500_double_to_ieee754(d2);
        double ieee_mul = ieee1 * ieee2;
        uint64_t d_mul = nd500_double_from_ieee754(ieee_mul);
        int64_t result_mul = nd500_double_to_int64(d_mul);

        char name_mul[64];
        snprintf(name_mul, sizeof(name_mul), "%lld * %lld (double)", (long long)a, (long long)b);
        char details_mul[128];
        snprintf(details_mul, sizeof(details_mul), "expected %lld, got %lld",
                 (long long)test_cases[i].expected_mul, (long long)result_mul);
        test_result(name_mul, test_cases[i].expected_mul == result_mul, details_mul);

        /* Test division */
        double ieee_div = ieee1 / ieee2;
        uint64_t d_div = nd500_double_from_ieee754(ieee_div);
        int64_t result_div = nd500_double_to_int64(d_div);

        char name_div[64];
        snprintf(name_div, sizeof(name_div), "%lld / %lld (double)", (long long)a, (long long)b);
        char details_div[128];
        snprintf(details_div, sizeof(details_div), "expected %lld, got %lld",
                 (long long)test_cases[i].expected_div, (long long)result_div);
        test_result(name_div, test_cases[i].expected_div == result_div, details_div);
    }
}

/* Test combined multiply-divide operations */
static void test_combined_operations(void) {
    printf("\n=== Combined Operations Tests ===\n");

    struct {
        int32_t a, mul, div, expected;
    } test_cases[] = {
        {100, 7, 7, 100},      /* 100 * 7 / 7 = 100 */
        {1000, 3, 3, 1000},    /* 1000 * 3 / 3 = 1000 */
        {50, 20, 10, 100},     /* 50 * 20 / 10 = 100 */
        {-100, 5, 5, -100},    /* -100 * 5 / 5 = -100 */
    };
    int num_tests = sizeof(test_cases) / sizeof(test_cases[0]);

    for (int i = 0; i < num_tests; i++) {
        int32_t a = test_cases[i].a;
        int32_t mul = test_cases[i].mul;
        int32_t div = test_cases[i].div;
        int32_t expected = test_cases[i].expected;

        /* Convert to ND500 float */
        uint32_t f1 = nd500_float_from_int32(a);
        uint32_t f_mul = nd500_float_from_int32(mul);
        uint32_t f_div = nd500_float_from_int32(div);

        /* Multiply */
        float ieee1 = nd500_float_to_ieee754(f1);
        float ieee_mul = nd500_float_to_ieee754(f_mul);
        float ieee_intermediate = ieee1 * ieee_mul;

        /* Divide */
        float ieee_div = nd500_float_to_ieee754(f_div);
        float ieee_result = ieee_intermediate / ieee_div;

        /* Convert back */
        uint32_t f_result = nd500_float_from_ieee754(ieee_result);
        int32_t result = nd500_float_to_int32(f_result);

        char name[64];
        snprintf(name, sizeof(name), "%d * %d / %d", a, mul, div);

        char details[128];
        snprintf(details, sizeof(details), "expected %d, got %d", expected, result);

        test_result(name, expected == result, details);
    }
}

/* Test fractional values through the ND <-> IEEE conversion */
static void test_fractional_values(void) {
    printf("\n=== Fractional Value Tests ===\n");

    /* Values exactly representable in both ND (22+1 bit mantissa) and IEEE */
    float test_values[] = {0.5f, -0.5f, 0.25f, 2.5f, -2.5f, 3.75f, 0.125f,
                           1.5f, -1.5f, 100.625f, -100.625f, 0.0078125f};
    int num_tests = sizeof(test_values) / sizeof(test_values[0]);

    for (int i = 0; i < num_tests; i++) {
        float val = test_values[i];
        uint32_t nd_bits = nd500_float_from_ieee754(val);
        float back = nd500_float_to_ieee754(nd_bits);

        char name[64];
        snprintf(name, sizeof(name), "fractional(%g)", (double)val);

        char details[128];
        snprintf(details, sizeof(details), "expected %g, got %g (nd bits: 0x%08X)",
                 (double)val, (double)back, nd_bits);

        test_result(name, val == back, details);
    }
}

/* Test D <-> F precision conversion (must preserve fractions, not go via int) */
static void test_double_single_conversion(void) {
    printf("\n=== Double <-> Single Conversion Tests ===\n");

    /* F -> D -> F must be exact for any F value */
    float f_values[] = {2.5f, -2.5f, 0.5f, 100.625f, -0.125f, 12345.0f, 1.0f};
    int num_f = sizeof(f_values) / sizeof(f_values[0]);

    for (int i = 0; i < num_f; i++) {
        float val = f_values[i];
        uint32_t f_bits = nd500_float_from_ieee754(val);
        uint64_t d_bits = nd500_single_to_double(f_bits);
        uint32_t f_back = nd500_double_to_single(d_bits);

        char name[64];
        snprintf(name, sizeof(name), "F->D->F(%g)", (double)val);

        char details[160];
        snprintf(details, sizeof(details),
                 "f_bits 0x%08X -> d_bits 0x%016llX -> 0x%08X",
                 f_bits, (unsigned long long)d_bits, f_back);

        test_result(name, f_bits == f_back, details);
    }

    /* D -> F keeps fractional value (regression: old code converted via int64,
     * so 2.5 D -> F yielded 2) */
    double d_values[] = {2.5, -2.5, 0.5, 100.625, -0.125};
    int num_d = sizeof(d_values) / sizeof(d_values[0]);

    for (int i = 0; i < num_d; i++) {
        double val = d_values[i];
        uint64_t d_bits = nd500_double_from_ieee754(val);
        uint32_t f_bits = nd500_double_to_single(d_bits);
        float f_val = nd500_float_to_ieee754(f_bits);

        char name[64];
        snprintf(name, sizeof(name), "D->F(%g)", val);

        char details[128];
        snprintf(details, sizeof(details), "expected %g, got %g", val, (double)f_val);

        test_result(name, (double)f_val == val, details);
    }

    /* Zero maps to zero in both directions */
    test_result("D->F(0)", nd500_double_to_single(0) == 0, "nonzero result");
    test_result("F->D(0)", nd500_single_to_double(0) == 0, "nonzero result");
}

/* Test overflow/underflow edges of the 9-bit bias-256 exponent */
static void test_exponent_edges(void) {
    printf("\n=== Exponent Edge Tests ===\n");

    /* The ND-500 single exponent is 9 bits, bias 256 -> e in [1-256, 511-256] =
     * [-255, 255], so the smallest representable magnitude is 2^(1-256) * 0.5 =
     * 2^-256 (~8.6e-78). That is FAR BELOW the smallest IEEE-754 single denormal
     * (2^-149 ~ 1.4e-45): a float can never be small enough to underflow the ND
     * single format. So the smallest IEEE denormal must convert to a NORMAL,
     * NON-ZERO ND value (frexp gives e=-148 -> e_field=108, well inside [1,511]).
     * (The previous version of this test asserted flush-to-zero here; that premise
     * was backwards - IEEE denormals sit far ABOVE the ND underflow floor, not
     * below it - and a float input cannot reach ND single's underflow threshold.) */
    union { uint32_t u; float f; } denorm;
    denorm.u = 0x00000001u; /* smallest IEEE denormal = 2^-149 */
    uint32_t nd_small = nd500_float_from_ieee754(denorm.f);
    test_result("smallest IEEE denormal is representable (no underflow)",
                !nd500_float_is_zero(nd_small),
                "2^-149 wrongly flushed to zero (it is well within ND single range)");

    /* Genuine ND single underflow: only a value below 2^-256 flushes to zero, and
     * that is only reachable through the double-input native codec (a float cannot
     * hold it). 1e-100 ~ 2^-332 -> e_field = -332 + 256 = -76 < 1 -> flush. */
    bool unfl = false;
    uint32_t nd_tiny = nd500_native_single_from_double(1e-100, NULL, &unfl);
    test_result("value below 2^-256 flushes to zero", nd500_float_is_zero(nd_tiny),
                "sub-2^-256 magnitude did not flush to zero");
    test_result("underflow flag set on flush", unfl, "underflow flag not reported");

    /* IEEE infinity saturates to ND max (exponent field all ones) */
    union { uint32_t u; float f; } inf;
    inf.u = 0x7F800000u;
    uint32_t nd_inf = nd500_float_from_ieee754(inf.f);
    test_result("infinity saturates", !nd500_float_is_zero(nd_inf) && !nd500_float_is_negative(nd_inf),
                "infinity mapped to zero or negative");

    /* Negative zero in, zero out */
    union { uint32_t u; float f; } negz;
    negz.u = 0x80000000u;
    uint32_t nd_negz = nd500_float_from_ieee754(negz.f);
    test_result("negative zero is zero", nd500_float_is_zero(nd_negz),
                "minus zero did not map to ND zero");

    /* Round-trip a very large in-range value */
    float big = 1.0e30f;
    uint32_t nd_big = nd500_float_from_ieee754(big);
    float big_back = nd500_float_to_ieee754(nd_big);
    /* Truncating conversion loses at most 1 ulp of the 22-bit mantissa */
    float rel_err = (big_back - big) / big;
    if (rel_err < 0) rel_err = -rel_err;
    char details[128];
    snprintf(details, sizeof(details), "1e30 -> 0x%08X -> %g (rel err %g)",
             nd_big, (double)big_back, (double)rel_err);
    test_result("large value roundtrip", rel_err < 1.0e-6f, details);
}

int main(int argc, char** argv) {
    printf("ND500 Float Arithmetic Tests\n");
    printf("============================\n");

    /* Run all test categories */
    test_int_to_float_roundtrip();
    test_ieee754_conversion();
    test_float_multiply();
    test_float_divide();
    test_float_add_sub();
    test_double_operations();
    test_combined_operations();
    test_fractional_values();
    test_double_single_conversion();
    test_exponent_edges();

    /* Summary */
    printf("\n============================\n");
    printf("Total: %d passed, %d failed\n", tests_passed, tests_failed);

    return (tests_failed > 0) ? 1 : 0;
}
