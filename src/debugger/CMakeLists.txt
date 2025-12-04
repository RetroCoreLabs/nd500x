cmake_minimum_required(VERSION 3.16)
add_library(nd500_debugger commands.c debugger.c dap_adapter.c)
target_include_directories(nd500_debugger PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_SOURCE_DIR}/src/machine ${CMAKE_SOURCE_DIR}/src/cpu ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(nd500_debugger PUBLIC nd500_machine nd500_cpu nd500_ndlib)

# Link readline if available
if(READLINE_FOUND)
    target_link_libraries(nd500_debugger PUBLIC ${READLINE_LIBRARIES})
    target_include_directories(nd500_debugger PUBLIC ${READLINE_INCLUDE_DIRS})
endif()

if(DEFINED HAVE_LIBDAP)
    target_compile_definitions(nd500_debugger PUBLIC WITH_DEBUGGER)
    if(TARGET dap_objects)
        target_link_libraries(nd500_debugger PUBLIC dap_objects)
        target_include_directories(nd500_debugger PUBLIC ${CMAKE_SOURCE_DIR}/external/libdap/libdap/include)
    endif()
endif()

