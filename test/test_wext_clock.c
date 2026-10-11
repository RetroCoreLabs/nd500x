/*
 * WEXT, REXT, CLINIT and CLREAD - the '87 extensions a second ND-5000 CPU needs.
 *
 * ND-05.009.4 sections 16.28 (REXT), 16.29 (WEXT), 15.17 (CLINIT), 15.18
 * (CLREAD). The swapper of SINTRAN III VSX/500 L executes WEXT and CLREAD as
 * soon as a process is placed on a second CPU; see the headers of
 * src/cpu/instructions/SYSTEM/Wext.c and Clock.c.
 *
 * Every case here goes through nd500_cpu_step(), so the opcode table and the
 * dispatch table are tested together with the instruction bodies. The bytes
 * are:
 *
 *   FF EC+n-1 40    Wn WEXT b.0     device number is the word at B+0
 *   FF E8+n-1 40    Wn REXT b.0
 *   FF 1E           CLINIT
 *   FF 1F           CLREAD
 *
 * Operand byte 40 is "b.0", the same byte the conformance corpus uses in
 * Tset_Memory_NotSet (FD 40 40 = BY TSET b.0).
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/cpu/instruction_helpers.h"
#include "../src/machine/machine_protos.h"

static int tests_passed = 0;
static int tests_failed = 0;
#define CHECK(cond, name) do { \
    if (cond) { printf("  PASS: %s\n", name); tests_passed++; } \
    else      { printf("  FAIL: %s\n", name); tests_failed++; } \
} while (0)

#define MEMORY_SIZE (1024 * 1024)
#define CODE_AT     0x1000u
#define DATA_B      0x8000u

/* What the hooks saw, and what they answer. */
static int      s_write_calls;
static uint32_t s_write_device;
static uint32_t s_write_value;
static int      s_write_answer;
static int      s_read_calls;
static uint32_t s_read_device;
static uint32_t s_read_value;
static int      s_read_answer;

static int hook_write(Nd500Cpu* cpu, uint32_t device, uint32_t value) {
    (void)cpu;
    s_write_calls++;
    s_write_device = device;
    s_write_value = value;
    return s_write_answer;
}

static int hook_read(Nd500Cpu* cpu, uint32_t device, uint32_t* value) {
    (void)cpu;
    s_read_calls++;
    s_read_device = device;
    if (s_read_answer == 0) {
        *value = s_read_value;
    }
    return s_read_answer;
}

/* Place up to three bytes at CODE_AT and execute exactly one instruction. */
static bool run_one(Nd500Machine* m, Nd500Cpu* cpu, const uint8_t* bytes, unsigned n,
                    uint32_t device, bool privileged) {
    for (unsigned i = 0; i < n; i++) {
        nd500_bus_write8(m, CODE_AT + i, bytes[i]);
    }
    nd500_bus_write32(m, DATA_B, device);
    cpu->B = DATA_B;
    cpu->PC = CODE_AT;
    if (privileged) cpu->ST1 |= ND500_FLAG_PIA;
    else            cpu->ST1 &= ~ND500_FLAG_PIA;
    nd500_trap_clear();
    s_write_calls = 0;
    s_read_calls = 0;
    return nd500_cpu_step(cpu);
}

static void sleep_ms(long ms) {
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    (void)nanosleep(&ts, NULL);
}

int main(void) {
    Nd500Machine m;
    Nd500Cpu cpu;
    char name[128];

    memset(&m, 0, sizeof(m));
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);

    /* ---- WEXT -------------------------------------------------------------- */
    printf("=== WEXT ===\n");
    nd500_set_ext_device_hooks(hook_write, hook_read);

    for (unsigned r = 1; r <= 4; r++) {
        const uint8_t code[] = { 0xFF, (uint8_t)(0xEC + r - 1), 0x40 };
        uint32_t frame = 0x00003900u + r;      /* destination 071B in bits 13-8 */
        for (unsigned k = 1; k <= 4; k++) {
            nd500_write_integer_register(&cpu, (int)k, 0xEEEE0000u + k);
        }
        nd500_write_integer_register(&cpu, (int)r, frame);
        cpu.ST1 |= ND500_FLAG_K;
        s_write_answer = 0;
        bool ok = run_one(&m, &cpu, code, 3, 0, true);

        snprintf(name, sizeof name, "W%u WEXT device 0 executes without a trap", r);
        CHECK(ok && !nd500_trap_occurred(), name);
        snprintf(name, sizeof name, "W%u WEXT hands register %u to device 0, once", r, r);
        CHECK(s_write_calls == 1 && s_write_device == 0 && s_write_value == frame, name);
        snprintf(name, sizeof name, "W%u WEXT clears K when the word was taken", r);
        CHECK((cpu.ST1 & ND500_FLAG_K) == 0, name);
        snprintf(name, sizeof name, "W%u WEXT is three bytes long", r);
        CHECK(cpu.PC == CODE_AT + 3, name);
    }

    {
        const uint8_t code[] = { 0xFF, 0xEC, 0x40 };

        cpu.ST1 &= ~ND500_FLAG_K;
        s_write_answer = 1;
        (void)run_one(&m, &cpu, code, 3, 0, true);
        CHECK(s_write_calls == 1 && (cpu.ST1 & ND500_FLAG_K) != 0,
              "WEXT sets K when the device cannot take the word");

        for (uint32_t device = 1; device <= 8; device += 7) {
            cpu.ST1 &= ~ND500_FLAG_K;
            s_write_answer = 0;
            (void)run_one(&m, &cpu, code, 3, device, true);
            snprintf(name, sizeof name, "WEXT device %u sets K and writes nothing", device);
            CHECK(s_write_calls == 0 && (cpu.ST1 & ND500_FLAG_K) != 0, name);
        }

        cpu.ST1 &= ~ND500_FLAG_K;
        s_write_answer = 0;
        (void)run_one(&m, &cpu, code, 3, 0, false);
        CHECK(nd500_trap_occurred(), "WEXT without privilege traps");
        CHECK(s_write_calls == 0, "and writes nothing");
        nd500_trap_clear();

        nd500_set_ext_device_hooks(NULL, NULL);
        cpu.ST1 &= ~ND500_FLAG_K;
        bool ok = run_one(&m, &cpu, code, 3, 0, true);
        CHECK(ok && !nd500_trap_occurred() && (cpu.ST1 & ND500_FLAG_K) != 0,
              "WEXT with no device installed sets K and does not trap");
        nd500_set_ext_device_hooks(hook_write, hook_read);
    }

    /* ---- REXT -------------------------------------------------------------- */
    printf("=== REXT ===\n");
    for (unsigned r = 1; r <= 4; r++) {
        const uint8_t code[] = { 0xFF, (uint8_t)(0xE8 + r - 1), 0x40 };
        for (unsigned k = 1; k <= 4; k++) {
            nd500_write_integer_register(&cpu, (int)k, 0xEEEE0000u + k);
        }
        cpu.ST1 |= ND500_FLAG_K;
        s_read_answer = 0;
        s_read_value = 0x0000A500u + r;
        bool ok = run_one(&m, &cpu, code, 3, 0, true);

        snprintf(name, sizeof name, "W%u REXT device 0 executes without a trap", r);
        CHECK(ok && !nd500_trap_occurred() && s_read_calls == 1 && s_read_device == 0, name);
        snprintf(name, sizeof name, "W%u REXT puts the word in register %u", r, r);
        CHECK(nd500_read_integer_register(&cpu, (int)r) == 0x0000A500u + r, name);
        bool others = true;
        for (unsigned k = 1; k <= 4; k++) {
            if (k != r && nd500_read_integer_register(&cpu, (int)k) != 0xEEEE0000u + k) {
                others = false;
            }
        }
        snprintf(name, sizeof name, "W%u REXT leaves the other three registers alone", r);
        CHECK(others, name);
        snprintf(name, sizeof name, "W%u REXT clears K when a word was read", r);
        CHECK((cpu.ST1 & ND500_FLAG_K) == 0, name);
    }

    {
        const uint8_t code[] = { 0xFF, 0xE8, 0x40 };

        nd500_write_integer_register(&cpu, 1, 0xDEADBEEFu);
        cpu.ST1 &= ~ND500_FLAG_K;
        s_read_answer = 1;
        (void)run_one(&m, &cpu, code, 3, 0, true);
        CHECK(s_read_calls == 1 && (cpu.ST1 & ND500_FLAG_K) != 0,
              "REXT sets K when nothing was read");
        CHECK(nd500_read_integer_register(&cpu, 1) == 0u, "and the register gets 0");

        nd500_write_integer_register(&cpu, 1, 0xDEADBEEFu);
        cpu.ST1 &= ~ND500_FLAG_K;
        s_read_answer = 0;
        s_read_value = 0x12345678u;
        (void)run_one(&m, &cpu, code, 3, 9, true);
        CHECK(s_read_calls == 0 && (cpu.ST1 & ND500_FLAG_K) != 0
              && nd500_read_integer_register(&cpu, 1) == 0u,
              "REXT device 9 sets K, reads nothing and the register gets 0");

        nd500_write_integer_register(&cpu, 1, 0xDEADBEEFu);
        (void)run_one(&m, &cpu, code, 3, 0, false);
        CHECK(nd500_trap_occurred(), "REXT without privilege traps");
        CHECK(s_read_calls == 0 && nd500_read_integer_register(&cpu, 1) == 0xDEADBEEFu,
              "and leaves the register alone");
        nd500_trap_clear();

        nd500_set_ext_device_hooks(NULL, NULL);
        nd500_write_integer_register(&cpu, 1, 0xDEADBEEFu);
        cpu.ST1 &= ~ND500_FLAG_K;
        bool ok = run_one(&m, &cpu, code, 3, 0, true);
        CHECK(ok && !nd500_trap_occurred() && (cpu.ST1 & ND500_FLAG_K) != 0
              && nd500_read_integer_register(&cpu, 1) == 0u,
              "REXT with no device installed sets K, gives 0 and does not trap");
    }

    /* ---- CLINIT and CLREAD ------------------------------------------------- */
    printf("=== CLINIT / CLREAD ===\n");
    {
        const uint8_t clinit[] = { 0xFF, 0x1E };
        const uint8_t clread[] = { 0xFF, 0x1F };

        (void)run_one(&m, &cpu, clinit, 2, 0, false);
        CHECK(nd500_trap_occurred(), "CLINIT without privilege traps");
        nd500_trap_clear();

        uint32_t st1_before = (cpu.ST1 | ND500_FLAG_PIA) | ND500_FLAG_Z | ND500_FLAG_S;
        cpu.ST1 = st1_before;
        bool ok = run_one(&m, &cpu, clinit, 2, 0, true);
        CHECK(ok && !nd500_trap_occurred() && cpu.PC == CODE_AT + 2,
              "CLINIT executes and is two bytes long");
        CHECK(cpu.ST1 == st1_before, "CLINIT leaves the status bits alone");

        sleep_ms(20);
        nd500_write_integer_register(&cpu, 1, 0xDEADBEEFu);
        nd500_write_integer_register(&cpu, 2, 0xEEEE0002u);
        ok = run_one(&m, &cpu, clread, 2, 0, false);
        uint32_t first = nd500_read_integer_register(&cpu, 1);
        printf("  CLREAD 20 ms after CLINIT: %u microseconds\n", first);
        CHECK(ok && !nd500_trap_occurred(), "CLREAD executes without privilege");
        CHECK(first >= 20000u && first < 5000000u,
              "CLREAD gives W1 the microseconds since CLINIT (20 ms slept, under 5 s)");
        CHECK(nd500_read_integer_register(&cpu, 2) == 0xEEEE0002u, "and leaves W2 alone");
        CHECK((cpu.ST1 & (ND500_FLAG_Z | ND500_FLAG_S)) == 0,
              "a positive non-zero clock clears Z and S");

        sleep_ms(5);
        (void)run_one(&m, &cpu, clread, 2, 0, false);
        uint32_t second = nd500_read_integer_register(&cpu, 1);
        CHECK(second >= first + 5000u, "the clock keeps counting between two CLREADs");

        sleep_ms(5);
        (void)run_one(&m, &cpu, clinit, 2, 0, true);
        (void)run_one(&m, &cpu, clread, 2, 0, false);
        uint32_t third = nd500_read_integer_register(&cpu, 1);
        printf("  CLREAD right after a second CLINIT: %u microseconds\n", third);
        CHECK(third < second, "a second CLINIT starts the clock from zero again");
    }

    /* The clock is per CPU: a second CPU's CLINIT does not reset the first. */
    {
        const uint8_t clinit[] = { 0xFF, 0x1E };
        const uint8_t clread[] = { 0xFF, 0x1F };
        Nd500Cpu other;
        nd500_cpu_init(&other, &m);

        (void)run_one(&m, &cpu, clinit, 2, 0, true);
        sleep_ms(20);
        (void)run_one(&m, &other, clinit, 2, 0, true);
        (void)run_one(&m, &cpu, clread, 2, 0, false);
        (void)run_one(&m, &other, clread, 2, 0, false);
        uint32_t mine = nd500_read_integer_register(&cpu, 1);
        uint32_t theirs = nd500_read_integer_register(&other, 1);
        printf("  two CPUs: %u and %u microseconds\n", mine, theirs);
        CHECK(mine >= 20000u && theirs < mine,
              "each CPU has its own clock");
    }

    printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    nd500_machine_free(&m);
    return tests_failed == 0 ? 0 : 1;
}
