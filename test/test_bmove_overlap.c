/**
 * ND500 BMOVE Overlap Test
 *
 * Tests BMOVE instruction behavior with overlapping memory regions.
 *
 * When destination > source and regions overlap, BMOVE must copy backward
 * to prevent data corruption. This is the classic memmove() vs memcpy() problem.
 *
 * Example: Copy [0,1,2,3] from 0x1000 to 0x1002 (2-byte overlap)
 *   WRONG (forward copy):  0x1000:[0,1,0,1,0,1,0,1] - corrupted!
 *   RIGHT (backward copy): 0x1000:[0,1,0,1,2,3,6,7] - correct (last 2 bytes unchanged)
 *
 * Reference: ND-500 Reference Manual Section 15.1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/cpu/nd500_instructions.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/instruction_helpers.h"

/* Test memory size */
#define MEMORY_SIZE (1 << 20)  /* 1MB */
#define CODE_ADDR   0x2000     /* Where we place test instructions */
#define DATA_ADDR   0x1000     /* Where we place test data */

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

/* Helper to write instruction bytes */
static void write_code(Nd500Machine* m, uint32_t addr, const uint8_t* code, size_t len) {
    for (size_t i = 0; i < len; i++) {
        nd500_bus_write8(m, addr + i, code[i]);
    }
}

/* Helper to write test data pattern */
static void write_data_bytes(Nd500Machine* m, uint32_t addr, const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        nd500_bus_write8(m, addr + i, data[i]);
    }
}

/* Helper to read data bytes into buffer */
static void read_data_bytes(Nd500Machine* m, uint32_t addr, uint8_t* buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        buf[i] = nd500_bus_read8(m, addr + i);
    }
}

/**
 * Test BMOVE with overlapping regions (dst > src)
 *
 * This is the critical case that requires backward copying.
 */
static void test_bmove_overlap_forward(void) {
    printf("\n=== BMOVE Overlap Tests (dst > src) ===\n");

    /* Create machine and CPU */
    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);

    /* Test 1: Byte BMOVE with 2-byte overlap
     * Using pre-indexed addressing: source=r1.(0), dest=r1.(2), count=4
     * Initial: [0,1,2,3,4,5,6,7]
     * Copy 4 bytes from position 0 to position 2
     * Expected: positions 0-1 unchanged, positions 2-5 get values from 0-3
     * Result: [0,1,0,1,2,3,6,7] (bytes 6,7 unchanged)
     */
    {
        uint8_t initial[] = {0, 1, 2, 3, 4, 5, 6, 7};
        uint8_t expected[] = {0, 1, 0, 1, 2, 3, 6, 7};  /* Copy 4 bytes from +0 to +2 */
        write_data_bytes(&m, DATA_ADDR, initial, sizeof(initial));

        /* BY BMOVE r1.(0), r1.(2), $4
         * 0xFD 0x20 = BY BMOVE opcode
         * 0xF4 0x00 = r1.(0) - source (pre-indexed I1 + byte offset 0)
         * 0xF4 0x02 = r1.(2) - dest (pre-indexed I1 + byte offset 2)
         * 0x04 = short constant 4 (count)
         */
        uint8_t code[] = {
            0xFD, 0x20,             /* BY BMOVE */
            0xF4, 0x00,             /* source = r1.(0) */
            0xF4, 0x02,             /* dest = r1.(2) */
            0x04                    /* count = 4 (short constant) */
        };
        write_code(&m, CODE_ADDR, code, sizeof(code));

        /* Set I1 to DATA_ADDR */
        cpu.I[0] = DATA_ADDR;
        cpu.PC = CODE_ADDR;
        m.run_flag = 1;  /* Enable running */
        nd500_cpu_step(&cpu);

        uint8_t result[8];
        read_data_bytes(&m, DATA_ADDR, result, sizeof(result));

        bool passed = (memcmp(result, expected, sizeof(expected)) == 0);

        char details[256];
        if (!passed) {
            snprintf(details, sizeof(details),
                "got [%d,%d,%d,%d,%d,%d,%d,%d], expected [%d,%d,%d,%d,%d,%d,%d,%d]",
                result[0], result[1], result[2], result[3],
                result[4], result[5], result[6], result[7],
                expected[0], expected[1], expected[2], expected[3],
                expected[4], expected[5], expected[6], expected[7]);
        }
        test_result("BY BMOVE overlap (copy 4 bytes, 2-byte overlap)", passed, details);
    }

    /* Test 2: Word BMOVE with 4-byte overlap
     * W BMOVE r1.(0), r1.(4), $3 - copy 3 words from +0 to +4 (1 word overlap)
     * Initial: [0x11111111, 0x22222222, 0x33333333, 0x44444444]
     * Copy 3 words from position 0 to position 4 (word 1)
     * Expected: word 0 unchanged, words 1-3 get values from words 0-2
     */
    {
        uint32_t initial[] = {0x11111111, 0x22222222, 0x33333333, 0x44444444};
        uint32_t expected[] = {0x11111111, 0x11111111, 0x22222222, 0x33333333};
        for (int i = 0; i < 4; i++) {
            nd500_bus_write8(&m, DATA_ADDR + i*4 + 0, (initial[i] >> 24) & 0xFF);
            nd500_bus_write8(&m, DATA_ADDR + i*4 + 1, (initial[i] >> 16) & 0xFF);
            nd500_bus_write8(&m, DATA_ADDR + i*4 + 2, (initial[i] >> 8) & 0xFF);
            nd500_bus_write8(&m, DATA_ADDR + i*4 + 3, initial[i] & 0xFF);
        }

        /* W BMOVE r1.(0), r1.(4), $3
         * 0xFE 0x79 = W BMOVE opcode
         * 0xF4 0x00 = r1.(0) - source
         * 0xF4 0x04 = r1.(4) - dest
         * 0x03 = short constant 3 (count)
         */
        uint8_t code[] = {
            0xFE, 0x79,             /* W BMOVE */
            0xF4, 0x00,             /* source = r1.(0) */
            0xF4, 0x04,             /* dest = r1.(4) */
            0x03                    /* count = 3 (short constant) */
        };
        write_code(&m, CODE_ADDR, code, sizeof(code));

        cpu.I[0] = DATA_ADDR;
        cpu.PC = CODE_ADDR;
        m.run_flag = 1;  /* Enable running */
        nd500_cpu_step(&cpu);

        uint32_t result[4];
        for (int i = 0; i < 4; i++) {
            result[i] = ((uint32_t)nd500_bus_read8(&m, DATA_ADDR + i*4 + 0) << 24) |
                        ((uint32_t)nd500_bus_read8(&m, DATA_ADDR + i*4 + 1) << 16) |
                        ((uint32_t)nd500_bus_read8(&m, DATA_ADDR + i*4 + 2) << 8) |
                        (uint32_t)nd500_bus_read8(&m, DATA_ADDR + i*4 + 3);
        }

        bool passed = (memcmp(result, expected, sizeof(expected)) == 0);

        char details[256];
        if (!passed) {
            snprintf(details, sizeof(details),
                "got [0x%08X,0x%08X,0x%08X,0x%08X], expected [0x%08X,0x%08X,0x%08X,0x%08X]",
                result[0], result[1], result[2], result[3],
                expected[0], expected[1], expected[2], expected[3]);
        }
        test_result("W BMOVE overlap (copy 3 words, 4-byte overlap)", passed, details);
    }

    /* Test 3: Non-overlapping copy (should still work)
     * BY BMOVE r1.(0), r1.(4), $4 - copy 4 bytes from +0 to +4
     */
    {
        uint8_t initial[] = {0xAA, 0xBB, 0xCC, 0xDD, 0x00, 0x00, 0x00, 0x00};
        uint8_t expected[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xAA, 0xBB, 0xCC, 0xDD};
        write_data_bytes(&m, DATA_ADDR, initial, sizeof(initial));

        /* BY BMOVE r1.(0), r1.(4), $4 */
        uint8_t code[] = {
            0xFD, 0x20,             /* BY BMOVE */
            0xF4, 0x00,             /* source = r1.(0) */
            0xF4, 0x04,             /* dest = r1.(4) */
            0x04                    /* count = 4 (short constant) */
        };
        write_code(&m, CODE_ADDR, code, sizeof(code));

        cpu.I[0] = DATA_ADDR;
        cpu.PC = CODE_ADDR;
        m.run_flag = 1;  /* Enable running */
        nd500_cpu_step(&cpu);

        uint8_t result[8];
        read_data_bytes(&m, DATA_ADDR, result, sizeof(result));

        bool passed = (memcmp(result, expected, sizeof(expected)) == 0);

        char details[256];
        if (!passed) {
            snprintf(details, sizeof(details),
                "got [0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,0x%02X]",
                result[0], result[1], result[2], result[3],
                result[4], result[5], result[6], result[7]);
        }
        test_result("BY BMOVE non-overlapping copy", passed, details);
    }

    nd500_machine_free(&m);
}

/**
 * Test BMOVE with backward overlap (src > dst)
 *
 * This case should use forward copying and work correctly.
 */
static void test_bmove_overlap_backward(void) {
    printf("\n=== BMOVE Overlap Tests (src > dst) ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);

    /* Test: Copy from r1.(2) to r1.(0) (src > dst, forward copy is fine)
     * BY BMOVE r1.(2), r1.(0), $4
     * Initial: [0xFF, 0xFF, 0, 1, 2, 3, 0xFF, 0xFF]
     * Copy 4 bytes from position 2 to position 0
     * Expected: positions 0-3 get [0,1,2,3], positions 4-5 are overwritten by overlap
     *           positions 6-7 unchanged
     * Result: [0, 1, 2, 3, 2, 3, 0xFF, 0xFF]
     */
    {
        uint8_t initial[] = {0xFF, 0xFF, 0, 1, 2, 3, 0xFF, 0xFF};
        uint8_t expected[] = {0, 1, 2, 3, 2, 3, 0xFF, 0xFF};
        write_data_bytes(&m, DATA_ADDR, initial, sizeof(initial));

        /* BY BMOVE r1.(2), r1.(0), $4 */
        uint8_t code[] = {
            0xFD, 0x20,             /* BY BMOVE */
            0xF4, 0x02,             /* source = r1.(2) */
            0xF4, 0x00,             /* dest = r1.(0) */
            0x04                    /* count = 4 (short constant) */
        };
        write_code(&m, CODE_ADDR, code, sizeof(code));

        cpu.I[0] = DATA_ADDR;
        cpu.PC = CODE_ADDR;
        m.run_flag = 1;  /* Enable running */
        nd500_cpu_step(&cpu);

        uint8_t result[8];
        read_data_bytes(&m, DATA_ADDR, result, sizeof(result));

        bool passed = (memcmp(result, expected, sizeof(expected)) == 0);

        char details[256];
        if (!passed) {
            snprintf(details, sizeof(details),
                "got [%d,%d,%d,%d,%d,%d,%d,%d], expected [%d,%d,%d,%d,%d,%d,%d,%d]",
                result[0], result[1], result[2], result[3],
                result[4], result[5], result[6], result[7],
                expected[0], expected[1], expected[2], expected[3],
                expected[4], expected[5], expected[6], expected[7]);
        }
        test_result("BY BMOVE backward overlap (src > dst)", passed, details);
    }

    nd500_machine_free(&m);
}

/**
 * BMOVE MUST NOT DEPEND ON THE HOST'S RUN LOOP.
 *
 * Every other case in this file sets m.run_flag = 1 before stepping, because
 * BMOVE used to abort when the flag was clear - and the debugger's step command
 * had been made to set it for the same reason (src/debugger/commands.c). That is
 * a host property, not a guest signal: nothing a BMOVE does clears it, and an
 * EMBEDDING that steps the CPU itself never sets it at all.
 *
 * nd100x's mfbus bridge is exactly such an embedding. With run_flag in the abort
 * condition, every BMOVE in the ND-5000 lane returned after its first read having
 * written NOTHING - no trap, PC advancing normally, completely silent. That is
 * what broke SINTRAN's ND-500 swapper: the BMOVE at 0x0800744C never filled the
 * local it copies a count into, the count read 0 instead of 0x1000, 0-1 gave
 * 0xFFFF as a loop bound, and the 65535-iteration loop ran four bytes off the end
 * of segment 6 - "The Swapper stopped" plus a PAGE FAULT at segment 6 +0x8004.
 *
 * So this case deliberately does NOT set run_flag, and a copy must still happen.
 */
static void test_bmove_without_run_flag(void) {
    printf("\n=== BMOVE does not depend on run_flag ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);

    /* The shape the swapper uses: four bytes from an unrelated source into a
     * four-byte destination, no overlap. The source bytes are the ones measured
     * live at 0x0801289C, so a half-done copy is visible rather than ambiguous. */
    const uint8_t src_bytes[4]  = {0x00, 0x07, 0x10, 0x00};
    const uint8_t expected[4]   = {0x00, 0x07, 0x10, 0x00};

    write_data_bytes(&m, DATA_ADDR, src_bytes, sizeof(src_bytes));
    {
        const uint8_t zeros[4] = {0, 0, 0, 0};
        write_data_bytes(&m, DATA_ADDR + 16u, zeros, sizeof(zeros));
    }

    /* BY BMOVE r1.(0), r1.(16), $4 */
    uint8_t code[] = {
        0xFD, 0x20,             /* BY BMOVE */
        0xF4, 0x00,             /* source = r1.(0)  */
        0xF4, 0x10,             /* dest   = r1.(16) */
        0x04                    /* count  = 4 */
    };
    write_code(&m, CODE_ADDR, code, sizeof(code));

    cpu.I[0] = DATA_ADDR;
    cpu.PC = CODE_ADDR;
    /* run_flag deliberately left at 0 - this is the whole point of the test. */
    nd500_cpu_step(&cpu);

    uint8_t result[4];
    read_data_bytes(&m, DATA_ADDR + 16u, result, sizeof(result));

    bool passed = (memcmp(result, expected, sizeof(expected)) == 0);
    char details[256];
    if (!passed) {
        snprintf(details, sizeof(details),
            "got [%02X,%02X,%02X,%02X], expected [%02X,%02X,%02X,%02X] "
            "- BMOVE aborted because the host run loop was not running",
            result[0], result[1], result[2], result[3],
            expected[0], expected[1], expected[2], expected[3]);
    }
    test_result("BY BMOVE copies with run_flag clear (embedded stepping)", passed, details);

    /* And the halfword the swapper actually reads out of it. A copy that moved
     * only some of the bytes would still fail the memcmp above, but this names
     * the value whose loss caused the runaway loop. */
    {
        uint8_t again[4];
        read_data_bytes(&m, DATA_ADDR + 16u, again, sizeof(again));
        uint16_t hw = (uint16_t)(((uint16_t)again[2] << 8) | again[3]);
        char d2[128];
        d2[0] = '\0';
        if (hw != 0x1000u) {
            snprintf(d2, sizeof(d2), "halfword at +2 = 0x%04X, expected 0x1000", hw);
        }
        test_result("and the halfword at +2 survives the copy", hw == 0x1000u, d2);
    }

    nd500_machine_free(&m);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("ND500 BMOVE Overlap Tests\n");
    printf("=========================\n");
    test_bmove_without_run_flag();

    test_bmove_overlap_forward();
    test_bmove_overlap_backward();

    printf("\n=== Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
