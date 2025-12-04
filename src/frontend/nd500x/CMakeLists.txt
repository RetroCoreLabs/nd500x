cmake_minimum_required(VERSION 3.16)
add_executable(nd500x nd500x.c)
target_include_directories(nd500x PRIVATE ${CMAKE_SOURCE_DIR}/src/cpu ${CMAKE_SOURCE_DIR}/src/machine ${CMAKE_SOURCE_DIR}/src/debugger ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(nd500x PRIVATE nd500_cpu nd500_machine nd500_ndlib nd500_debugger)


