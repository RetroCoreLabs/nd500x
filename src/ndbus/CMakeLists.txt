cmake_minimum_required(VERSION 3.16)

# The MFbus: the MPM-5 shared memory pool and the octobus.
#
# This library links NOTHING from the emulator - not nd500_cpu, not nd500_machine,
# not nd500_ndlib. It reaches both machines only through the two vtables in
# ndbus_types.h, which is what lets test layers 1, 3 and 4 run with two mock CPUs
# and no emulator present. Do not add an emulator target to this file; the
# isolation check below exists to catch it if someone does.
add_library(nd500_ndbus
    ndbus_pool.c
    ndbus_lock.c
    ndbus_doorbell.c
    ndbus_octobus.c
    ndbus_accp.c
    ndbus_multibyte.c
    ndbus_nd5000.c
    ndbus_window.c
    ndbus_runner.c
    ndbus_mailbox.c
    ndbus_context.c
)

target_link_libraries(nd500_ndbus PRIVATE nd500x_warnings)   # house C standard, Tier A
target_include_directories(nd500_ndbus PUBLIC ${CMAKE_CURRENT_LIST_DIR})

# The lock-cycle mutex is a real pthread mutex natively and compiles to nothing
# under Emscripten (one WebAssembly memory, no threads).
if(NOT PLATFORM_WASM)
    find_package(Threads REQUIRED)
    target_link_libraries(nd500_ndbus PUBLIC Threads::Threads)
endif()

# Enforce the isolation rule as part of the build, not as a code-review habit.
# POSIX shell, so it is skipped on a Windows build where no sh is present.
if(NOT CMAKE_HOST_WIN32)
    add_custom_command(TARGET nd500_ndbus POST_BUILD
        COMMAND ${ND500X_ROOT}/tools/check_ndbus_isolation.sh
        COMMENT "Checking that src/ndbus/ includes no emulator type"
        VERBATIM)
endif()
