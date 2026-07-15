/* diag_cat_record.c - Trace CAT-500's command-record chain walk.
 *
 * CAT-500's command dispatcher walks a record chain:
 *   0801EB06: w1 := $0x80232BC      ; load active record ptr
 *   0801EB0C: w1 := r1.(0x0)        ; w1 = record->next
 *   0801EB0F: w1 =: $0x80232BC      ; store back
 *   0801EB1B: w test r1.(0x0)       ; deref -> TRAP_PGF when ptr = 0x20202020
 *
 * This dumps the chain pointer each iteration plus the bytes it points at, so we
 * can see where the chain turns into ASCII text instead of a valid pointer.
 *
 *   argv[1] = DOM   argv[2] = console input   argv[3] = maxsteps
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/nd500_mmu.h"
#include "../src/cpu/nd500_domain.h"
#include "../src/ndlib/ndlib.h"
#include "../src/libmon/mon.h"
#include "../src/libmon/mon_log.h"
#include "../src/libmon/mon_file_table.h"
#include "../src/libmon/mon_clock.h"
#define MEMSZ (16u*1024u*1024u)
#define RECPTR 0x080232BCu

extern Nd500TrapState g_trap_state;

/* Read a word through the data MMU WITHOUT perturbing CPU state: the probe can
 * itself raise a translation trap, so save/restore the whole trap state around
 * it. The probe must be invisible to the program being traced. */
static uint32_t rd32(Nd500Cpu* c, uint32_t v, int* ok) {
    *ok = 1;
    if (c->machine && c->machine->mmu_enabled) {
        Nd500TrapState saved = g_trap_state;
        uint32_t p = nd500_mmu_translate(c, v, 0, 0);
        int faulted = nd500_trap_occurred();
        g_trap_state = saved;               /* restore: probe leaves no trace */
        if (faulted || p + 4 >= MEMSZ) { *ok = 0; return 0; }
        return nd500_bus_read32(c->machine, p);
    }
    if (v + 4 >= MEMSZ) { *ok = 0; return 0; }
    return nd500_bus_read32(c->machine, v);
}

static void dump_at(Nd500Cpu* c, uint32_t addr, const char* label) {
    printf("      %s @0x%08X: ", label, addr);
    char txt[33]; int any = 0;
    for (int i = 0; i < 32; i++) {
        int ok; uint32_t w = rd32(c, addr + (uint32_t)i, &ok);
        if (!ok) { printf("<unmapped>"); return; }
        uint8_t b = (uint8_t)(w & 0xFF);
        txt[i] = (b >= 0x20 && b < 0x7F) ? (char)b : '.';
        any = 1;
    }
    txt[32] = 0;
    if (any) printf("\"%s\"", txt);
    printf("\n");
}

int main(int argc, char** argv) {
    const char* dom = (argc > 1) ? argv[1] : "/mnt/d/ND/500/CAT5-CAT/cat-cat5-b06.dom";
    const char* cmd = (argc > 2) ? argv[2] : "generate-code,SCRATCH-00001:CAT,B:NRF\r";
    long maxsteps = (argc > 3) ? atol(argv[3]) : 3000000;

    if (ndlib_load_dom_header(dom) || ndlib_load_dom_segments()) { fprintf(stderr, "load fail\n"); return 2; }
    Nd500Machine m; Nd500Cpu c;
    nd500_machine_init(&m, MEMSZ); nd500_cpu_init(&c, &m); nd500_cpu_reset(&c);
    nd500_mmu_init(&c); nd500_domain_init(&c); mon_init();
    mon_log_enable(0);

    char in[160]; size_t j = 0;
    for (size_t i = 0; cmd[i] && j < 158; i++) {
        if (cmd[i] == '\\' && cmd[i+1] == 'r') { in[j++] = '\r'; i++; }
        else in[j++] = cmd[i];
    }
    in[j] = 0;
    mon_queue_console_input(in);

    uint32_t sa = 0; int dm = 0;
    if (ndlib_dom_load_to_machine(&m, &c, -1, NULL, NULL, &sa, &dm)) { fprintf(stderr, "load2 fail\n"); return 2; }
    printf("Loaded entry=0x%08X, tracing record chain at $0x%08X\n", c.PC, RECPTR);

    m.run_flag = 1; m.stop_reason = STOP_NONE;
    /* Watch the cell every step and report WHICH PC changed it. That pinpoints
     * the writer that puts ASCII text where a record pointer belongs. */
    uint32_t last = 0xFFFFFFFFu; int hits = 0;
    for (long s = 0; s < maxsteps && m.run_flag; s++) {
        uint32_t pc_before = c.PC;
        nd500_cpu_step(&c);
        if (hits < 20) {
            int ok; uint32_t rp = rd32(&c, RECPTR, &ok);
            if (ok && rp != last) {
                printf("  [%2d] instr=%llu  written by PC=0x%08X: $80232BC = 0x%08X%s\n",
                       hits, (unsigned long long)c.instruction_count, pc_before, rp,
                       (rp == 0x20202020u) ? "   <-- ASCII SPACES (corrupt)" : "");
                if (rp == 0x20202020u) dump_at(&c, RECPTR - 32, "cell-32");
                last = rp; hits++;
            }
        }
        if (m.run_flag == 0 && m.stop_reason != STOP_NONE) break;
    }
    printf("STOP=%s instr=%llu PC=%08X\n", nd500_stop_reason_str(m.stop_reason),
           (unsigned long long)c.instruction_count, c.PC);

    /* Are the mapped file bytes visible at the virtual address CAT-500 derives
     * from the segment number FSCNT returned? VA = [Segment(5)|Page(16)|Offset(11)],
     * so segment N base = N << 27. B.CAT starts d0 02 00 13 07 "CHARPTR". */
    printf("---- segment base probe (expect CHARPTR for the CAT input) ----\n");
    for (uint32_t seg = 3; seg <= 6; seg++) {
        uint32_t base = seg << 27;
        printf("  seg %u base 0x%08X: ", seg, base);
        int ok; uint32_t w = rd32(&c, base, &ok);
        if (!ok) { printf("<unmapped>\n"); continue; }
        printf("first word=0x%08X  ", w);
        dump_at(&c, base, "");
    }

    /* Queuing console input installs a ConsoleIO whose write_char captures output
     * into an internal buffer instead of stdout, so CAT-500's banner/prompts are
     * invisible unless we print the captured buffer here. */
    const char* out = mon_get_console_output();
    if (out && *out) {
        printf("---- CAT-500 console output ----\n%s\n--------------------------------\n", out);
    }
    return 0;
}
