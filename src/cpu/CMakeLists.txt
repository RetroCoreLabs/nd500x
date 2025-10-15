cmake_minimum_required(VERSION 3.16)

# Collect all instruction implementation files from subdirectories
# Note: Instruction stubs are generated in binary dir by gen_instructions tool
file(GLOB_RECURSE INSTRUCTION_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/instructions/*/*.c")

# Add generated dispatch table to the CPU library
set(DISPATCH_TABLE_SOURCE "${CMAKE_BINARY_DIR}/nd500_instructions_gen.c")

add_library(nd500_cpu cpu.c cpu_instr.c ${INSTRUCTION_SOURCES} ${DISPATCH_TABLE_SOURCE})
target_include_directories(nd500_cpu PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_SOURCE_DIR}/src/machine)

