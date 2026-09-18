cmake_minimum_required(VERSION 3.16)
add_library(nd500_disasm nd500_disasm.c)
target_link_libraries(nd500_disasm PRIVATE nd500x_warnings)   # house C standard, Tier A
target_include_directories(nd500_disasm PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${ND500X_ROOT}/src/cpu ${ND500X_ROOT}/src/machine)
target_link_libraries(nd500_disasm PUBLIC nd500_cpu)

