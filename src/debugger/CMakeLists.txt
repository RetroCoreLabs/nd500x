cmake_minimum_required(VERSION 3.16)
add_library(nd500_debugger commands.c debugger.c dap_adapter.c)
target_include_directories(nd500_debugger PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${ND500X_ROOT}/src/machine ${ND500X_ROOT}/src/cpu ${ND500X_ROOT}/include)
target_link_libraries(nd500_debugger PUBLIC nd500_machine nd500_cpu nd500_ndlib mon)

# Link readline if available
if(READLINE_FOUND)
    target_link_libraries(nd500_debugger PUBLIC ${READLINE_LIBRARIES})
    target_include_directories(nd500_debugger PUBLIC ${READLINE_INCLUDE_DIRS})
endif()

# DAP support: enabled when the libdap object library target exists
# (added by the top-level CMakeLists when external/libdap is present and
# SKIP_LIBDAP is off). DAP_ENABLED matches the nd100x convention.
if(TARGET dap_objects)
    target_compile_definitions(nd500_debugger PUBLIC DAP_ENABLED=1)
    target_link_libraries(nd500_debugger PUBLIC dap_objects)
    target_include_directories(nd500_debugger PUBLIC ${ND500X_ROOT}/external/libdap/libdap/include)
endif()

