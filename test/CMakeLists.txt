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

    # Copy test data file to build directory
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/nd500_tests.json
        ${CMAKE_CURRENT_BINARY_DIR}/nd500_tests.json
        COPYONLY
    )

    message(STATUS "Building test_instruction_validation with cJSON support")
else()
    message(STATUS "Skipping test_instruction_validation (cJSON not found)")
endif()

