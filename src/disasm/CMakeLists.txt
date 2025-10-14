cmake_minimum_required(VERSION 3.16)
add_library(nd500_disasm nd500_disasm.c)
target_include_directories(nd500_disasm PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_SOURCE_DIR}/src/cpu ${CMAKE_SOURCE_DIR}/src/machine)
target_link_libraries(nd500_disasm PUBLIC nd500_cpu)

