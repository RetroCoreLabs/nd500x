cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Create a build directory and run cmake from there.")
endif()

project(nd500x VERSION 0.1.0 LANGUAGES C)

# With no CMAKE_BUILD_TYPE, CMake passes no -O flag at all and GCC/Clang default
# to -O0. Every file of the emulator was being built unoptimized, which for an
# instruction interpreter is the dominant cost - it also stops trivial helpers
# like io.c's in_range() from being inlined, so they show up as real calls on
# every memory access. RelWithDebInfo keeps -g, so the debugger and DAP work
# exactly as before. Pass -DCMAKE_BUILD_TYPE=Debug explicitly for an -O0 build.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
                 Debug Release RelWithDebInfo MinSizeRel)
    message(STATUS "No CMAKE_BUILD_TYPE given - defaulting to RelWithDebInfo (-O2 -g)")
endif()

option(BUILD_WASM "Build WebAssembly frontend with Emscripten" OFF)
option(DEBUGGER_ENABLED "Enable debugger support" ON)

# Platform detection
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(PLATFORM_LINUX TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(PLATFORM_WINDOWS TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set(PLATFORM_WASM TRUE)
    set(DEBUGGER_ENABLED OFF)
else()
    message(STATUS "Building on ${CMAKE_SYSTEM_NAME}")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# cJSON integration strategy
#
# cJSON is REQUIRED, not optional: src/machine/memory_map.c includes
# <cjson/cJSON.h> unconditionally and calls it in 37 places, and the DAP layer
# needs it too. The old code only looked via pkg-config and, when that failed,
# printed "Native build can proceed without JSON output" and carried on - which
# was untrue. On Windows, where pkg-config finds nothing, the build died with
# "cjson/cJSON.h: No such file or directory".
#
# Resolution order, cheapest first:
#   1. pkg-config libcjson   - Linux distro packages, MSYS2, Homebrew
#   2. plain find_path/find_library - installs that ship no .pc file
#   3. FetchContent from GitHub - last resort; needs network at configure time
#
# CI installs a package on every platform, so step 3 only ever fires for a
# developer building on a machine without cJSON.
if(PLATFORM_WASM OR BUILD_WASM)
    include(FetchContent)
    # SOURCE_SUBDIR points at nothing on purpose - see the long note in the
    # native branch below. Short version: cJSON's own CMakeLists asks for
    # cmake_minimum_required(VERSION 3.0), which CMake 4 refuses to honour, and
    # only cJSON.c is needed here anyway.
    FetchContent_Declare(cjson
        GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
        GIT_TAG        master
        SOURCE_SUBDIR  no-cmake-build-here)
    FetchContent_MakeAvailable(cjson)
    add_library(cjson_objects OBJECT ${cjson_SOURCE_DIR}/cJSON.c)
    target_include_directories(cjson_objects PUBLIC ${cjson_SOURCE_DIR})
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include/cjson)
    configure_file(${cjson_SOURCE_DIR}/cJSON.h ${CMAKE_BINARY_DIR}/include/cjson/cJSON.h COPYONLY)
    include_directories(${CMAKE_BINARY_DIR}/include)
else()
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(CJSON QUIET libcjson)
    endif()

    if(CJSON_FOUND)
        message(STATUS "cJSON: pkg-config (${CJSON_VERSION})")
        add_compile_definitions(HAVE_SYSTEM_CJSON)
        include_directories(${CJSON_INCLUDE_DIRS})
        link_directories(${CJSON_LIBRARY_DIRS})
    else()
        # No .pc file: look for the header and library directly. MSYS2 ships
        # both but its pkgconf is not always on PATH for a w64devkit build.
        find_path(ND_CJSON_INCLUDE_DIR cjson/cJSON.h)
        find_library(ND_CJSON_LIBRARY NAMES cjson libcjson)

        if(ND_CJSON_INCLUDE_DIR AND ND_CJSON_LIBRARY)
            message(STATUS "cJSON: found at ${ND_CJSON_LIBRARY}")
            add_compile_definitions(HAVE_SYSTEM_CJSON)
            include_directories(${ND_CJSON_INCLUDE_DIR})
            set(CJSON_LIBRARIES ${ND_CJSON_LIBRARY})
        else()
            message(STATUS "cJSON not installed - fetching a private copy from GitHub")
            include(FetchContent)
            # Pinned, not 'master': a release build should not change because
            # somebody pushed to cJSON this morning.
            #
            # SOURCE_SUBDIR names a directory that does not exist on purpose.
            # That makes FetchContent download the sources and stop, instead of
            # running cJSON's own CMakeLists - which opens with
            # cmake_minimum_required(VERSION 3.0) and is therefore REJECTED
            # outright by CMake 4 (measured: 4.2.3 on Windows fails to
            # configure, 3.22 on Linux still accepts it). Only cJSON.c is
            # wanted here, so its build system is dead weight either way.
            FetchContent_Declare(cjson
                GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
                GIT_TAG        v1.7.18
                SOURCE_SUBDIR  no-cmake-build-here)
            FetchContent_MakeAvailable(cjson)
            add_library(cjson_objects OBJECT ${cjson_SOURCE_DIR}/cJSON.c)
            target_include_directories(cjson_objects PUBLIC ${cjson_SOURCE_DIR})
            # The sources say #include <cjson/cJSON.h>, so give them a directory
            # with that exact shape rather than editing 40-odd include lines.
            file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include/cjson)
            configure_file(${cjson_SOURCE_DIR}/cJSON.h
                           ${CMAKE_BINARY_DIR}/include/cjson/cJSON.h COPYONLY)
            include_directories(${CMAKE_BINARY_DIR}/include)
        endif()
    endif()
endif()

# Readline integration for tab completion and command history
if(NOT PLATFORM_WASM)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(READLINE QUIET readline)
        if(READLINE_FOUND)
            add_compile_definitions(HAVE_READLINE)
            message(STATUS "Readline found - enabling tab completion and command history")
        else()
            message(STATUS "Readline not found - tab completion and history disabled")
        endif()
    else()
        message(STATUS "PkgConfig not found - readline detection disabled")
    endif()
endif()

# Optionally include externals if present (native only - DAP requires sockets)
option(SKIP_LIBDAP "Skip DAP library even if present" OFF)

# DAP is built on every platform. libdap used to be POSIX-only - straight BSD
# sockets, AF_UNIX, fcntl(O_NONBLOCK) and errno - and now goes through its own
# libdap/include/dap_net_compat.h shim, which maps all of that onto Winsock.
if(NOT PLATFORM_WASM AND NOT SKIP_LIBDAP AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/libdap/CMakeLists.txt")
    add_subdirectory(external/libdap)
    if(TARGET dap_objects AND TARGET cjson_objects)
        target_link_libraries(dap_objects PRIVATE cjson_objects)
    endif()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/libsymbols")
    add_definitions(-DHAVE_LIBSYMBOLS)
    add_subdirectory(external/libsymbols)
endif()

# MON call library (ndmonlib) - essential for SINTRAN support
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/ndmonlib/CMakeLists.txt")
    add_subdirectory(external/ndmonlib)
else()
    message(WARNING "ndmonlib submodule not found at external/ndmonlib")
    message(WARNING "Run: git submodule update --init external/ndmonlib")
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add subdirectories
add_subdirectory(src/ndlib)
add_subdirectory(src/disasm)
add_subdirectory(src/machine)
add_subdirectory(src/cpu)
# MON library now comes from external/ndmonlib (added above)
add_subdirectory(src/debugger)

if(BUILD_WASM)
    add_subdirectory(src/frontend/nd500wasm)
else()
    add_subdirectory(src/frontend/nd500x)
endif()

if(PLATFORM_WASM OR BUILD_WASM)
    if(TARGET nd500wasm)
        set_property(TARGET nd500wasm APPEND_STRING PROPERTY LINK_FLAGS " -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS','UTF8ToString','lengthBytesUTF8','HEAPU8'] -s ALLOW_MEMORY_GROWTH=1")
        set_property(TARGET nd500wasm APPEND_STRING PROPERTY LINK_FLAGS " -s EXPORTED_FUNCTIONS=['_malloc','_free','_nd500wasm_init','_nd500_dbg_mem_json','_nd500_dbg_disasm_json','_nd500_dbg_regs_json','_nd500_dbg_step_js','_nd500_dbg_run_js','_nd500_dbg_stop_js','_nd500_dbg_load_aout_js','_nd500_dbg_load_aout_path_js','_nd500_dbg_load_pseg_path_js','_nd500_dbg_load_dseg_path_js','_nd500_dbg_load_segments_path_js','_nd500_dbg_load_strerror_js','_nd500_dbg_get_memory_size_js','_nd500_dbg_reset_memory_js','_nd500_dbg_set_pc_js','_nd500_dbg_bp_add_js','_nd500_dbg_bp_del_js','_nd500_dbg_bp_enable_js','_nd500_dbg_bp_disable_js','_nd500_dbg_bp_list_json','_nd500_dbg_status_json','_nd500_dbg_traps_json','_nd500_dbg_clear_traps_js','_nd500_dbg_set_reg_js','_nd500_dbg_instr_count_js','_nd500_dbg_mnemonic_js','_nd500_dbg_build_info_js','_nd500_dbg_clear_symbols_js','_nd500_dbg_symbols_json','_nd500_cmd_exec_js','_nd500_cmd_list_js','_nd500_cmd_subcommands_js','_nd500_dbg_memory_map_json_js','_nd500_dbg_memory_map_for_domain_json_js','_nd500_dbg_mmu_is_enabled_js']")
        # Enable modularized factory for Node/web and ensure it works under Node tests
        set_property(TARGET nd500wasm APPEND_STRING PROPERTY LINK_FLAGS " -s MODULARIZE=1 -s EXPORT_NAME='ND500Wasm' -s ENVIRONMENT='web,node'")
        
        # Configure output to bin/ directory with .js extension
        set_target_properties(nd500wasm PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
            OUTPUT_NAME "nd500wasm")
    endif()
endif()


enable_testing()
add_subdirectory(test)
