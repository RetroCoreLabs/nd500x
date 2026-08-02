/*
 * RPGU/RWIP take a PAGE NUMBER operand, and it must be read at full width.
 *
 * ND-05.009.4 16.17 and 16.20: "The operand specifies the physical memory page
 * number (BIn RWIP) or physical page number/16 (Hn RWIP)", and "Only the lower
 * 25 bits of the bit number are significant". A 25-bit-significant page number
 * cannot be carried in a single bit.
 *
 * NDIX passes it as an ordinary 32-bit frame argument
 * (machine/locore.c:1216-1221):
 *
 *      _rpgu:  ents $24
 *              bi1 rpgu b.20
 *              ret
 *
 * which assembles to FE 88 45 - opcode 0xFE88, operand byte 0x45 = b.0x14
 * (b.20 decimal), the first argument slot.
 *
 * Under a real NDIX paging load every RPGU query arrived as page 0 while the
 * emulator had 1482 pages marked in the range 17..3071. The "BI" in BIn RPGU
 * names the TABLE being addressed (a bit in the PGU/WIP table), not the width
 * of the operand that selects it - but the decoder types the BI prefix as
 * ND500_DTYPE_BIT (5), so reading the operand with fi->data_type yielded a
 * single bit instead of the page number.
 *
 * ND500_DTYPE_BIT is CORRECT for genuine bit-addressing instructions (see
 * test_bit_addressing), so the decoder is left alone; these four instructions
 * read their page-number operand as a word instead.
 *
 * This test executes the real instruction - it does not re-read the operand
 * itself, which would only test this file against itself.
 */

#include <stdio.h>
#include <string.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/cpu/instruction_helpers.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/nd500_page_bits.h"

extern void nd500_instr_Rpgu(Nd500Cpu*, const Nd500FetchedInstruction*);
extern void nd500_instr_Rwip(Nd500Cpu*, const Nd500FetchedInstruction*);

static int tests_passed = 0;
static int tests_failed = 0;
#define CHECK(cond, name) do { \
    if (cond) { printf("  PASS: %s\n", name); tests_passed++; } \
    else      { printf("  FAIL: %s\n", name); tests_failed++; } \
} while (0)

#define MEMORY_SIZE (8 * 1024 * 1024)   /* PAGE_NUMBER<<11 is ~4.9 MB - must fit */
#define CODE_AT     0x1000u
#define FRAME_B     0x8000u
#define PAGE_NUMBER 0x000009E4u   /* a real pfn seen in an NDIX pagedaemon run */

static const char* dtype_name(int t) {
    switch (t) {
        case ND500_DTYPE_BYTE:       return "BYTE(8)";
        case ND500_DTYPE_HALFWORD:   return "HALFWORD(16)";
        case ND500_DTYPE_WORD:       return "WORD(32)";
        case ND500_DTYPE_DOUBLEWORD: return "DOUBLEWORD(64)";
        default:                     return "?";
    }
}

/* Decode the exact three bytes the NDIX kernel executes and report what the
 * operand read yields. */
static void check_one(Nd500Machine* m, Nd500Cpu* cpu,
                      uint16_t opcode, const char* label) {
    uint8_t b0 = (uint8_t)(opcode >> 8), b1 = (uint8_t)(opcode & 0xFF);

    nd500_bus_write8(m, CODE_AT + 0, b0);
    nd500_bus_write8(m, CODE_AT + 1, b1);
    nd500_bus_write8(m, CODE_AT + 2, 0x45);      /* b.0x14 - the arg slot */

    /* The page number as the guest stores it: a big-endian 32-bit word. */
    nd500_bus_write32(m, FRAME_B + 0x14, PAGE_NUMBER);
    cpu->B = FRAME_B;

    Nd500FetchedInstruction fi;
    memset(&fi, 0, sizeof(fi));
    int rc = nd500_decode_at(m, CODE_AT, &fi);

    /* Execute the real instruction rather than re-reading the operand here -
     * re-implementing the read would test this file against itself. Mark only
     * PAGE_NUMBER, so a result of 1 can ONLY come from the instruction having
     * reached that exact page. */
    nd500_page_bits_clear_all(m, ND500_PAGE_TABLE_PGU);
    nd500_page_bits_clear_all(m, ND500_PAGE_TABLE_WIP);
    nd500_page_bits_mark(m, PAGE_NUMBER << 11, 1);   /* sets PGU and WIP */

    cpu->ST1 |= ND500_FLAG_PIA;                      /* both are privileged */
    nd500_write_integer_register(cpu, 1, 0xDEADBEEFu);

    if (opcode == 0xFE88) nd500_instr_Rpgu(cpu, &fi);
    else                  nd500_instr_Rwip(cpu, &fi);

    uint32_t got = nd500_read_integer_register(cpu, 1);

    printf("  %s: rc=%d opcode=0x%04X dtype=%s(%d) len=%u -> r1=0x%X (want 1)\n",
           label, rc, fi.opcode, dtype_name((int)fi.data_type), (int)fi.data_type,
           fi.total_len, got);

    char name[96];
    snprintf(name, sizeof name, "%s reports page 0x%X as used", label, PAGE_NUMBER);
    CHECK(got == 1u, name);
}

int main(void) {
    Nd500Machine m;
    Nd500Cpu cpu;

    printf("=== RPGU/RWIP operand width ===\n");

    memset(&m, 0, sizeof(m));
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);

    check_one(&m, &cpu, 0xFE88, "BI1 RPGU");
    check_one(&m, &cpu, 0xFE94, "BI1 RWIP");

    printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    nd500_machine_free(&m);
    return tests_failed == 0 ? 0 : 1;
}
