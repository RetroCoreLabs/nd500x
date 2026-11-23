/**
 * F SWAP and D SWAP operate on the float registers.
 *
 * ND-05.009.4 8.13 (Register addressing): "If the data type of an instruction
 * is an integer or it does not contain a data type specification, one of the
 * integer registers is taken as the operand. If the data type of the
 * instruction is float or double float, one of the float or double float
 * registers is taken as the operand."
 *
 * F SWAP (opcode FCDC) was decoded with the word data type, so "F SWAP R1, R2"
 * swapped I1 and I2 and left A1 and A2 as they were.
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

#define MEMORY_SIZE (1 << 20)
#define CODE_ADDR   0x2000

static int tests_passed = 0;
static int tests_failed = 0;

static void check(const char* name, bool passed, uint32_t got, uint32_t want) {
    if (passed) {
        tests_passed++;
        printf("  [PASS] %s\n", name);
    } else {
        tests_failed++;
        printf("  [FAIL] %s: got 0x%08X, expected 0x%08X\n", name, got, want);
    }
}

int main(void) {
    printf("\n=== F SWAP on the float registers ===\n");
    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);

    /* F SWAP R1, R2: FC DC D0 D1 (bytes of the corpus case MOVE_swap "F SWAP r1, r2") */
    static const uint8_t code[] = {0xFC, 0xDC, 0xD0, 0xD1};
    for (size_t i = 0; i < sizeof code; i++) {
        nd500_bus_write8(&m, CODE_ADDR + (uint32_t)i, code[i]);
    }
    nd500_write_float_register(&cpu, 1, 0x00000000u);
    nd500_write_float_register(&cpu, 2, 0x3F800000u);
    cpu.I[0] = 0x11111111u;
    cpu.I[1] = 0x22222222u;
    cpu.PC = CODE_ADDR;
    m.run_flag = 1;
    nd500_cpu_step(&cpu);

    uint32_t a1 = nd500_read_float_register(&cpu, 1);
    uint32_t a2 = nd500_read_float_register(&cpu, 2);
    check("A1 gets the old A2", a1 == 0x3F800000u, a1, 0x3F800000u);
    check("A2 gets the old A1", a2 == 0x00000000u, a2, 0x00000000u);
    check("I1 not touched", cpu.I[0] == 0x11111111u, cpu.I[0], 0x11111111u);
    check("I2 not touched", cpu.I[1] == 0x22222222u, cpu.I[1], 0x22222222u);
    check("PC past the instruction", cpu.PC == CODE_ADDR + 4u, cpu.PC, CODE_ADDR + 4u);

    nd500_machine_free(&m);
    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
