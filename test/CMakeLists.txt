cmake_minimum_required(VERSION 3.16)

add_executable(disasm_tests
    disasm_tests.c
)
target_link_libraries(disasm_tests PRIVATE nd500_machine nd500_cpu nd500_ndlib)

add_executable(test_mmu_translation
    test_mmu_translation.c
    ../src/cpu/nd500_mmu.c
)
target_include_directories(test_mmu_translation PRIVATE ../src/cpu)
# Note: This is a unit test that links only nd500_mmu.c directly
# Full integration tests will be in Phase 5 (Memory Bus Integration)

add_executable(test_mmu_separate_id
    test_mmu_separate_id.c
)
target_link_libraries(test_mmu_separate_id PRIVATE nd500_machine nd500_cpu nd500_debugger nd500_ndlib)

