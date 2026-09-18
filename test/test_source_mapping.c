#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "../src/ndlib/ndlib.h"

/*
 * Test program for source-level debugging functionality
 * Tests map file parsing, source line lookups, and address lookups
 */

static int test_count = 0;
static int test_passed = 0;
static int test_failed = 0;

#define TEST_START(name) \
    do { \
        test_count++; \
        printf("[TEST %d] %s...\n", test_count, name); \
    } while(0)

#define TEST_PASS() \
    do { \
        test_passed++; \
        printf("  PASS\n\n"); \
    } while(0)

#define TEST_FAIL(msg) \
    do { \
        test_failed++; \
        printf("  FAIL: %s\n\n", msg); \
    } while(0)

#define ASSERT_EQ(expected, actual, msg) \
    do { \
        if ((expected) != (actual)) { \
            printf("  ASSERTION FAILED: %s\n", msg); \
            printf("    Expected: %d, Got: %d\n", (int)(expected), (int)(actual)); \
            TEST_FAIL(msg); \
            return 0; \
        } \
    } while(0)

#define ASSERT_STR_EQ(expected, actual, msg) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            printf("  ASSERTION FAILED: %s\n", msg); \
            printf("    Expected: \"%s\", Got: \"%s\"\n", (expected), (actual)); \
            TEST_FAIL(msg); \
            return 0; \
        } \
    } while(0)

#define ASSERT_NOT_NULL(ptr, msg) \
    do { \
        if ((ptr) == NULL) { \
            printf("  ASSERTION FAILED: %s\n", msg); \
            TEST_FAIL(msg); \
            return 0; \
        } \
    } while(0)

/* Create a temporary map file for testing */
static void create_test_map_file(const char* filename, const char* content) {
    FILE* f = fopen(filename, "w");
    assert(f != NULL);
    fprintf(f, "%s", content);
    fclose(f);
}

/* Test 1: Map file parsing with octal addresses */
static int test_map_parsing_octal(void) {
    TEST_START("Map file parsing (octal addresses)");

    // Create test map file with octal addresses
    const char* map_content =
        "program.s:1 -> 000000\n"
        "program.s:5 -> 000010\n"
        "program.s:10 -> 000020\n"
        "helper.s:1 -> 000100\n";

    create_test_map_file("/tmp/test_octal.map", map_content);

    // Clear any existing symbols
    ndlib_symbols_clear();

    // Load map file
    int result = ndlib_map_load("/tmp/test_octal.map");
    ASSERT_EQ(0, result, "Map file should load successfully");

    // Test address lookups
    int line = ndlib_symbols_line_for_addr(0x0000);
    ASSERT_EQ(1, line, "Address 0x0000 should map to line 1");

    line = ndlib_symbols_line_for_addr(0x0008);  // Octal 010 = Dec 8
    ASSERT_EQ(5, line, "Address 0x0008 should map to line 5");

    line = ndlib_symbols_line_for_addr(0x0010);  // Octal 020 = Dec 16
    ASSERT_EQ(10, line, "Address 0x0010 should map to line 10");

    line = ndlib_symbols_line_for_addr(0x0040);  // Octal 100 = Dec 64
    ASSERT_EQ(1, line, "Address 0x0040 should map to line 1 of helper.s");

    // Test file name lookups
    const char* file = ndlib_symbols_file_for_addr(0x0000);
    ASSERT_NOT_NULL(file, "File name should be found for address 0x0000");
    ASSERT_STR_EQ("program.s", file, "Address 0x0000 should map to program.s");

    file = ndlib_symbols_file_for_addr(0x0040);
    ASSERT_NOT_NULL(file, "File name should be found for address 0x0040");
    ASSERT_STR_EQ("helper.s", file, "Address 0x0040 should map to helper.s");

    // Test reverse lookup (file:line -> address)
    uint32_t addr = 0;
    result = ndlib_symbols_addr_for_line("program.s", 5, &addr);
    ASSERT_EQ(0, result, "Should find address for program.s:5");
    ASSERT_EQ(0x0008, addr, "program.s:5 should map to address 0x0008");

    result = ndlib_symbols_addr_for_line("helper.s", 1, &addr);
    ASSERT_EQ(0, result, "Should find address for helper.s:1");
    ASSERT_EQ(0x0040, addr, "helper.s:1 should map to address 0x0040");

    // Test non-existent address
    line = ndlib_symbols_line_for_addr(0x9999);
    ASSERT_EQ(-1, line, "Non-existent address should return -1");

    // Test non-existent file:line
    result = ndlib_symbols_addr_for_line("nonexistent.s", 1, &addr);
    ASSERT_EQ(-1, result, "Non-existent file should return -1");

    TEST_PASS();
    return 1;
}

/* Test 2: Map file parsing with hexadecimal addresses */
static int test_map_parsing_hex(void) {
    TEST_START("Map file parsing (hexadecimal addresses)");

    // Create test map file with hex addresses
    const char* map_content =
        "main.s:1 -> 0x08000000\n"
        "main.s:10 -> 0x08000010\n"
        "main.s:20 -> 0x08000020\n"
        "lib.s:5 -> 0x08001000\n";

    create_test_map_file("/tmp/test_hex.map", map_content);

    // Clear any existing symbols
    ndlib_symbols_clear();

    // Load map file
    int result = ndlib_map_load("/tmp/test_hex.map");
    ASSERT_EQ(0, result, "Map file should load successfully");

    // Test address lookups
    int line = ndlib_symbols_line_for_addr(0x08000000);
    ASSERT_EQ(1, line, "Address 0x08000000 should map to line 1");

    line = ndlib_symbols_line_for_addr(0x08000010);
    ASSERT_EQ(10, line, "Address 0x08000010 should map to line 10");

    line = ndlib_symbols_line_for_addr(0x08001000);
    ASSERT_EQ(5, line, "Address 0x08001000 should map to line 5");

    // Test file name lookups
    const char* file = ndlib_symbols_file_for_addr(0x08000000);
    ASSERT_NOT_NULL(file, "File name should be found");
    ASSERT_STR_EQ("main.s", file, "Address 0x08000000 should map to main.s");

    file = ndlib_symbols_file_for_addr(0x08001000);
    ASSERT_NOT_NULL(file, "File name should be found");
    ASSERT_STR_EQ("lib.s", file, "Address 0x08001000 should map to lib.s");

    // Test reverse lookup
    uint32_t addr = 0;
    result = ndlib_symbols_addr_for_line("main.s", 10, &addr);
    ASSERT_EQ(0, result, "Should find address for main.s:10");
    ASSERT_EQ(0x08000010, addr, "main.s:10 should map to address 0x08000010");

    TEST_PASS();
    return 1;
}

/* Test 3: Mixed octal and hex addresses */
static int test_map_parsing_mixed(void) {
    TEST_START("Map file parsing (mixed octal and hex)");

    // Create test map file with both octal and hex addresses
    const char* map_content =
        "boot.s:1 -> 000000\n"
        "kernel.s:1 -> 0x08000000\n"
        "boot.s:50 -> 000777\n"
        "kernel.s:100 -> 0x08000100\n";

    create_test_map_file("/tmp/test_mixed.map", map_content);

    // Clear any existing symbols
    ndlib_symbols_clear();

    // Load map file
    int result = ndlib_map_load("/tmp/test_mixed.map");
    ASSERT_EQ(0, result, "Map file should load successfully");

    // Test octal address
    int line = ndlib_symbols_line_for_addr(0x0000);
    ASSERT_EQ(1, line, "Octal address 000000 should work");

    line = ndlib_symbols_line_for_addr(0x01FF);  // Octal 777 = Dec 511
    ASSERT_EQ(50, line, "Octal address 000777 should work");

    // Test hex address
    line = ndlib_symbols_line_for_addr(0x08000000);
    ASSERT_EQ(1, line, "Hex address 0x08000000 should work");

    line = ndlib_symbols_line_for_addr(0x08000100);
    ASSERT_EQ(100, line, "Hex address 0x08000100 should work");

    TEST_PASS();
    return 1;
}

/* Test 4: Source file storage and retrieval */
static int test_source_storage(void) {
    TEST_START("Source file storage and retrieval");

    const char* source_content =
        "        .title  TEST PROGRAM\n"
        "        move    I1,0\n"
        "        add     I1,1\n"
        "        jmp     start\n"
        "end:    halt\n";

    const char* filename = "test.s";

    // Store source file
    int result = ndlib_source_store(filename, source_content);
    ASSERT_EQ(0, result, "Source file should be stored successfully");

    // Retrieve full content
    const char* content = ndlib_source_get_content(filename);
    ASSERT_NOT_NULL(content, "Source content should be retrievable");
    ASSERT_STR_EQ(source_content, content, "Retrieved content should match original");

    // Retrieve specific lines (1-indexed)
    const char* line1 = ndlib_source_get_line(filename, 1);
    ASSERT_NOT_NULL(line1, "Line 1 should be retrievable");
    ASSERT_STR_EQ("        .title  TEST PROGRAM", line1, "Line 1 content should match");

    const char* line2 = ndlib_source_get_line(filename, 2);
    ASSERT_NOT_NULL(line2, "Line 2 should be retrievable");
    ASSERT_STR_EQ("        move    I1,0", line2, "Line 2 content should match");

    const char* line5 = ndlib_source_get_line(filename, 5);
    ASSERT_NOT_NULL(line5, "Line 5 should be retrievable");
    ASSERT_STR_EQ("end:    halt", line5, "Line 5 content should match");

    // Test out-of-bounds line
    const char* line99 = ndlib_source_get_line(filename, 99);
    if (line99 != NULL) {
        TEST_FAIL("Out-of-bounds line should return NULL");
        return 0;
    }

    // Test non-existent file
    const char* no_content = ndlib_source_get_content("nonexistent.s");
    if (no_content != NULL) {
        TEST_FAIL("Non-existent file should return NULL");
        return 0;
    }

    TEST_PASS();
    return 1;
}

/* Test 5: Empty map file */
static int test_empty_map(void) {
    TEST_START("Empty map file handling");

    create_test_map_file("/tmp/test_empty.map", "");

    ndlib_symbols_clear();

    int result = ndlib_map_load("/tmp/test_empty.map");
    ASSERT_EQ(0, result, "Empty map file should load without error");

    // All lookups should return not found
    int line = ndlib_symbols_line_for_addr(0x0000);
    ASSERT_EQ(-1, line, "Empty map should return -1 for any address");

    const char* file = ndlib_symbols_file_for_addr(0x0000);
    if (file != NULL) {
        TEST_FAIL("Empty map should return NULL for file lookup");
        return 0;
    }

    TEST_PASS();
    return 1;
}

/* Test 6: Malformed map file entries */
static int test_malformed_map(void) {
    TEST_START("Malformed map file handling");

    // Map file with some valid and some invalid entries
    const char* map_content =
        "good.s:1 -> 0x1000\n"
        "invalid line without arrow\n"
        "missing:address -> \n"
        "good.s:10 -> 0x1010\n"
        "-> missing_filename\n";

    create_test_map_file("/tmp/test_malformed.map", map_content);

    ndlib_symbols_clear();

    // Should load successfully, skipping malformed lines
    int result = ndlib_map_load("/tmp/test_malformed.map");
    ASSERT_EQ(0, result, "Malformed map file should still load");

    // Valid entries should still work
    int line = ndlib_symbols_line_for_addr(0x1000);
    ASSERT_EQ(1, line, "Valid entry at 0x1000 should work");

    line = ndlib_symbols_line_for_addr(0x1010);
    ASSERT_EQ(10, line, "Valid entry at 0x1010 should work");

    TEST_PASS();
    return 1;
}

/* Main test runner */
int main(void) {
    printf("=======================================================\n");
    printf("ND-500 Source-Level Debugging Test Suite\n");
    printf("=======================================================\n\n");

    // Run all tests
    test_map_parsing_octal();
    test_map_parsing_hex();
    test_map_parsing_mixed();
    test_source_storage();
    test_empty_map();
    test_malformed_map();

    // Print summary
    printf("=======================================================\n");
    printf("Test Summary:\n");
    printf("  Total:  %d\n", test_count);
    printf("  Passed: %d\n", test_passed);
    printf("  Failed: %d\n", test_failed);
    printf("=======================================================\n");

    if (test_failed == 0) {
        printf("\nOK All tests PASSED!\n\n");
        return 0;
    } else {
        printf("\nSome tests FAILED!\n\n");
        return 1;
    }
}
