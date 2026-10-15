/**
 * ND-500 conversion instructions: the destination has its OWN data type.
 *
 * ND-05.009.4 section 15.2 "Data type conversion" gives the format
 *
 *     t1 t2CONV  <source/r/t1>, <dest/w/t2>
 *
 * so in W BICONV the source is a word and the destination is a BIT. A
 * post-indexed destination therefore counts BITS: byte base + index/8, bit
 * 7 - index%8 ("post indexing always counts the data elements from the left").
 *
 * The instruction bytes of the first case are the ones the real PLANC-500 G
 * compiler emitted in the PKA01 probe of the new-planc repository at 08000175H:
 *
 *     FD 53 45 E2 08 00 06 58      W BICONV  B.14H, $08000658H(I3)
 *
 * With I3 = 7 the bit is bit 0 of byte 08000658H. Scaling the index by the
 * source's word size instead wrote a whole byte at 08000674H, the high byte of
 * a descriptor address, and the program later took a protect violation.
 *
 * Also held here: post-indexed F operands count 4-byte elements.
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
#define DATA_ADDR   0x1000
#define FRAME_ADDR  0x3000

static int tests_passed = 0;
static int tests_failed = 0;

static void check(const char* name, bool passed, const char* details) {
    if (passed) {
        tests_passed++;
        printf("  [PASS] %s\n", name);
    } else {
        tests_failed++;
        printf("  [FAIL] %s: %s\n", name, details);
    }
}

static void put32(Nd500Machine* m, uint32_t addr, uint32_t v) {
    nd500_bus_write8(m, addr + 0, (uint8_t)(v >> 24));
    nd500_bus_write8(m, addr + 1, (uint8_t)(v >> 16));
    nd500_bus_write8(m, addr + 2, (uint8_t)(v >> 8));
    nd500_bus_write8(m, addr + 3, (uint8_t)v);
}

static void clear_data(Nd500Machine* m) {
    for (uint32_t i = 0; i < 64; i++) {
        nd500_bus_write8(m, DATA_ADDR + i, 0xAA);
    }
}

/* Run one instruction: opcode FD xx, source B.14H (address code 45H), destination
 * $DATA_ADDR(I3) (address code E2H). */
static void run_conv(Nd500Machine* m, Nd500Cpu* cpu, uint8_t opcode_low,
                     uint32_t source_word, uint32_t i3) {
    uint8_t code[] = {
        0xFD, opcode_low, 0x45, 0xE2,
        (DATA_ADDR >> 24) & 0xFF, (DATA_ADDR >> 16) & 0xFF,
        (DATA_ADDR >> 8) & 0xFF, DATA_ADDR & 0xFF
    };
    nd500_cpu_reset(cpu);
    cpu->B = FRAME_ADDR;
    cpu->I[2] = i3;
    put32(m, FRAME_ADDR + 0x14, source_word);
    for (size_t i = 0; i < sizeof code; i++) {
        nd500_bus_write8(m, CODE_ADDR + (uint32_t)i, code[i]);
    }
    cpu->PC = CODE_ADDR;
    nd500_cpu_step(cpu);
}

/* Every byte of the data area except `except` must still hold the AA it was given. */
static bool rest_untouched(Nd500Machine* m, uint32_t except, uint32_t* where) {
    for (uint32_t i = 0; i < 64; i++) {
        if (i == except) continue;
        if (nd500_bus_read8(m, DATA_ADDR + i) != 0xAA) {
            *where = i;
            return false;
        }
    }
    return true;
}

static void test_w_biconv(void) {
    printf("\n=== W BICONV, destination post-indexed in bits ===\n");
    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);

    struct {
        uint32_t source, index;
        uint32_t byte_offset;
        uint8_t expected;
        const char* name;
    } cases[] = {
        /* AA = 10101010b */
        { 5, 7, 0, 0xAB, "index 7 sets bit 0 of byte 0 (PKA01)" },
        { 0, 7, 0, 0xAA, "index 7, zero source: bit 0 stays 0" },
        { 0, 0, 0, 0x2A, "index 0, zero source clears bit 7 of byte 0" },
        { 1, 9, 1, 0xEA, "index 9 sets bit 6 of byte 1" },
        { 0, 31, 3, 0xAA, "index 31, zero source: bit 0 of byte 3 stays 0" },
        { 0, 30, 3, 0xA8, "index 30, zero source clears bit 1 of byte 3" },
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        clear_data(&m);
        run_conv(&m, &cpu, 0x53, cases[i].source, cases[i].index);
        uint8_t got = nd500_bus_read8(&m, DATA_ADDR + cases[i].byte_offset);
        uint32_t where = 0;
        bool rest = rest_untouched(&m, cases[i].byte_offset, &where);
        char details[160];
        snprintf(details, sizeof details,
                 "byte %u: expected %02X got %02X; other bytes untouched: %s (first change at +%u)",
                 cases[i].byte_offset, cases[i].expected, got, rest ? "yes" : "no", where);
        check(cases[i].name, got == cases[i].expected && rest, details);
    }
    nd500_machine_free(&m);
}

static void test_w_byconv(void) {
    printf("\n=== W BYCONV, destination post-indexed in bytes ===\n");
    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);

    clear_data(&m);
    run_conv(&m, &cpu, 0x54, 0x41, 7);
    uint8_t got = nd500_bus_read8(&m, DATA_ADDR + 7);
    uint32_t where = 0;
    bool rest = rest_untouched(&m, 7, &where);
    char details[160];
    snprintf(details, sizeof details,
             "byte 7: expected 41 got %02X; other bytes untouched: %s (first change at +%u)",
             got, rest ? "yes" : "no", where);
    check("W BYCONV index 7 writes byte 7", got == 0x41 && rest, details);
    nd500_machine_free(&m);
}

static void test_f_post_index(void) {
    printf("\n=== F operand post-indexed in 4-byte elements ===\n");
    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);

    /* $DATA_ADDR(I1) with I1 = 3 at type F is DATA_ADDR + 12. */
    Nd500OperandDecoded op;
    memset(&op, 0, sizeof op);
    op.mode = ND500_ADDR_ABSOLUTE_PI;
    op.address_code = 0xE0;
    op.data_len = 4;
    op.data[0] = (DATA_ADDR >> 24) & 0xFF;
    op.data[1] = (DATA_ADDR >> 16) & 0xFF;
    op.data[2] = (DATA_ADDR >> 8) & 0xFF;
    op.data[3] = DATA_ADDR & 0xFF;
    cpu.I[0] = 3;
    uint32_t ea = nd500_operand_ea_at_dtype(&cpu, &op, ND500_DTYPE_FLOAT);
    char details[96];
    snprintf(details, sizeof details, "expected %X got %X", DATA_ADDR + 12, ea);
    check("F index 3 is 12 bytes on", ea == DATA_ADDR + 12, details);
    nd500_machine_free(&m);
}

int main(void) {
    printf("ND-500 conversion operand type tests\n");
    test_w_biconv();
    test_w_byconv();
    test_f_post_index();
    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
