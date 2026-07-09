/*
 * MON Call Unit Tests
 *
 * Tests for SINTRAN III Monitor Call implementations.
 * Validates that MON call registry and helpers work correctly.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/nd500_mmu.h"
#include "../src/cpu/nd500_domain.h"
#include "../src/cpu/instruction_helpers.h"
#include "../src/libmon/mon.h"
#include "../src/libmon/mon_file_table.h"

/* Test counters */
static int tests_passed = 0;
static int tests_failed = 0;

/* ============================================================
 * Queued Console I/O for testing
 *
 * Allows queuing input characters for MON calls that read from
 * console (MON 1B INBT, MON 503B DVINST, etc.)
 * ============================================================ */

#define QUEUED_CONSOLE_MAX 256

typedef struct {
    char buffer[QUEUED_CONSOLE_MAX];
    size_t read_pos;
    size_t write_pos;
    size_t count;
    /* Output capture */
    char output[QUEUED_CONSOLE_MAX];
    size_t output_len;
} QueuedConsoleState;

static QueuedConsoleState g_queued_console = {0};

static void queued_console_reset(void) {
    memset(&g_queued_console, 0, sizeof(g_queued_console));
}

static void queued_console_queue_string(const char* str) {
    while (*str && g_queued_console.count < QUEUED_CONSOLE_MAX) {
        g_queued_console.buffer[g_queued_console.write_pos] = *str++;
        g_queued_console.write_pos = (g_queued_console.write_pos + 1) % QUEUED_CONSOLE_MAX;
        g_queued_console.count++;
    }
}

static bool queued_console_char_available(void* ctx) {
    (void)ctx;
    return g_queued_console.count > 0;
}

static int queued_console_read_char(void* ctx) {
    (void)ctx;
    if (g_queued_console.count == 0) {
        return -1;  /* EOF */
    }
    char ch = g_queued_console.buffer[g_queued_console.read_pos];
    g_queued_console.read_pos = (g_queued_console.read_pos + 1) % QUEUED_CONSOLE_MAX;
    g_queued_console.count--;
    return (unsigned char)ch;
}

static void queued_console_write_char(void* ctx, int ch) {
    (void)ctx;
    if (g_queued_console.output_len < QUEUED_CONSOLE_MAX - 1) {
        g_queued_console.output[g_queued_console.output_len++] = (char)ch;
        g_queued_console.output[g_queued_console.output_len] = '\0';
    }
}

static ConsoleIO g_test_console = {
    .read_char = queued_console_read_char,
    .write_char = queued_console_write_char,
    .char_available = queued_console_char_available,
    .context = NULL
};

#define TEST_PASS(name) do { \
    printf("  PASS: %s\n", name); \
    tests_passed++; \
} while(0)

#define TEST_FAIL(name, msg) do { \
    printf("  FAIL: %s - %s\n", name, msg); \
    tests_failed++; \
} while(0)

/* Test fixtures */
static Nd500Machine machine;
static Nd500Cpu cpu;

/* Memory access callbacks for MON context (direct, no MMU) */
static uint32_t test_read_word(void* cpu_ptr, uint32_t addr) {
    (void)cpu_ptr;
    /* Read 32-bit word (big-endian on ND-500) */
    uint32_t value = 0;
    value |= (uint32_t)nd500_bus_read8(&machine, addr) << 24;
    value |= (uint32_t)nd500_bus_read8(&machine, addr + 1) << 16;
    value |= (uint32_t)nd500_bus_read8(&machine, addr + 2) << 8;
    value |= (uint32_t)nd500_bus_read8(&machine, addr + 3);
    return value;
}

static void test_write_word(void* cpu_ptr, uint32_t addr, uint32_t val) {
    (void)cpu_ptr;
    /* Write 32-bit word (big-endian on ND-500) */
    nd500_bus_write8(&machine, addr, (uint8_t)(val >> 24));
    nd500_bus_write8(&machine, addr + 1, (uint8_t)(val >> 16));
    nd500_bus_write8(&machine, addr + 2, (uint8_t)(val >> 8));
    nd500_bus_write8(&machine, addr + 3, (uint8_t)val);
}

static uint16_t test_read_halfword(void* cpu_ptr, uint32_t addr) {
    (void)cpu_ptr;
    /* Read 16-bit halfword (big-endian on ND-500) */
    uint16_t value = 0;
    value |= (uint16_t)nd500_bus_read8(&machine, addr) << 8;
    value |= (uint16_t)nd500_bus_read8(&machine, addr + 1);
    return value;
}

static void test_write_halfword(void* cpu_ptr, uint32_t addr, uint16_t val) {
    (void)cpu_ptr;
    /* Write 16-bit halfword (big-endian on ND-500) */
    nd500_bus_write8(&machine, addr, (uint8_t)(val >> 8));
    nd500_bus_write8(&machine, addr + 1, (uint8_t)val);
}

static uint8_t test_read_byte(void* cpu_ptr, uint32_t addr) {
    (void)cpu_ptr;
    return nd500_bus_read8(&machine, addr);
}

static void test_write_byte(void* cpu_ptr, uint32_t addr, uint8_t val) {
    (void)cpu_ptr;
    nd500_bus_write8(&machine, addr, val);
}

static void test_set_k_flag(void* cpu_ptr, int value) {
    Nd500Cpu* c = (Nd500Cpu*)cpu_ptr;
    if (c) {
        if (value) {
            c->ST1 |= ND500_FLAG_K;  /* Set K flag */
        } else {
            c->ST1 &= ~ND500_FLAG_K; /* Clear K flag */
        }
    }
}

static void test_set_error_code(void* cpu_ptr, int32_t code) {
    Nd500Cpu* c = (Nd500Cpu*)cpu_ptr;
    if (c) {
        c->I[0] = (uint32_t)code;  /* W1/I1 */
    }
}

static void test_set_i1(void* cpu_ptr, uint32_t value) {
    Nd500Cpu* c = (Nd500Cpu*)cpu_ptr;
    if (c) {
        c->I[0] = value;
    }
}

static uint32_t test_get_i1(void* cpu_ptr) {
    Nd500Cpu* c = (Nd500Cpu*)cpu_ptr;
    return c ? c->I[0] : 0;
}

/* Setup a MonContext for testing */
static void setup_mon_context(MonContext* ctx, uint32_t mon_number,
                              uint32_t arg_count, uint32_t* args) {
    memset(ctx, 0, sizeof(*ctx));

    ctx->cpu = &cpu;
    ctx->machine = &machine;
    ctx->mon_number = mon_number;
    ctx->arg_count = arg_count;

    for (uint32_t i = 0; i < arg_count && i < MON_MAX_ARGS; i++) {
        ctx->arg_addresses[i] = args[i];
    }

    ctx->read_word = test_read_word;
    ctx->write_word = test_write_word;
    ctx->read_halfword = test_read_halfword;
    ctx->write_halfword = test_write_halfword;
    ctx->read_byte = test_read_byte;
    ctx->write_byte = test_write_byte;
    ctx->set_k_flag = test_set_k_flag;
    ctx->set_error_code = test_set_error_code;
    ctx->set_i1 = test_set_i1;
    ctx->get_i1 = test_get_i1;
}

static void setup(void) {
    nd500_machine_init(&machine, 16 * 1024 * 1024);  /* 16MB */
    nd500_cpu_init(&cpu, &machine);
    nd500_cpu_reset(&cpu);
    mon_init();
    mon_file_table_init();
}

static void teardown(void) {
    mon_file_table_reset();
    nd500_machine_free(&machine);
}

/*
 * Test MON registry functions
 */
static void test_mon_registry(void) {
    printf("\nTesting MON registry...\n");
    setup();

    /* Test MON 0B LEAVE */
    const char* name = mon_get_name(0);
    if (name && strcmp(name, "LEAVE") == 0) {
        TEST_PASS("MON 0 name is LEAVE");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "got %s", name ? name : "NULL");
        TEST_FAIL("MON 0 name is LEAVE", msg);
    }

    /* Test MON 11B (9 decimal) TIME */
    name = mon_get_name(9);
    if (name && strcmp(name, "TIME") == 0) {
        TEST_PASS("MON 11B name is TIME");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "got %s", name ? name : "NULL");
        TEST_FAIL("MON 11B name is TIME", msg);
    }

    /* Test MON 113B (75 decimal) CLOCK - should be VALIDATED now */
    MonImplStatus status = mon_get_status(75);
    if (status == MON_STATUS_VALIDATED) {
        TEST_PASS("MON 113B status is VALIDATED");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "status=%d", status);
        TEST_FAIL("MON 113B status is VALIDATED", msg);
    }

    /* Test total count > 0 */
    int total = mon_get_total_count();
    if (total > 200) {
        TEST_PASS("Registry has >200 MON calls");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "total=%d", total);
        TEST_FAIL("Registry has >200 MON calls", msg);
    }

    teardown();
}

/*
 * Test MON 113B CLOCK (GetCurrentTime)
 * Verifies that time values are within valid ranges
 */
static void test_mon_113B_clock(void) {
    printf("\nTesting MON 113B CLOCK (GetCurrentTime)...\n");
    setup();

    /* Set up buffer at address 0x1000 */
    uint32_t buffer_addr = 0x1000;
    uint32_t args[1] = { buffer_addr };

    /* Create context and dispatch */
    MonContext ctx;
    setup_mon_context(&ctx, 75, 1, args);  /* 113B = 75 decimal */

    MonResult result = mon_dispatch(&ctx);

    if (result == MON_SUCCESS) {
        TEST_PASS("MON 113B returns success");
    } else {
        TEST_FAIL("MON 113B returns success", "returned error");
        teardown();
        return;
    }

    /* Read time values from buffer (big-endian) */
    uint32_t basic_units = test_read_word(&cpu, buffer_addr + 0);
    uint32_t seconds = test_read_word(&cpu, buffer_addr + 4);
    uint32_t minutes = test_read_word(&cpu, buffer_addr + 8);
    uint32_t hours = test_read_word(&cpu, buffer_addr + 12);
    uint32_t day = test_read_word(&cpu, buffer_addr + 16);
    uint32_t month = test_read_word(&cpu, buffer_addr + 20);
    uint32_t year = test_read_word(&cpu, buffer_addr + 24);

    /* Validate ranges */
    if (seconds <= 59) {
        TEST_PASS("Seconds in range 0-59");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "seconds=%u", seconds);
        TEST_FAIL("Seconds in range 0-59", msg);
    }

    if (minutes <= 59) {
        TEST_PASS("Minutes in range 0-59");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "minutes=%u", minutes);
        TEST_FAIL("Minutes in range 0-59", msg);
    }

    if (hours <= 23) {
        TEST_PASS("Hours in range 0-23");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "hours=%u", hours);
        TEST_FAIL("Hours in range 0-23", msg);
    }

    if (day >= 1 && day <= 31) {
        TEST_PASS("Day in range 1-31");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "day=%u", day);
        TEST_FAIL("Day in range 1-31", msg);
    }

    if (month >= 1 && month <= 12) {
        TEST_PASS("Month in range 1-12");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "month=%u", month);
        TEST_FAIL("Month in range 1-12", msg);
    }

    if (year <= 99) {
        TEST_PASS("Year in range 0-99");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "year=%u", year);
        TEST_FAIL("Year in range 0-99", msg);
    }

    /* Validate basic units calculation */
    uint32_t expected_basic = (hours * 3600 + minutes * 60 + seconds) * 50;
    if (basic_units == expected_basic) {
        TEST_PASS("Basic units calculation correct");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "expected=%u, got=%u", expected_basic, basic_units);
        TEST_FAIL("Basic units calculation correct", msg);
    }

    teardown();
}

/*
 * Test MON 113B CLOCK with missing arguments
 */
static void test_mon_113B_clock_missing_args(void) {
    printf("\nTesting MON 113B CLOCK error handling...\n");
    setup();

    /* Create context with 0 arguments */
    MonContext ctx;
    setup_mon_context(&ctx, 75, 0, NULL);

    MonResult result = mon_dispatch(&ctx);

    if (result == MON_ERROR) {
        TEST_PASS("MON 113B returns error with no arguments");
    } else {
        TEST_FAIL("MON 113B returns error with no arguments", "should fail");
    }

    teardown();
}

/*
 * Test MON 312B MOINF (CheckMonCall)
 */
static void test_mon_312B_moinf(void) {
    printf("\nTesting MON 312B MOINF (CheckMonCall)...\n");
    setup();

    /* Set up parameters */
    uint32_t mon_number_loc = 0x1000;
    uint32_t result_loc = 0x1004;

    /* Check MON 0 (LEAVE) - should exist */
    test_write_word(&cpu, mon_number_loc, 0);
    test_write_word(&cpu, result_loc, 0xFFFFFFFF);  /* Pre-fill with garbage */

    uint32_t args[2] = { mon_number_loc, result_loc };
    MonContext ctx;
    setup_mon_context(&ctx, 202, 2, args);  /* 312B = 202 decimal */

    MonResult result = mon_dispatch(&ctx);

    if (result == MON_SUCCESS) {
        TEST_PASS("MON 312B returns success");
    } else {
        TEST_FAIL("MON 312B returns success", "returned error");
        teardown();
        return;
    }

    /* Implemented call: entry address is the shared emulator convention
     * 0xF8000000 + mon_number (segment 31 + call number). MON 0 -> 0xF8000000.
     * Provenance: convention (agreed with the C# emulator), not manual text. */
    uint32_t check_result = test_read_word(&cpu, result_loc);
    if (check_result == 0xF8000000u) {
        TEST_PASS("MON 312B implemented call returns entry 0xF8000000+n");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "result=0x%08X (expected 0xF8000000)", check_result);
        TEST_FAIL("MON 312B implemented call returns entry 0xF8000000+n", msg);
    }

    /* Non-existent call (200B = 128 decimal, unused) must return 0 */
    test_write_word(&cpu, mon_number_loc, 128);
    test_write_word(&cpu, result_loc, 0xFFFFFFFF);
    setup_mon_context(&ctx, 202, 2, args);
    result = mon_dispatch(&ctx);
    check_result = test_read_word(&cpu, result_loc);
    if (result == MON_SUCCESS && check_result == 0) {
        TEST_PASS("MON 312B non-existent call returns 0");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "result=0x%08X (expected 0)", check_result);
        TEST_FAIL("MON 312B non-existent call returns 0", msg);
    }

    /* Deprecated call (321B = 209 decimal) must also return 0 */
    test_write_word(&cpu, mon_number_loc, 209);
    test_write_word(&cpu, result_loc, 0xFFFFFFFF);
    setup_mon_context(&ctx, 202, 2, args);
    result = mon_dispatch(&ctx);
    check_result = test_read_word(&cpu, result_loc);
    if (result == MON_SUCCESS && check_result == 0) {
        TEST_PASS("MON 312B deprecated call returns 0");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "result=0x%08X (expected 0)", check_result);
        TEST_FAIL("MON 312B deprecated call returns 0", msg);
    }

    teardown();
}

/*
 * Test MON 62B RMAX (GetBytesInFile) - error case for unopened file
 */
static void test_mon_62B_rmax_file_not_open(void) {
    printf("\nTesting MON 62B RMAX error handling...\n");
    setup();

    /* Set up parameters for file 64 (not opened) */
    uint32_t file_no_loc = 0x1000;
    uint32_t result_loc = 0x1004;

    test_write_word(&cpu, file_no_loc, 64);  /* File 64 */

    uint32_t args[2] = { file_no_loc, result_loc };
    MonContext ctx;
    setup_mon_context(&ctx, 50, 2, args);  /* 62B = 50 decimal */

    MonResult result = mon_dispatch(&ctx);

    /* Should fail because file is not open */
    if (result == MON_ERROR) {
        TEST_PASS("MON 62B returns error for unopened file");
    } else {
        TEST_FAIL("MON 62B returns error for unopened file", "should fail");
    }

    teardown();
}

/*
 * Test MON 12B SETCM (SetCommandBuffer)
 */
static void test_mon_12B_setcm(void) {
    printf("\nTesting MON 12B SETCM (SetCommandBuffer)...\n");
    setup();

    /* Set up command string in memory (apostrophe terminated per SINTRAN) */
    uint32_t string_addr = 0x1000;
    const char* test_cmd = "TEST-COMMAND";

    /* Write the command string to memory (0x27 terminated) */
    for (size_t i = 0; i < strlen(test_cmd); i++) {
        nd500_bus_write8(&machine, string_addr + i, (uint8_t)test_cmd[i]);
    }
    nd500_bus_write8(&machine, string_addr + strlen(test_cmd), 0x27); /* Apostrophe terminator */

    uint32_t args[1] = { string_addr };
    MonContext ctx;
    setup_mon_context(&ctx, 10, 1, args);  /* 12B = 10 decimal */

    MonResult result = mon_dispatch(&ctx);

    if (result == MON_SUCCESS) {
        TEST_PASS("MON 12B returns success");
    } else {
        TEST_FAIL("MON 12B returns success", "returned error");
    }

    /* Verify the command buffer was set */
    const char* buffer = mon_get_command_buffer();
    if (buffer && strcmp(buffer, test_cmd) == 0) {
        TEST_PASS("Command buffer contains correct string");
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "got '%s'", buffer ? buffer : "NULL");
        TEST_FAIL("Command buffer contains correct string", msg);
    }

    teardown();
}

/*
 * Test MON 12B SETCM with missing arguments
 */
static void test_mon_12B_setcm_missing_args(void) {
    printf("\nTesting MON 12B SETCM error handling...\n");
    setup();

    MonContext ctx;
    setup_mon_context(&ctx, 10, 0, NULL);  /* 12B = 10 decimal, 0 args */

    MonResult result = mon_dispatch(&ctx);

    if (result == MON_ERROR) {
        TEST_PASS("MON 12B returns error with no arguments");
    } else {
        TEST_FAIL("MON 12B returns error with no arguments", "should fail");
    }

    teardown();
}

/*
 * Test MON 256B DEABF (FullFileName)
 */
static void test_mon_256B_deabf(void) {
    printf("\nTesting MON 256B DEABF (FullFileName)...\n");
    setup();

    /* Set up memory for strings */
    uint32_t abbrev_addr = 0x1000;
    uint32_t output_addr = 0x1100;
    uint32_t type_addr = 0x1200;

    /* Write abbreviated filename (0x27 terminated) */
    const char* abbrev = "MYFILE";
    for (size_t i = 0; i < strlen(abbrev); i++) {
        nd500_bus_write8(&machine, abbrev_addr + i, (uint8_t)abbrev[i]);
    }
    nd500_bus_write8(&machine, abbrev_addr + strlen(abbrev), 0x27);

    /* Write file type (0x27 terminated) */
    const char* type = "TXT";
    for (size_t i = 0; i < strlen(type); i++) {
        nd500_bus_write8(&machine, type_addr + i, (uint8_t)type[i]);
    }
    nd500_bus_write8(&machine, type_addr + strlen(type), 0x27);

    uint32_t args[3] = { abbrev_addr, output_addr, type_addr };
    MonContext ctx;
    setup_mon_context(&ctx, 174, 3, args);  /* 256B = 174 decimal */

    MonResult result = mon_dispatch(&ctx);

    if (result == MON_SUCCESS) {
        TEST_PASS("MON 256B returns success");
    } else {
        TEST_FAIL("MON 256B returns success", "returned error");
        teardown();
        return;
    }

    /* Read output string */
    char output[128];
    size_t i = 0;
    while (i < sizeof(output) - 1) {
        uint8_t ch = nd500_bus_read8(&machine, output_addr + i);
        if (ch == 0x27) break;  /* SINTRAN terminator */
        output[i] = (char)ch;
        i++;
    }
    output[i] = '\0';

    /* Verify output contains filename.type */
    if (strcmp(output, "MYFILE.TXT") == 0) {
        TEST_PASS("Full filename is MYFILE.TXT");
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "got '%s'", output);
        TEST_FAIL("Full filename is MYFILE.TXT", msg);
    }

    teardown();
}

/*
 * Test MON 256B DEABF with existing type in filename
 */
static void test_mon_256B_deabf_with_existing_type(void) {
    printf("\nTesting MON 256B DEABF with existing type...\n");
    setup();

    /* Set up memory for strings */
    uint32_t abbrev_addr = 0x1000;
    uint32_t output_addr = 0x1100;
    uint32_t type_addr = 0x1200;

    /* Write abbreviated filename with type already (0x27 terminated) */
    const char* abbrev = "MYFILE.DAT";
    for (size_t i = 0; i < strlen(abbrev); i++) {
        nd500_bus_write8(&machine, abbrev_addr + i, (uint8_t)abbrev[i]);
    }
    nd500_bus_write8(&machine, abbrev_addr + strlen(abbrev), 0x27);

    /* Write default file type (should be ignored) */
    const char* type = "TXT";
    for (size_t i = 0; i < strlen(type); i++) {
        nd500_bus_write8(&machine, type_addr + i, (uint8_t)type[i]);
    }
    nd500_bus_write8(&machine, type_addr + strlen(type), 0x27);

    uint32_t args[3] = { abbrev_addr, output_addr, type_addr };
    MonContext ctx;
    setup_mon_context(&ctx, 174, 3, args);  /* 256B = 174 decimal */

    MonResult result = mon_dispatch(&ctx);

    if (result == MON_SUCCESS) {
        TEST_PASS("MON 256B returns success");
    } else {
        TEST_FAIL("MON 256B returns success", "returned error");
        teardown();
        return;
    }

    /* Read output string */
    char output[128];
    size_t i = 0;
    while (i < sizeof(output) - 1) {
        uint8_t ch = nd500_bus_read8(&machine, output_addr + i);
        if (ch == 0x27) break;
        output[i] = (char)ch;
        i++;
    }
    output[i] = '\0';

    /* Existing type should be preserved */
    if (strcmp(output, "MYFILE.DAT") == 0) {
        TEST_PASS("Existing type preserved: MYFILE.DAT");
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "got '%s'", output);
        TEST_FAIL("Existing type preserved: MYFILE.DAT", msg);
    }

    teardown();
}

/*
 * Test MON 2B OUTBT (OutByte) - write to console
 */
static void test_mon_2B_outbt_console(void) {
    printf("\nTesting MON 2B OUTBT (OutByte) to console...\n");
    setup();

    /* Set up parameters */
    uint32_t device_no_loc = 0x1000;
    uint32_t value_loc = 0x1004;

    /* Device 0 (console), write 'A' */
    test_write_word(&cpu, device_no_loc, 0);
    test_write_word(&cpu, value_loc, 'A');

    uint32_t args[2] = { device_no_loc, value_loc };
    MonContext ctx;
    setup_mon_context(&ctx, 2, 2, args);  /* 2B = 2 decimal */

    MonResult result = mon_dispatch(&ctx);

    if (result == MON_SUCCESS) {
        TEST_PASS("MON 2B returns success for console write");
    } else {
        TEST_FAIL("MON 2B returns success for console write", "returned error");
    }

    teardown();
}

/*
 * Test MON 2B OUTBT - file not open error
 */
static void test_mon_2B_outbt_file_not_open(void) {
    printf("\nTesting MON 2B OUTBT file not open...\n");
    setup();

    /* Set up parameters */
    uint32_t device_no_loc = 0x1000;
    uint32_t value_loc = 0x1004;

    /* Device 80 (mass storage) - not opened */
    test_write_word(&cpu, device_no_loc, 80);
    test_write_word(&cpu, value_loc, 'X');

    uint32_t args[2] = { device_no_loc, value_loc };
    MonContext ctx;
    setup_mon_context(&ctx, 2, 2, args);  /* 2B = 2 decimal */

    MonResult result = mon_dispatch(&ctx);

    if (result == MON_ERROR) {
        TEST_PASS("MON 2B returns error for unopened file");
    } else {
        TEST_FAIL("MON 2B returns error for unopened file", "should fail");
    }

    teardown();
}

/*
 * Test MON 2B OUTBT - missing arguments
 */
static void test_mon_2B_outbt_missing_args(void) {
    printf("\nTesting MON 2B OUTBT missing arguments...\n");
    setup();

    /* Only provide 1 arg when 2 are needed */
    uint32_t device_no_loc = 0x1000;
    test_write_word(&cpu, device_no_loc, 0);

    uint32_t args[1] = { device_no_loc };
    MonContext ctx;
    setup_mon_context(&ctx, 2, 1, args);  /* 2B = 2 decimal, 1 arg */

    MonResult result = mon_dispatch(&ctx);

    if (result == MON_ERROR) {
        TEST_PASS("MON 2B returns error with insufficient arguments");
    } else {
        TEST_FAIL("MON 2B returns error with insufficient arguments", "should fail");
    }

    teardown();
}

/*
 * Test MON 503B DVINST (InputString) - error case for unopened file
 */
static void test_mon_503B_dvinst_file_not_open(void) {
    printf("\nTesting MON 503B DVINST (InputString) error handling...\n");
    setup();

    /* Set up parameters */
    uint32_t dev_no_loc = 0x1000;
    uint32_t max_no_loc = 0x1004;
    uint32_t ret_count_loc = 0x1008;
    uint32_t buffer_loc = 0x1100;

    /* Device 80 (mass storage) - not opened */
    test_write_word(&cpu, dev_no_loc, 80);
    test_write_word(&cpu, max_no_loc, 100);
    test_write_word(&cpu, ret_count_loc, 0xDEADBEEF);  /* Pre-fill with garbage */

    uint32_t args[4] = { dev_no_loc, max_no_loc, ret_count_loc, buffer_loc };
    MonContext ctx;
    setup_mon_context(&ctx, 323, 4, args);  /* 503B = 323 decimal */

    MonResult result = mon_dispatch(&ctx);

    /* Should fail because file is not open */
    if (result == MON_ERROR) {
        TEST_PASS("MON 503B returns error for unopened file");
    } else {
        TEST_FAIL("MON 503B returns error for unopened file", "should fail");
    }

    teardown();
}

/*
 * Test MON 503B DVINST with missing arguments
 */
static void test_mon_503B_dvinst_missing_args(void) {
    printf("\nTesting MON 503B DVINST missing args...\n");
    setup();

    /* Only provide 2 args when 4 are needed */
    uint32_t dev_no_loc = 0x1000;
    uint32_t max_no_loc = 0x1004;
    test_write_word(&cpu, dev_no_loc, 0);
    test_write_word(&cpu, max_no_loc, 10);

    uint32_t args[2] = { dev_no_loc, max_no_loc };
    MonContext ctx;
    setup_mon_context(&ctx, 323, 2, args);  /* 503B = 323 decimal, only 2 args (need 4) */

    MonResult result = mon_dispatch(&ctx);

    if (result == MON_ERROR) {
        TEST_PASS("MON 503B returns error with insufficient arguments");
    } else {
        TEST_FAIL("MON 503B returns error with insufficient arguments", "should fail");
    }

    teardown();
}

/*
 * Test MON 503B DVINST with max bytes too large
 */
static void test_mon_503B_dvinst_max_bytes_exceeded(void) {
    printf("\nTesting MON 503B DVINST max bytes exceeded...\n");
    setup();

    /* Set up parameters */
    uint32_t dev_no_loc = 0x1000;
    uint32_t max_no_loc = 0x1004;
    uint32_t ret_count_loc = 0x1008;
    uint32_t buffer_loc = 0x1100;

    /* Device 0 (console), but request more than 2048 bytes */
    test_write_word(&cpu, dev_no_loc, 0);
    test_write_word(&cpu, max_no_loc, 3000);  /* Exceeds DVINST_MAX_BYTES (2048) */

    uint32_t args[4] = { dev_no_loc, max_no_loc, ret_count_loc, buffer_loc };
    MonContext ctx;
    setup_mon_context(&ctx, 323, 4, args);  /* 503B = 323 decimal */

    MonResult result = mon_dispatch(&ctx);

    if (result == MON_ERROR) {
        TEST_PASS("MON 503B returns error when max bytes exceeded");
    } else {
        TEST_FAIL("MON 503B returns error when max bytes exceeded", "should fail");
    }

    teardown();
}

/*
 * Test MON 503B DVINST with zero bytes (should succeed immediately)
 */
static void test_mon_503B_dvinst_zero_bytes(void) {
    printf("\nTesting MON 503B DVINST zero bytes...\n");
    setup();

    /* Set up parameters */
    uint32_t dev_no_loc = 0x1000;
    uint32_t max_no_loc = 0x1004;
    uint32_t ret_count_loc = 0x1008;
    uint32_t buffer_loc = 0x1100;

    /* Device 0 (console), request 0 bytes */
    test_write_word(&cpu, dev_no_loc, 0);
    test_write_word(&cpu, max_no_loc, 0);
    test_write_word(&cpu, ret_count_loc, 0xDEADBEEF);

    uint32_t args[4] = { dev_no_loc, max_no_loc, ret_count_loc, buffer_loc };
    MonContext ctx;
    setup_mon_context(&ctx, 323, 4, args);  /* 503B = 323 decimal */

    MonResult result = mon_dispatch(&ctx);

    if (result == MON_SUCCESS) {
        TEST_PASS("MON 503B returns success for zero bytes");
    } else {
        TEST_FAIL("MON 503B returns success for zero bytes", "should succeed");
    }

    /* Verify return count is 0 */
    uint32_t ret_count = test_read_word(&cpu, ret_count_loc);
    if (ret_count == 0) {
        TEST_PASS("MON 503B returns 0 bytes read");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "got %u", ret_count);
        TEST_FAIL("MON 503B returns 0 bytes read", msg);
    }

    teardown();
}

/*
 * Test MON 503B DVINST with queued "HELP\r\n" input
 *
 * Verifies that MON 503B reads input correctly from a queued console.
 * Uses break strategy 2 (MAC) which breaks on CR, LF, ESC, EOF, 0x27.
 */
static void test_mon_503B_dvinst_queued_input(void) {
    printf("\nTesting MON 503B DVINST with queued HELP input...\n");
    setup();

    /* Set up queued console with "HELP\r\n" */
    queued_console_reset();
    queued_console_queue_string("HELP\r\n");
    mon_file_table_set_console(&g_test_console);

    /* Set up parameters */
    uint32_t dev_no_loc = 0x1000;
    uint32_t max_no_loc = 0x1004;
    uint32_t ret_count_loc = 0x1008;
    uint32_t buffer_loc = 0x1100;
    uint32_t break_strat_loc = 0x100C;
    uint32_t echo_strat_loc = 0x1010;

    /* Device 0 (console), max 100 bytes, break strategy 2 (MAC), echo strategy 2 (MAC) */
    test_write_word(&cpu, dev_no_loc, 0);
    test_write_word(&cpu, max_no_loc, 100);
    test_write_word(&cpu, ret_count_loc, 0xDEADBEEF);
    test_write_word(&cpu, break_strat_loc, 2);  /* MAC strategy breaks on CR */
    test_write_word(&cpu, echo_strat_loc, 2);   /* MAC strategy echoes printable + CR/LF */

    /* Clear buffer */
    for (int i = 0; i < 16; i++) {
        nd500_bus_write8(&machine, buffer_loc + i, 0);
    }

    uint32_t args[6] = { dev_no_loc, max_no_loc, ret_count_loc, buffer_loc, break_strat_loc, echo_strat_loc };
    MonContext ctx;
    setup_mon_context(&ctx, 323, 6, args);  /* 503B = 323 decimal, 6 args */

    MonResult result = mon_dispatch(&ctx);

    if (result == MON_SUCCESS) {
        TEST_PASS("MON 503B returns success");
    } else {
        TEST_FAIL("MON 503B returns success", "returned error");
        mon_file_table_set_console(NULL);
        teardown();
        return;
    }

    /* Verify bytes read (should be 5: "HELP\r" - CR is break char, included) */
    uint32_t ret_count = test_read_word(&cpu, ret_count_loc);
    if (ret_count == 5) {
        TEST_PASS("MON 503B read 5 bytes (HELP + CR)");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "expected 5, got %u", ret_count);
        TEST_FAIL("MON 503B read 5 bytes (HELP + CR)", msg);
    }

    /* Verify buffer content */
    char read_buf[16];
    for (uint32_t i = 0; i < ret_count && i < 15; i++) {
        read_buf[i] = (char)nd500_bus_read8(&machine, buffer_loc + i);
    }
    read_buf[ret_count < 15 ? ret_count : 15] = '\0';

    if (ret_count >= 4 && strncmp(read_buf, "HELP", 4) == 0) {
        TEST_PASS("Buffer contains HELP");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "got '%s'", read_buf);
        TEST_FAIL("Buffer contains HELP", msg);
    }

    /* Verify CR was included */
    if (ret_count >= 5 && read_buf[4] == '\r') {
        TEST_PASS("Buffer contains CR break character");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "byte[4]=0x%02X", ret_count >= 5 ? (unsigned char)read_buf[4] : 0);
        TEST_FAIL("Buffer contains CR break character", msg);
    }

    /* Verify echo output: "HELP\r" should be echoed back */
    if (g_queued_console.output_len == 5 &&
        strncmp(g_queued_console.output, "HELP\r", 5) == 0) {
        TEST_PASS("Echo output is 'HELP\\r' (5 chars)");
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "echo_len=%zu, got '%s'",
                 g_queued_console.output_len, g_queued_console.output);
        TEST_FAIL("Echo output is 'HELP\\r' (5 chars)", msg);
    }

    /* Verify LF remains in queue (CR broke before LF was read) */
    if (g_queued_console.count == 1) {
        TEST_PASS("LF remains in input queue after CR break");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "remaining=%zu", g_queued_console.count);
        TEST_FAIL("LF remains in input queue after CR break", msg);
    }

    mon_file_table_set_console(NULL);
    teardown();
}

/*
 * Test MON 1B INBT with queued input
 */
static void test_mon_1B_inbt_queued_input(void) {
    printf("\nTesting MON 1B INBT with queued input...\n");
    setup();

    /* Set up queued console with 'X' */
    queued_console_reset();
    queued_console_queue_string("X");
    mon_file_table_set_console(&g_test_console);

    /* Set up parameters */
    uint32_t dev_no_loc = 0x1000;
    uint32_t value_loc = 0x1004;

    /* Device 1 = terminal (character device). Device 0 is the SINTRAN
     * command buffer, NOT the console - using 0 here read leftover
     * command-buffer text from the MON 12B test ('T' of "TEST-COMMAND"). */
    test_write_word(&cpu, dev_no_loc, 1);
    test_write_word(&cpu, value_loc, 0xDEAD);   /* Pre-fill with garbage */

    uint32_t args[2] = { dev_no_loc, value_loc };
    MonContext ctx;
    setup_mon_context(&ctx, 1, 2, args);  /* 1B = 1 decimal */

    MonResult result = mon_dispatch(&ctx);

    if (result == MON_SUCCESS) {
        TEST_PASS("MON 1B returns success");
    } else {
        TEST_FAIL("MON 1B returns success", "returned error");
        mon_file_table_set_console(NULL);
        teardown();
        return;
    }

    /* Verify character read */
    uint32_t value = test_read_word(&cpu, value_loc);
    if ((value & 0xFF) == 'X') {
        TEST_PASS("MON 1B read 'X' from queue");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "expected 'X' (0x58), got 0x%02X", value & 0xFF);
        TEST_FAIL("MON 1B read 'X' from queue", msg);
    }

    /* Agreed semantics (matches C# emulator): non-blocking, empty input
     * queue returns error 57 (EOF) instead of waiting */
    setup_mon_context(&ctx, 1, 2, args);
    result = mon_dispatch(&ctx);
    if (result == MON_ERROR && ctx.error_code == 57) {
        TEST_PASS("MON 1B empty queue returns EOF error 57 (non-blocking)");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "result=%d, error_code=%d", (int)result, ctx.error_code);
        TEST_FAIL("MON 1B empty queue returns EOF error 57 (non-blocking)", msg);
    }

    mon_file_table_set_console(NULL);
    teardown();
}

/*
 * Test MON 321B UEADM (UEAdministrator) - deprecated, always returns error
 */
static void test_mon_321B_ueadm_deprecated(void) {
    printf("\nTesting MON 321B UEADM (deprecated)...\n");
    setup();

    /* 321B is deprecated and should always return error 52 */
    MonContext ctx;
    uint32_t args[1] = { 0x1000 };
    setup_mon_context(&ctx, 209, 1, args);  /* 321B = 209 decimal */

    MonResult result = mon_dispatch(&ctx);

    if (result == MON_ERROR) {
        TEST_PASS("MON 321B returns error (deprecated)");
    } else {
        TEST_FAIL("MON 321B returns error (deprecated)", "should fail");
    }

    /* Verify error code is 52 (Invalid parameter) */
    if (cpu.I[0] == 52) {
        TEST_PASS("MON 321B error code is 52");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "got %u", cpu.I[0]);
        TEST_FAIL("MON 321B error code is 52", msg);
    }

    teardown();
}

/*
 * Test MON 317B UECOM (ExecuteCommand)
 */
static void test_mon_317B_uecom(void) {
    printf("\nTesting MON 317B UECOM (ExecuteCommand)...\n");
    setup();

    /* Set up command string in memory (apostrophe terminated per SINTRAN) */
    uint32_t string_addr = 0x1000;
    const char* test_cmd = "LIST-FILES";

    /* Write the command string to memory (0x27 terminated) */
    for (size_t i = 0; i < strlen(test_cmd); i++) {
        nd500_bus_write8(&machine, string_addr + i, (uint8_t)test_cmd[i]);
    }
    nd500_bus_write8(&machine, string_addr + strlen(test_cmd), 0x27); /* Apostrophe terminator */

    uint32_t args[1] = { string_addr };
    MonContext ctx;
    setup_mon_context(&ctx, 207, 1, args);  /* 317B = 207 decimal */

    MonResult result = mon_dispatch(&ctx);

    if (result == MON_SUCCESS) {
        TEST_PASS("MON 317B returns success");
    } else {
        TEST_FAIL("MON 317B returns success", "returned error");
    }

    teardown();
}

/*
 * Test MON 317B UECOM with missing arguments
 */
static void test_mon_317B_uecom_missing_args(void) {
    printf("\nTesting MON 317B UECOM error handling...\n");
    setup();

    MonContext ctx;
    setup_mon_context(&ctx, 207, 0, NULL);  /* 317B = 207 decimal, 0 args */

    MonResult result = mon_dispatch(&ctx);

    if (result == MON_ERROR) {
        TEST_PASS("MON 317B returns error with no arguments");
    } else {
        TEST_FAIL("MON 317B returns error with no arguments", "should fail");
    }

    teardown();
}

/*
 * Test MON 317B UECOM with empty command
 */
static void test_mon_317B_uecom_empty_command(void) {
    printf("\nTesting MON 317B UECOM empty command...\n");
    setup();

    /* Set up empty command string (just terminator) */
    uint32_t string_addr = 0x1000;
    nd500_bus_write8(&machine, string_addr, 0x27); /* Just terminator */

    uint32_t args[1] = { string_addr };
    MonContext ctx;
    setup_mon_context(&ctx, 207, 1, args);  /* 317B = 207 decimal */

    MonResult result = mon_dispatch(&ctx);

    /* Even empty command should succeed (stub just logs) */
    if (result == MON_SUCCESS) {
        TEST_PASS("MON 317B accepts empty command");
    } else {
        TEST_FAIL("MON 317B accepts empty command", "returned error");
    }

    teardown();
}

/*
 * Test MON 412B FSCNT (FileAsSegment) - file not open
 */
static void test_mon_412B_fscnt_file_not_open(void) {
    printf("\nTesting MON 412B FSCNT file not open...\n");
    setup();

    /* Set up parameters for non-open file */
    uint32_t file_no_loc = 0x1000;
    uint32_t seg_no_loc = 0x1004;
    uint32_t access_loc = 0x1008;
    uint32_t out_seg_loc = 0x100C;

    test_write_word(&cpu, file_no_loc, 64);   /* File 64 - not opened */
    test_write_word(&cpu, seg_no_loc, 1);     /* Segment 1 */
    test_write_word(&cpu, access_loc, 0);     /* Access type 0 */

    uint32_t args[4] = { file_no_loc, seg_no_loc, access_loc, out_seg_loc };
    MonContext ctx;
    setup_mon_context(&ctx, 266, 4, args);  /* 412B = 266 decimal */

    MonResult result = mon_dispatch(&ctx);

    if (result == MON_ERROR) {
        TEST_PASS("MON 412B returns error for unopened file");
    } else {
        TEST_FAIL("MON 412B returns error for unopened file", "should fail");
    }

    /* Check error code is 53 (file not open) */
    if (cpu.I[0] == 53) {
        TEST_PASS("MON 412B error code is 53 (file not open)");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "got %u", cpu.I[0]);
        TEST_FAIL("MON 412B error code is 53 (file not open)", msg);
    }

    teardown();
}

/*
 * Test MON 412B FSCNT missing args
 */
static void test_mon_412B_fscnt_missing_args(void) {
    printf("\nTesting MON 412B FSCNT missing args...\n");
    setup();

    uint32_t file_no_loc = 0x1000;
    uint32_t seg_no_loc = 0x1004;
    test_write_word(&cpu, file_no_loc, 64);
    test_write_word(&cpu, seg_no_loc, 1);

    uint32_t args[2] = { file_no_loc, seg_no_loc };
    MonContext ctx;
    setup_mon_context(&ctx, 266, 2, args);  /* Only 2 args, need 3 */

    MonResult result = mon_dispatch(&ctx);

    if (result == MON_ERROR) {
        TEST_PASS("MON 412B returns error with insufficient arguments");
    } else {
        TEST_FAIL("MON 412B returns error with insufficient arguments", "should fail");
    }

    teardown();
}

/*
 * Test MON 413B FSCDNT (FileNotAsSegment) - file not open
 */
static void test_mon_413B_fscdnt_file_not_open(void) {
    printf("\nTesting MON 413B FSCDNT file not open...\n");
    setup();

    uint32_t file_no_loc = 0x1000;
    uint32_t seg_no_loc = 0x1004;

    test_write_word(&cpu, file_no_loc, 64);   /* File 64 - not opened */
    test_write_word(&cpu, seg_no_loc, 1);     /* Segment 1 */

    uint32_t args[2] = { file_no_loc, seg_no_loc };
    MonContext ctx;
    setup_mon_context(&ctx, 267, 2, args);  /* 413B = 267 decimal */

    MonResult result = mon_dispatch(&ctx);

    if (result == MON_ERROR) {
        TEST_PASS("MON 413B returns error for unopened file");
    } else {
        TEST_FAIL("MON 413B returns error for unopened file", "should fail");
    }

    /* Check error code is 53 (file not open) */
    if (cpu.I[0] == 53) {
        TEST_PASS("MON 413B error code is 53 (file not open)");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "got %u", cpu.I[0]);
        TEST_FAIL("MON 413B error code is 53 (file not open)", msg);
    }

    teardown();
}

/*
 * Test MON 413B FSCDNT missing args
 */
static void test_mon_413B_fscdnt_missing_args(void) {
    printf("\nTesting MON 413B FSCDNT missing args...\n");
    setup();

    uint32_t file_no_loc = 0x1000;
    test_write_word(&cpu, file_no_loc, 64);

    uint32_t args[1] = { file_no_loc };
    MonContext ctx;
    setup_mon_context(&ctx, 267, 1, args);  /* Only 1 arg, need 2 */

    MonResult result = mon_dispatch(&ctx);

    if (result == MON_ERROR) {
        TEST_PASS("MON 413B returns error with insufficient arguments");
    } else {
        TEST_FAIL("MON 413B returns error with insufficient arguments", "should fail");
    }

    teardown();
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("=== MON Call Unit Tests ===\n");

    /* Run tests */
    test_mon_registry();
    test_mon_113B_clock();
    test_mon_113B_clock_missing_args();
    test_mon_312B_moinf();
    test_mon_62B_rmax_file_not_open();
    test_mon_12B_setcm();
    test_mon_12B_setcm_missing_args();
    test_mon_256B_deabf();
    test_mon_256B_deabf_with_existing_type();
    test_mon_2B_outbt_console();
    test_mon_2B_outbt_file_not_open();
    test_mon_2B_outbt_missing_args();
    test_mon_321B_ueadm_deprecated();
    test_mon_317B_uecom();
    test_mon_317B_uecom_missing_args();
    test_mon_317B_uecom_empty_command();
    test_mon_503B_dvinst_file_not_open();
    test_mon_503B_dvinst_missing_args();
    test_mon_503B_dvinst_max_bytes_exceeded();
    test_mon_503B_dvinst_zero_bytes();
    test_mon_503B_dvinst_queued_input();
    test_mon_1B_inbt_queued_input();
    test_mon_412B_fscnt_file_not_open();
    test_mon_412B_fscnt_missing_args();
    test_mon_413B_fscdnt_file_not_open();
    test_mon_413B_fscdnt_missing_args();

    /* Summary */
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
