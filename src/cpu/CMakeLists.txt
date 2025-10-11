cmake_minimum_required(VERSION 3.16)
add_library(nd500_cpu cpu.c cpu_instr.c)
target_include_directories(nd500_cpu PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_SOURCE_DIR}/src/machine)

