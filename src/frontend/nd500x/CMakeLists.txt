cmake_minimum_required(VERSION 3.16)
add_executable(nd500x nd500x.c nd500x_shell.c nd500x_telnet.c
               nd500x_ndix.c ndix_ffs.c ndix_menu.c telnetserver.c net_compat.c
               uplink_tcp.c uplink_tap.c)
target_include_directories(nd500x PRIVATE ${ND500X_ROOT}/src/cpu ${ND500X_ROOT}/src/machine ${ND500X_ROOT}/src/debugger ${ND500X_ROOT}/include)
target_link_libraries(nd500x PRIVATE nd500_cpu nd500_machine nd500_ndlib nd500_debugger mon)

# Threads. On Linux this is -lpthread; on macOS the pthread symbols live in
# libSystem and there is no -lpthread to link; on Windows w64devkit/MSYS2 supply
# winpthreads. Threads::Threads is the one spelling that is right everywhere,
# instead of hard-coding "pthread" and failing on the other two.
find_package(Threads REQUIRED)
target_link_libraries(nd500x PRIVATE Threads::Threads)

# Windows-only link libraries:
#   shell32 - SHGetFolderPathA, used by expand_tilde() in nd500x.c to resolve
#             the Documents folder (which is where '~' points on Windows).
#   ws2_32  - Winsock, for net_compat.c / telnetserver.c / nd500x_telnet.c.
#             POSIX has sockets in libc, so nothing is linked there.
if(PLATFORM_WINDOWS)
    target_link_libraries(nd500x PRIVATE shell32 ws2_32)
endif()


