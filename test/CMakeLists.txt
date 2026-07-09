cmake_minimum_required(VERSION 3.16)

add_executable(disasm_tests
    disasm_tests.c
)
target_link_libraries(disasm_tests PRIVATE nd500_machine nd500_cpu nd500_ndlib)

add_executable(test_mmu_translation
    test_mmu_translation.c
)
target_link_libraries(test_mmu_translation PRIVATE nd500_machine nd500_cpu nd500_debugger nd500_ndlib)

add_executable(test_mmu_separate_id
    test_mmu_separate_id.c
)
target_link_libraries(test_mmu_separate_id PRIVATE nd500_machine nd500_cpu nd500_debugger nd500_ndlib)

add_executable(test_source_mapping
    test_source_mapping.c
)
target_link_libraries(test_source_mapping PRIVATE nd500_ndlib nd500_machine)

add_executable(test_lget_instruction
    test_lget_instruction.c
)
target_link_libraries(test_lget_instruction PRIVATE nd500_machine nd500_cpu nd500_debugger nd500_ndlib)

# Instruction validation tests (requires cJSON)
# Find cJSON independently to ensure it's available in this scope
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(CJSON_TEST QUIET libcjson)
endif()

if(CJSON_TEST_FOUND)
    add_executable(test_instruction_validation
        test_instruction_validation.c
    )
    target_link_libraries(test_instruction_validation PRIVATE
        nd500_machine
        nd500_cpu
        nd500_debugger
        nd500_ndlib
        ${CJSON_TEST_LIBRARIES}
    )
    target_include_directories(test_instruction_validation PRIVATE
        ${CJSON_TEST_INCLUDE_DIRS}
    )

    # Copy test data file to binary output directory (where test executable lives)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/nd500_tests.json
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nd500_tests.json
        COPYONLY
    )

    message(STATUS "Building test_instruction_validation with cJSON support")
else()
    message(STATUS "Skipping test_instruction_validation (cJSON not found)")
endif()

# Float arithmetic tests (helper functions only - NOT actual instruction tests)
# NOTE: Actual float instruction tests should be added to the C# test generator
# and included in nd500_tests.json, not as standalone C tests
add_executable(test_float_arithmetic
    test_float_arithmetic.c
)
target_link_libraries(test_float_arithmetic PRIVATE nd500_machine nd500_cpu nd500_debugger nd500_ndlib)
target_link_libraries(test_float_arithmetic PRIVATE m)  # Link math library

# OTE instruction tests
add_executable(test_ote_instructions
    test_ote_instructions.c
)
target_link_libraries(test_ote_instructions PRIVATE nd500_machine nd500_cpu nd500_debugger nd500_ndlib)

# Unsigned displacement tests
# Verifies LOCAL, RECORD, PREINDEXED displacements are treated as unsigned
# per ND-05.009.4 Section 8.4
add_executable(test_unsigned_displacement
    test_unsigned_displacement.c
)
target_link_libraries(test_unsigned_displacement PRIVATE nd500_machine nd500_cpu nd500_debugger nd500_ndlib)

# DOM integration test
# Loads a DOM file, runs execution for up to N steps or until MON 0B exit
add_executable(test_dom_integration
    test_dom_integration.c
)
target_link_libraries(test_dom_integration PRIVATE nd500_machine nd500_cpu nd500_ndlib nd500_debugger mon)

# BIT addressing tests
# Tests BI-prefixed instructions and bit addressing mode
# Verifies fix for bug where bit index was treated as byte offset
add_executable(test_bit_addressing
    test_bit_addressing.c
)
target_link_libraries(test_bit_addressing PRIVATE nd500_machine nd500_cpu nd500_debugger nd500_ndlib)

# MON call unit tests
# Tests SINTRAN III Monitor Call implementations
add_executable(test_mon_calls
    test_mon_calls.c
)
target_link_libraries(test_mon_calls PRIVATE nd500_machine nd500_cpu nd500_debugger nd500_ndlib mon)

# BMOVE overlap tests
# Tests BMOVE instruction with overlapping memory regions
# Verifies fix for bug where forward copy corrupted overlapping destination > source
add_executable(test_bmove_overlap
    test_bmove_overlap.c
)
target_link_libraries(test_bmove_overlap PRIVATE nd500_machine nd500_cpu nd500_debugger nd500_ndlib)

# ALT prefix tests
# Tests ALT prefix (0xC8) domain switching for data access
# Verifies CAD is used instead of CED when ALT prefix is present
add_executable(test_alt_prefix
    test_alt_prefix.c
)
target_link_libraries(test_alt_prefix PRIVATE nd500_machine nd500_cpu nd500_debugger nd500_ndlib)


# ---------------------------------------------------------------------------
# CTest registration
# enable_testing() is called in the top-level CMakeLists.txt.
# disasm_tests is not registered: it requires an object file argument.
# test_mmu_translation and test_lget_instruction always exit 0, so they are
# gated on their printed output instead of the exit code.

add_test(NAME mmu_translation COMMAND test_mmu_translation)
set_tests_properties(mmu_translation PROPERTIES FAIL_REGULAR_EXPRESSION "FAIL")

add_test(NAME mmu_separate_id COMMAND test_mmu_separate_id)
add_test(NAME source_mapping COMMAND test_source_mapping)

add_test(NAME lget_instruction COMMAND test_lget_instruction)
set_tests_properties(lget_instruction PROPERTIES FAIL_REGULAR_EXPRESSION "FAIL")

add_test(NAME float_arithmetic COMMAND test_float_arithmetic)
add_test(NAME ote_instructions COMMAND test_ote_instructions)
add_test(NAME unsigned_displacement COMMAND test_unsigned_displacement)
add_test(NAME bit_addressing COMMAND test_bit_addressing)
add_test(NAME mon_calls COMMAND test_mon_calls)
add_test(NAME bmove_overlap COMMAND test_bmove_overlap)
add_test(NAME alt_prefix COMMAND test_alt_prefix)

if(CJSON_TEST_FOUND)
    # Runner locates nd500_tests.json in its own executable directory
    add_test(NAME instruction_validation COMMAND test_instruction_validation --continue)
    set_tests_properties(instruction_validation PROPERTIES TIMEOUT 600)
endif()

# DOM integration smoke test: load and run a real 1990 SINTRAN-linked binary
# (the NC compiler domain). Skipped when the external file is not present so
# fresh checkouts and CI do not fail.
set(NC_COMPILER_DOM "/mnt/d/ND/500/FraTor/nc/nc-a06.dom"
    CACHE FILEPATH "Path to NC compiler DOM binary for the integration smoke test")
if(EXISTS ${NC_COMPILER_DOM})
    add_test(NAME dom_nc_compiler COMMAND test_dom_integration ${NC_COMPILER_DOM} 100000)
    set_tests_properties(dom_nc_compiler PROPERTIES TIMEOUT 120)
    message(STATUS "DOM smoke test enabled: ${NC_COMPILER_DOM}")
else()
    message(STATUS "DOM smoke test skipped: ${NC_COMPILER_DOM} not found")
endif()
