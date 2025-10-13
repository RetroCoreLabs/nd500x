cmake_minimum_required(VERSION 3.16)

add_executable(disasm_tests
    disasm_tests.c
)
target_link_libraries(disasm_tests PRIVATE nd500_machine nd500_cpu nd500_ndlib)

if (EXISTS "${CMAKE_SOURCE_DIR}/samples/add.o")
    add_test(NAME disasm_add_o COMMAND disasm_tests ${CMAKE_SOURCE_DIR}/samples/add.o)
else()
    message(STATUS "samples/add.o not found; skipping disasm_add_o test")
endif()


