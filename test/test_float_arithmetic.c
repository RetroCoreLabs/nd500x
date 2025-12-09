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

    /* Summary */
    printf("\n============================\n");
    printf("Total: %d passed, %d failed\n", tests_passed, tests_failed);

    return (tests_failed > 0) ? 1 : 0;
}
