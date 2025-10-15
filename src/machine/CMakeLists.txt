cmake_minimum_required(VERSION 3.16)
add_library(nd500_machine machine.c io.c debug_api.c machine_loader.c breakpoints.c memory_map.c)
target_include_directories(nd500_machine PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_SOURCE_DIR}/src/cpu)
target_link_libraries(nd500_machine PUBLIC nd500_ndlib nd500_cpu nd500_disasm)

