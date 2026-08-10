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

# Tape record layer. Links only nd500_cpu: nd500_tape.c is deliberately free of
# machine/CPU dependencies so the format logic can be tested on its own.
add_executable(test_tape
    test_tape.c
)
target_link_libraries(test_tape PRIVATE nd500_cpu)

# XMSG ring discipline. Links only nd500_cpu because nd500_xring.c is free of
# CPU and machine dependencies on purpose - the rules are checkable against a
# plain byte array, and every way of getting them wrong is invisible inside a
# booting guest.
add_executable(test_xring
    test_xring.c
)
target_link_libraries(test_xring PRIVATE nd500_cpu)

# The XMSG SERVER - what gets answered, and how. Split from the ring tests
# because nd500_xmsg_service_mem() takes the same bare byte accessor and has no
# CPU in it either: the hang it guards against (a response whose func does not
# match its command) is completely silent inside a running guest.
add_executable(test_xmsg
    test_xmsg.c
)
target_link_libraries(test_xmsg PRIVATE nd500_cpu)

# The RETH wire format nd500x speaks to a RetroCore ethernet relay. Every
# constant in it is a claim about somebody else's code, and a wrong one gives a
# link that connects, says hello and then silently carries nothing.
add_executable(test_ethhub
    test_ethhub.c
)
target_link_libraries(test_ethhub PRIVATE nd500_cpu)

add_executable(test_riom
    test_riom.c
)
target_link_libraries(test_riom PRIVATE nd500_machine nd500_cpu nd500_debugger nd500_ndlib)

add_executable(test_lget_instruction
    test_lget_instruction.c
)
target_link_libraries(test_lget_instruction PRIVATE nd500_machine nd500_cpu nd500_debugger nd500_ndlib)

# DAP adapter unit tests (only when libdap is part of the build)
if(TARGET dap_objects)
    add_executable(test_dap_adapter
        test_dap_adapter.c
    )
    target_link_libraries(test_dap_adapter PRIVATE
        nd500_debugger nd500_machine nd500_cpu nd500_ndlib mon)
    target_include_directories(test_dap_adapter PRIVATE
        ${ND500X_ROOT}/external/libdap/libdap/include)
endif()

# Instruction validation tests (requires cJSON)
# Find cJSON independently to ensure it's available in this scope
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(CJSON_TEST QUIET libcjson)
endif()

if(CJSON_TEST_FOUND)
    add_executable(test_conformance
        test_conformance.c
    )
    target_link_libraries(test_conformance PRIVATE
        nd500_machine
        nd500_cpu
        nd500_debugger
        nd500_ndlib
        ${CJSON_TEST_LIBRARIES}
    )
    target_include_directories(test_conformance PRIVATE
        ${CJSON_TEST_INCLUDE_DIRS}
    )

    # Copy test data file to binary output directory (where test executable lives)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/nd500-conformance.json
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nd500-conformance.json
        COPYONLY
    )

    message(STATUS "Building test_conformance with cJSON support")
else()
    message(STATUS "Skipping test_conformance (cJSON not found)")
endif()

# Float arithmetic tests (helper functions only - NOT actual instruction tests)
# NOTE: Actual float instruction tests should be added to the C# test generator
# and included in nd500-conformance.json, not as standalone C tests
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

# STO (Stack Overflow) status-bit tests for heap/stack instructions (GETB et al.)
add_executable(test_stack_overflow
    test_stack_overflow.c
)
target_link_libraries(test_stack_overflow PRIVATE nd500_machine nd500_cpu nd500_debugger nd500_ndlib)

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

# Physical allocator + concurrent-domain residency test
# Checks page ownership/arena scoping, and that loading a second DOM leaves the
# first one's resident image intact (what makes nested DOM runs possible).
add_executable(test_phys_alloc
    test_phys_alloc.c
)
target_link_libraries(test_phys_alloc PRIVATE nd500_machine nd500_cpu nd500_ndlib nd500_debugger mon)

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


# Page-straddling memory access tests
# A 16/32-bit access crossing a 2KB (NBPG) page boundary must be split and each
# page translated on its own. Before the fix the tail bytes were written at
# paddr+1..N-1, i.e. into the frame that physically FOLLOWS the first one.
add_executable(test_page_straddle
    test_page_straddle.c
)
target_link_libraries(test_page_straddle PRIVATE nd500_machine nd500_cpu nd500_debugger nd500_ndlib)

# Double register (Dn = An:En) tests
# An is the HIGH half and En the low half; they used to be packed the other way
# round, which made D NEG flip a mantissa bit instead of the sign bit.
add_executable(test_double_register
    test_double_register.c
)
target_link_libraries(test_double_register PRIVATE nd500_machine nd500_cpu nd500_debugger nd500_ndlib)
target_link_libraries(test_double_register PRIVATE m)  # Link math library

# ENTS pending-CALL interlock must survive a trap dispatch + handler + RETT.
# Guards the fix where the CALL/ENT* sequence state is saved into the trap
# context on dispatch and restored by RETT (was a naked field the kernel
# handler's own CALL/ENT* pairs trampled -> false ISE on the resumed ENTS).
add_executable(test_ents_pending_call_trap
    test_ents_pending_call_trap.c
)
target_link_libraries(test_ents_pending_call_trap PRIVATE nd500_machine nd500_cpu nd500_debugger nd500_ndlib)

# PGU / WIP page tables. Both used to read back a hardwired zero, which is the
# aggressive answer for NDIX's pageout clock hand (sys/vm_page.c:547) and for
# dirty() (machine/pte.h:93), not the safe one.
add_executable(test_page_bits
    test_page_bits.c
)
target_link_libraries(test_page_bits PRIVATE nd500_machine nd500_cpu nd500_debugger nd500_ndlib)

# RPGU/RWIP operand width. The page-number operand must be read at full width;
# a byte read yields 0 for every page below 0x1000000 on a big-endian guest.
add_executable(test_rphs_wphs
    test_rphs_wphs.c
)
target_link_libraries(test_rphs_wphs PRIVATE nd500_machine nd500_cpu nd500_debugger nd500_ndlib)

add_executable(test_rpgu_operand
    test_rpgu_operand.c
)
target_link_libraries(test_rpgu_operand PRIVATE nd500_machine nd500_cpu nd500_debugger nd500_ndlib)

# Instruction abort guard. An operand access that faulted must leave the
# instruction committing nothing - no result, no flags, no second trap.
add_executable(test_abort_guard
    test_abort_guard.c
)
target_link_libraries(test_abort_guard PRIVATE nd500_machine nd500_cpu nd500_debugger nd500_ndlib)

# SOLO/TUTTI and the two process-switch traps they make possible: DT (timeout)
# and DE (non-ignorable trap taken inside the region).
add_executable(test_solo_traps
    test_solo_traps.c
)
target_link_libraries(test_solo_traps PRIVATE nd500_machine nd500_cpu nd500_debugger nd500_ndlib)

# Which trap does each instruction actually raise? The 19 wrong trap numbers
# fixed in ee3ec62 and 694bac2 all survived because nothing asserted the exact
# bit - conformance checks results only.
add_executable(test_trap_conformance
    test_trap_conformance.c
)
target_link_libraries(test_trap_conformance PRIVATE nd500_machine nd500_cpu nd500_debugger nd500_ndlib)


# Interactive-terminal smoke test: runs the real nd500x --debug binary on a
# pty (readline and termios behave differently on a pipe, so only a pty can
# catch "exits immediately on a real terminal" / "leaves the tty broken").
# The harness itself links nothing from the emulator - it forks and execs
# the nd500x binary, whose path ctest passes as the argument.
add_executable(test_pty_smoke
    test_pty_smoke.c
)

# ---------------------------------------------------------------------------
# diag_* investigation harnesses
#
# These are one-off harnesses written during an investigation, not asserting
# tests - they print what the machine did and a human reads it. They are NOT
# registered with CTest and are NOT part of the default build.
#
# They ARE build targets, which they were not before. Every one used to be
# hand-linked from the shell, `make` never rebuilt any of them, and a harness
# left over from an earlier session produced at least four investigations that
# chased a defect the current code did not have (the zero-target DOM and the
# 2026-07-26 "general regression" both traced back to a binary older than the
# source it was supposedly testing). Building them from the same tree as
# everything else removes that failure by construction.
#
# EXCLUDE_FROM_ALL keeps `make` untouched: build them with `make diag`, or one
# at a time with `make diag_linkdrive`. GLOB is deliberate here - the whole
# point is that adding test/diag_foo.c must not require remembering to edit
# this file. CONFIGURE_DEPENDS re-globs when the set of files changes.
#
# Every harness gets the same link line rather than a per-file one. All 57
# include machine_protos.h and cpu_protos.h, 56 call into mon, and 8 use the
# disassembler; the union costs nothing at link time and means a new harness
# builds without touching this block.
#
# nd500_debugger appears twice on purpose. nd500_ndlib and nd500_debugger call
# into each other - ndlib_dom_loader.c:717 calls
# nd500_debugger_register_domain(), defined in debugger/commands.c - and that
# dependency is not declared between the two targets, so CMake cannot know to
# repeat them. With a single mention the linker has already passed the
# debugger archive by the time ndlib pulls in the DOM loader, and all 16
# DOM-loading harnesses fail with one undefined reference. The existing tests
# do not hit this only because they do not pull in that object file.
file(GLOB ND500_DIAG_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/diag_*.c)

add_custom_target(diag COMMENT "Building the diag_* investigation harnesses")

foreach(_diag_src IN LISTS ND500_DIAG_SOURCES)
    get_filename_component(_diag_name ${_diag_src} NAME_WE)
    add_executable(${_diag_name} EXCLUDE_FROM_ALL ${_diag_src})
    target_link_libraries(${_diag_name} PRIVATE
        nd500_machine nd500_cpu nd500_debugger nd500_disasm nd500_ndlib mon
        nd500_debugger)
    add_dependencies(diag ${_diag_name})
endforeach()

list(LENGTH ND500_DIAG_SOURCES _diag_count)
message(STATUS "diag harnesses: ${_diag_count} targets (build with 'make diag')")


# ---------------------------------------------------------------------------
# CTest registration
# enable_testing() is called in the top-level CMakeLists.txt.
# disasm_tests is not registered: it requires an object file argument.
# test_mmu_translation and test_lget_instruction always exit 0, so they are
# gated on their printed output instead of the exit code.


# ---------------------------------------------------------------------------
# The settings catalog. Two tests, because there are two ways it can rot:
#   settings_catalog   - the documented defaults and, above all, the DIRECTION
#                        of the four inverted flags (ND500X_NOTLB and friends
#                        name what they switch OFF).
#   settings_no_getenv - a raw getenv creeping back into the libraries, which
#                        is how knobs used to escape the catalog entirely.
# ---------------------------------------------------------------------------
add_executable(test_settings_catalog test_settings_catalog.c)
target_link_libraries(test_settings_catalog PRIVATE nd500_cpu)

add_test(NAME xring COMMAND test_xring)
add_test(NAME xmsg COMMAND test_xmsg)
add_test(NAME ethhub COMMAND test_ethhub)
add_test(NAME mmu_translation COMMAND test_mmu_translation)
set_tests_properties(mmu_translation PROPERTIES FAIL_REGULAR_EXPRESSION "FAIL")

add_test(NAME mmu_separate_id COMMAND test_mmu_separate_id)
add_test(NAME source_mapping COMMAND test_source_mapping)

add_test(NAME lget_instruction COMMAND test_lget_instruction)
set_tests_properties(lget_instruction PROPERTIES FAIL_REGULAR_EXPRESSION "FAIL")

add_test(NAME float_arithmetic COMMAND test_float_arithmetic)
add_test(NAME ote_instructions COMMAND test_ote_instructions)
add_test(NAME stack_overflow COMMAND test_stack_overflow)
add_test(NAME unsigned_displacement COMMAND test_unsigned_displacement)
add_test(NAME bit_addressing COMMAND test_bit_addressing)
add_test(NAME tape COMMAND test_tape)
add_test(NAME riom COMMAND test_riom)
add_test(NAME mon_calls COMMAND test_mon_calls)
add_test(NAME bmove_overlap COMMAND test_bmove_overlap)
add_test(NAME alt_prefix COMMAND test_alt_prefix)
add_test(NAME page_straddle COMMAND test_page_straddle)
add_test(NAME double_register COMMAND test_double_register)
add_test(NAME ents_pending_call_trap COMMAND test_ents_pending_call_trap)
add_test(NAME page_bits COMMAND test_page_bits)
add_test(NAME rpgu_operand COMMAND test_rpgu_operand)
add_test(NAME rphs_wphs COMMAND test_rphs_wphs)
add_test(NAME abort_guard COMMAND test_abort_guard)
add_test(NAME solo_traps COMMAND test_solo_traps)
add_test(NAME trap_conformance COMMAND test_trap_conformance)

if(TARGET dap_objects)
    add_test(NAME dap_adapter COMMAND test_dap_adapter)
endif()

add_test(NAME pty_smoke COMMAND test_pty_smoke $<TARGET_FILE:nd500x>)
set_tests_properties(pty_smoke PROPERTIES TIMEOUT 60)

if(CJSON_TEST_FOUND)
    # Runner locates nd500-conformance.json in its own executable directory
    add_test(NAME conformance COMMAND test_conformance --continue)
    set_tests_properties(conformance PROPERTIES TIMEOUT 600)
endif()

# DOM integration smoke test: load and run a real 1990 SINTRAN-linked binary
# (the NC compiler domain). Skipped when the external file is not present so
# fresh checkouts and CI do not fail.
# The data tree is large and lives outside this repo, so it is located through
# ND500_TESTDATA (the same variable the diag_* harnesses use via testdata.h)
# rather than a hardcoded drive letter that only resolves on one machine.
# Unset simply leaves the path non-existent, and the EXISTS check below skips
# the test exactly as it does for a missing file.
if(DEFINED ENV{ND500_TESTDATA})
    set(_nd500_testdata "$ENV{ND500_TESTDATA}")
else()
    set(_nd500_testdata "ND500_TESTDATA-NOTSET")
endif()
set(NC_COMPILER_DOM "${_nd500_testdata}/FraTor/nc/nc-a06.dom"
    CACHE FILEPATH "Path to NC compiler DOM binary for the integration smoke test")
if(EXISTS ${NC_COMPILER_DOM})
    add_test(NAME dom_nc_compiler COMMAND test_dom_integration ${NC_COMPILER_DOM} 100000)
    set_tests_properties(dom_nc_compiler PROPERTIES TIMEOUT 120)
    message(STATUS "DOM smoke test enabled: ${NC_COMPILER_DOM}")
else()
    message(STATUS "DOM smoke test skipped: ${NC_COMPILER_DOM} not found")
endif()

# NC end-to-end compile tests: run the real NC compiler under emulation and
# assert it reaches code generation (a PROGRESS gate on instruction count).
#
# History: these once byte-compared the output against emulator-produced
# baselines, but that was a false positive - NC was only PREPROCESSING (the
# "no rewrite" bug, root-caused to the GETB heap allocator) and the "object"
# files were just macro-expanded source text. With that fixed NC genuinely
# compiles, runs well past the old ~1.11M-instruction preprocessing ceiling,
# and currently stops on a separate downstream bug (a corrupted pointer near
# instruction 1.5M). Until that is fixed and a real NRF baseline exists, we
# assert PROGRESS (>= 1,200,000 instructions = got past "no rewrite") rather
# than final output. Cross-emulator agreement with the C# RetroCore emulator
# remains the eventual ground truth.
#
# The sandbox is reset by a CTest fixture before each ctest invocation, and
# both compile tests share it, so they are serialized via RESOURCE_LOCK.
if(EXISTS ${NC_COMPILER_DOM})
    set(NC_SANDBOX ${CMAKE_BINARY_DIR}/nc_sandbox)

    add_test(NAME nc_sandbox_setup
        COMMAND ${CMAKE_COMMAND}
            -DSANDBOX=${NC_SANDBOX}
            -DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/nc_fixtures
            -P ${CMAKE_CURRENT_SOURCE_DIR}/nc_sandbox_setup.cmake)
    set_tests_properties(nc_sandbox_setup PROPERTIES
        FIXTURES_SETUP nc_sandbox)

    add_test(NAME dom_nc_compile_a
        COMMAND test_dom_integration ${NC_COMPILER_DOM} 5000000
            --input "COMPILE A,A,A\\r"
            --min-instructions 1200000
        WORKING_DIRECTORY ${NC_SANDBOX})
    add_test(NAME dom_nc_compile_b
        COMMAND test_dom_integration ${NC_COMPILER_DOM} 5000000
            --input "COMPILE B,B,B\\r"
            --min-instructions 1200000
        WORKING_DIRECTORY ${NC_SANDBOX})
    set_tests_properties(dom_nc_compile_a dom_nc_compile_b PROPERTIES
        FIXTURES_REQUIRED nc_sandbox
        RESOURCE_LOCK nc_sandbox
        TIMEOUT 300)
    message(STATUS "NC compile tests enabled (sandbox: ${NC_SANDBOX})")
endif()

add_test(NAME settings_catalog COMMAND test_settings_catalog)

add_test(NAME settings_no_getenv
         COMMAND ${CMAKE_COMMAND} -DSRC_DIR=${ND500X_ROOT}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/no_getenv_gate.cmake)
