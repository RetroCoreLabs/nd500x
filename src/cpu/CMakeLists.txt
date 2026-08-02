cmake_minimum_required(VERSION 3.16)

# Collect all instruction implementation files from src/cpu/instructions/
file(GLOB_RECURSE INSTRUCTION_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/instructions/*/*.c")

# Dispatch table and header are now in src/cpu/ (committed as source files)
set(DISPATCH_TABLE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/nd500_instructions.c")

# Create library with all sources
add_library(nd500_cpu cpu.c cpu_instr.c nd500_mmu.c nd500_tlb.c nd500_domain.c nd500_indirect.c nd500_fecall.c nd500_segment_alloc.c nd500_phys_alloc.c nd500_page_bits.c instruction_helpers.c bcd_helpers.c ${INSTRUCTION_SOURCES} ${DISPATCH_TABLE_SOURCE})
target_include_directories(nd500_cpu PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_SOURCE_DIR}/src/machine ${CMAKE_SOURCE_DIR}/external/ndmonlib/include)

# Link math library for BCD operations (pow function)
# Link libmon for MON call emulation (SINTRAN segment 31)
target_link_libraries(nd500_cpu PUBLIC m mon)

