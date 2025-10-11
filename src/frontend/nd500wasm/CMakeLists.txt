cmake_minimum_required(VERSION 3.16)
add_executable(nd500wasm main.c)
target_include_directories(nd500wasm PRIVATE ${CMAKE_SOURCE_DIR}/src/cpu ${CMAKE_SOURCE_DIR}/src/machine)
target_link_libraries(nd500wasm PRIVATE nd500_machine nd500_cpu nd500_ndlib)

