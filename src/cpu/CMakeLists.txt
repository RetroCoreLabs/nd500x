cmake_minimum_required(VERSION 3.16)

# Collect all instruction implementation files from src/cpu/instructions/
file(GLOB_RECURSE INSTRUCTION_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/instructions/*/*.c")

# Dispatch table and header are now in src/cpu/ (committed as source files)
set(DISPATCH_TABLE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/nd500_instructions.c")

# Create library with all sources
add_library(nd500_cpu cpu.c cpu_instr.c nd500_mmu.c ${INSTRUCTION_SOURCES} ${DISPATCH_TABLE_SOURCE})
target_include_directories(nd500_cpu PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_SOURCE_DIR}/src/machine)

