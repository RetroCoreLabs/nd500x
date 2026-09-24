cmake_minimum_required(VERSION 3.16)
add_library(nd500_machine machine.c io.c debug_api.c machine_loader.c breakpoints.c
                          memory_map.c nd500_ndix_boot.c)
target_link_libraries(nd500_machine PRIVATE nd500x_warnings)   # house C standard, Tier A
target_include_directories(nd500_machine PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${ND500X_ROOT}/src/cpu)
target_link_libraries(nd500_machine PUBLIC nd500_ndlib nd500_cpu nd500_disasm)

# io.c reaches the guest-visible bytes through ndbus_pool_r8 / _w8 - the relaxed
# atomics that make a shared pool defined C rather than a data race. ONE
# definition of that rule, in ndbus_pool.h, rather than a second copy here.
# The dependency runs this way on purpose: machine may use ndbus, ndbus may
# never use machine (see tools/check_ndbus_isolation.sh).
target_link_libraries(nd500_machine PUBLIC nd500_ndbus)

# memory_map.c calls cJSON directly. Until now those symbols only resolved by
# accident, because libdap happened to drag libcjson into the final link - so
# building with SKIP_LIBDAP=ON, or on any platform where DAP is off, would fail
# at link time. Say the dependency out loud instead.
# cjson_objects exists when cJSON was fetched rather than installed; otherwise
# CJSON_LIBRARIES holds whatever pkg-config or find_library turned up.
if(TARGET cjson_objects)
    target_link_libraries(nd500_machine PUBLIC cjson_objects)
elseif(CJSON_LIBRARIES)
    target_link_libraries(nd500_machine PUBLIC ${CJSON_LIBRARIES})
endif()

