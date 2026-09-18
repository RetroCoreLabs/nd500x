cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Create a build directory and run cmake from there.")
endif()

project(nd500x VERSION 0.1.0 LANGUAGES C)

# WHERE THIS PROJECT LIVES - use this, never CMAKE_SOURCE_DIR.
#
# nd500x is built two ways: on its own, and add_subdirectory()'d into another
# project (nd100x's WASM build links nd500_cpu + nd500_machine - see the
# ND500X_WITH_NDMON note below). CMAKE_SOURCE_DIR means the TOP-LEVEL source
# directory, so in the nested case it is nd100x's root and every
# ${CMAKE_SOURCE_DIR}/src/... path silently addresses the wrong tree.
# CMAKE_CURRENT_LIST_DIR of THIS file is nd500x either way.
set(ND500X_ROOT ${CMAKE_CURRENT_LIST_DIR})

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
set(CMAKE_C_EXTENSIONS ON)          # gnu11: never rely on the compiler default (gcc 14+ is gnu23)

# Tier A warnings from the house C standard. An INTERFACE target that each
# nd500x target links PRIVATE, so the flags reach only nd500x's own sources:
# never external/, and never a project that add_subdirectory()s nd500x.
# Every flag is probed first because this file is built by gcc (Linux and
# MinGW), clang (macOS) and emcc (WASM), and they do not all know the same
# -W options. The name carries the nd500x prefix on purpose: nd100x plans a
# warnings target of its own, and two add_subdirectory()'d trees creating
# the same name is a hard CMake error (the cjson_objects guard below exists
# for the same reason).
include(CheckCCompilerFlag)
option(ND500X_WERROR "Treat every warning as an error - for CI, not local builds" OFF)
if(NOT TARGET nd500x_warnings)
    add_library(nd500x_warnings INTERFACE)
    set(ND500X_TIER_A_FLAGS
        -Wall -Wextra -Wformat=2 -Wimplicit-fallthrough
        -Wstrict-prototypes -Wmissing-prototypes -Wold-style-definition
        -Wshadow -Wpointer-arith -Wundef -Wvla -Wwrite-strings
        -Werror=implicit-function-declaration
        -Werror=incompatible-pointer-types
        -Werror=int-conversion
        -Werror=format-security)
    foreach(flag IN LISTS ND500X_TIER_A_FLAGS)
        string(MAKE_C_IDENTIFIER "ND500X_CC_HAS${flag}" flag_var)
        check_c_compiler_flag(${flag} ${flag_var})
        if(${flag_var})
            target_compile_options(nd500x_warnings INTERFACE ${flag})
        endif()
    endforeach()
    if(ND500X_WERROR)
        target_compile_options(nd500x_warnings INTERFACE -Werror)
    endif()
endif()

# Make the project version visible to the code. The F12 menu shows it next to
# the build date and time, so "which binary am I actually running?" has an
# answer on screen rather than needing the file timestamp. Single source of
# truth: the project() line above.
add_compile_definitions(ND500X_VERSION="${PROJECT_VERSION}")

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
if(TARGET cjson_objects)
    # A parent project (nd100x) already built cJSON and put its header on
    # the include path. Fetching it a second time would download the same
    # sources and then be skipped by the guard below anyway.
    message(STATUS "cJSON provided by the parent project - not fetching")
elseif(PLATFORM_WASM OR BUILD_WASM)
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
    # Guarded: nd100x defines a target of the same name, and two
    # add_subdirectory()'d trees both creating it is a hard CMake error.
    if(NOT TARGET cjson_objects)
        add_library(cjson_objects OBJECT ${cjson_SOURCE_DIR}/cJSON.c)
        target_include_directories(cjson_objects PUBLIC ${cjson_SOURCE_DIR})
    endif()
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
            # Guarded - see the note on the WASM branch above.
            if(NOT TARGET cjson_objects)
                add_library(cjson_objects OBJECT ${cjson_SOURCE_DIR}/cJSON.c)
                target_include_directories(cjson_objects PUBLIC ${cjson_SOURCE_DIR})
            endif()
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

# MON call library (ndmonlib) - SINTRAN III monitor-call emulation.
#
# OPTIONAL since 2026-08-08. NDIX makes exactly ONE monitor call: MON 600, the
# ND-100 front-end call ("fecall"), issued from kernel machine/locore.c:227 as
# callg $0xf8000180. That one is served inside this repository by
# src/cpu/nd500_fecall.c and depends on nothing external. Everything else on
# segment 31 is SINTRAN emulation, and it now lives behind the seam declared in
# src/cpu/nd500_mon_sintran.h.
#
# Turn this OFF to build nd500_cpu + nd500_machine with NO ndmonlib dependency:
# the emulator still boots NDIX, and those two libraries can be linked into
# another host - nd100x's WASM build in particular - without this submodule,
# which lives in a separate private repository. The SINTRAN-facing parts (the
# debugger with its `mon` command, the native/WASM frontends and the test
# suite) do need it and are skipped in that configuration.
option(ND500X_WITH_NDMON "Build SINTRAN MON-call emulation (needs the ndmonlib submodule)" ON)

if(ND500X_WITH_NDMON)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/ndmonlib/CMakeLists.txt")
        add_subdirectory(external/ndmonlib)
    else()
        # This used to be two warnings followed by a confusing compile error.
        # Say it once, up front, and name both ways out.
        message(FATAL_ERROR
            "ndmonlib submodule not found at external/ndmonlib.\n"
            "  Fix:      git submodule update --init external/ndmonlib\n"
            "  Or build without SINTRAN MON emulation (NDIX still boots, it\n"
            "  only needs MON 600):  cmake -DND500X_WITH_NDMON=OFF ...")
    endif()
else()
    message(STATUS "ndmonlib DISABLED (ND500X_WITH_NDMON=OFF)"
                   " - MON 600 / NDIX only; debugger, frontends and tests skipped")
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
# The debugger (its `mon` command enumerates the ndmonlib registry) and both
# frontends are SINTRAN-facing and need ndmonlib. A WITH_NDMON=OFF build is a
# library-only build: nd500_cpu + nd500_machine for embedding elsewhere.
if(ND500X_WITH_NDMON)
    add_subdirectory(src/debugger)

    if(BUILD_WASM)
        add_subdirectory(src/frontend/nd500wasm)
    else()
        add_subdirectory(src/frontend/nd500x)
    endif()
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
# The test suite links `mon` directly (test_mon_calls and friends), so it only
# builds in the WITH_NDMON configuration.
if(ND500X_WITH_NDMON)
    add_subdirectory(test)
endif()
