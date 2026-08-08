cmake_minimum_required(VERSION 3.16)

# Collect all instruction implementation files from src/cpu/instructions/
file(GLOB_RECURSE INSTRUCTION_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/instructions/*/*.c")

# Dispatch table and header are now in src/cpu/ (committed as source files)
set(DISPATCH_TABLE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/nd500_instructions.c")

# Create library with all sources
# SINTRAN MON-call emulation is optional (ND500X_WITH_NDMON, root CMakeLists).
# Exactly one implementation of the seam in nd500_mon_sintran.h is compiled:
#   ON  -> nd500_mon_sintran.c       real dispatch, needs the ndmonlib submodule
#   OFF -> nd500_mon_sintran_stub.c  no dependency at all
# MON 600 (the NDIX front-end call) is served by nd500_fecall.c either way, so
# the OFF build still boots NDIX - it just cannot run SINTRAN DOM programs.
if(ND500X_WITH_NDMON)
    set(MON_SEAM_SOURCE nd500_mon_sintran.c)
else()
    set(MON_SEAM_SOURCE nd500_mon_sintran_stub.c)
endif()

add_library(nd500_cpu cpu.c cpu_instr.c nd500_mmu.c nd500_tlb.c nd500_tape.c nd500_domain.c nd500_indirect.c nd500_fecall.c nd500_host_posix.c nd500_settings.c nd500_segment_alloc.c nd500_phys_alloc.c nd500_page_bits.c instruction_helpers.c bcd_helpers.c ${MON_SEAM_SOURCE} ${INSTRUCTION_SOURCES} ${DISPATCH_TABLE_SOURCE})
target_include_directories(nd500_cpu PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_SOURCE_DIR}/src/machine)

# Link math library for BCD operations (pow function)
target_link_libraries(nd500_cpu PUBLIC m)

# nd500_cpu <-> nd500_machine is a GENUINE CYCLE and it is now declared.
#
# The machine layer owns the CPU (nd500_machine links nd500_cpu), and the CPU
# calls straight back into machine/debug_api.c - nd500_dbg_get_trace_mode()
# from Callg.c and Entm.c, nd500_dbg_get_mmu_log_level() from nd500_mmu.c,
# nd500_dbg_flush_console_output() from the MON path, and more.
#
# That half of the cycle was never declared, so the link only worked by luck:
# it depended on whether a given executable happened to pull the debug_api
# member out of libnd500_machine.a BEFORE the linker reached libnd500_cpu.a.
# Adding one new call from machine/io.c into this library (nd500_mon_files_init,
# part of the MON seam) was enough to change which members got pulled and turn
# test_source_mapping into an undefined-reference failure.
#
# Declaring the cycle is the supported fix: CMake repeats both archives on the
# link line for a strongly-connected component of STATIC libraries.
target_link_libraries(nd500_cpu PUBLIC nd500_machine)

# ndmonlib is PRIVATE on purpose: only nd500_mon_sintran.c includes <ndmon/*.h>
# now, so consumers of nd500_cpu no longer inherit the dependency. That is what
# lets another host link this library without the (private) submodule.
if(ND500X_WITH_NDMON)
    target_include_directories(nd500_cpu PRIVATE ${CMAKE_SOURCE_DIR}/external/ndmonlib/include)
    target_link_libraries(nd500_cpu PRIVATE mon)
endif()

