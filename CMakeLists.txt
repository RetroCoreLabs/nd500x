cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Create a build directory and run cmake from there.")
endif()

project(nd500x VERSION 0.1.0 LANGUAGES C)

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
if(PLATFORM_WASM OR BUILD_WASM)
    include(FetchContent)
    FetchContent_Declare(cjson GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git GIT_TAG master)
    FetchContent_MakeAvailable(cjson)
    add_library(cjson_objects OBJECT ${cjson_SOURCE_DIR}/cJSON.c)
    target_include_directories(cjson_objects PUBLIC ${cjson_SOURCE_DIR})
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include/cjson)
    configure_file(${cjson_SOURCE_DIR}/cJSON.h ${CMAKE_BINARY_DIR}/include/cjson/cJSON.h COPYONLY)
    include_directories(${CMAKE_BINARY_DIR}/include)
else()
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(CJSON QUIET cjson)
    endif()
    if(CJSON_FOUND)
        add_compile_definitions(HAVE_SYSTEM_CJSON)
        include_directories(${CJSON_INCLUDE_DIRS})
    else()
        message(STATUS "System cJSON not found. Native build can proceed without JSON output.")
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

# Optionally include externals if present
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/libdap")
    add_definitions(-DHAVE_LIBDAP)
    add_subdirectory(external/libdap)
    if(TARGET dap_objects AND TARGET cjson_objects)
        target_link_libraries(dap_objects PRIVATE cjson_objects)
    endif()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/libsymbols")
    add_definitions(-DHAVE_LIBSYMBOLS)
    add_subdirectory(external/libsymbols)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(src/ndlib)
    add_subdirectory(src/disasm)
    add_subdirectory(src/machine)
add_subdirectory(src/cpu)
add_subdirectory(src/debugger)

if(BUILD_WASM)
    add_subdirectory(src/frontend/nd500wasm)
else()
    add_subdirectory(src/frontend/nd500x)
endif()

# Instruction generator tool and codegen step
if(PLATFORM_WASM OR BUILD_WASM)
    # For WASM builds, use native gen_instructions tool
    set(NATIVE_BUILD_DIR "${CMAKE_SOURCE_DIR}/build")
    set(NATIVE_GEN_TOOL "${NATIVE_BUILD_DIR}/bin/gen_instructions")
    set(INSTR_JSON "${CMAKE_SOURCE_DIR}/src/cpu/instructions.json")
    set(GEN_H "${CMAKE_BINARY_DIR}/include/nd500_instructions_gen.h")
    set(GEN_C "${CMAKE_BINARY_DIR}/nd500_instructions_gen.c")
    
    add_custom_command(
        OUTPUT ${GEN_H} ${GEN_C}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/include
        COMMAND ${NATIVE_GEN_TOOL} ${INSTR_JSON} ${GEN_H} ${GEN_C}
        DEPENDS ${NATIVE_GEN_TOOL} ${INSTR_JSON}
        COMMENT "Generating ND-500 instruction tables from JSON (using native tool)"
        VERBATIM
    )
    
    add_custom_target(nd500_instr_codegen DEPENDS ${GEN_H} ${GEN_C})
    add_library(nd500_instr STATIC ${GEN_C})
    add_dependencies(nd500_instr nd500_instr_codegen)
    target_include_directories(nd500_instr PUBLIC ${CMAKE_BINARY_DIR}/include)
    target_link_libraries(nd500_cpu PUBLIC nd500_instr)
    target_include_directories(nd500_cpu PUBLIC ${CMAKE_BINARY_DIR}/include)
else()
    # For native builds, build and use the gen_instructions tool
    add_subdirectory(tools/gen_instructions)
    set(INSTR_JSON "${CMAKE_BINARY_DIR}/src/cpu/instructions.json")
    set(GEN_H "${CMAKE_BINARY_DIR}/include/nd500_instructions_gen.h")
    set(GEN_C "${CMAKE_BINARY_DIR}/nd500_instructions_gen.c")
    add_custom_command(
        OUTPUT ${GEN_H} ${GEN_C}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/include
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/src/cpu
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/src/cpu/instructions.json ${INSTR_JSON}
        COMMAND gen_instructions ${INSTR_JSON} ${GEN_H} ${GEN_C}
        DEPENDS gen_instructions ${INSTR_JSON}
        COMMENT "Generating ND-500 instruction tables from JSON"
        VERBATIM
    )
    
    add_custom_target(nd500_instr_codegen DEPENDS ${GEN_H} ${GEN_C})
    add_library(nd500_instr STATIC ${GEN_C})
    add_dependencies(nd500_instr nd500_instr_codegen)
    target_include_directories(nd500_instr PUBLIC ${CMAKE_BINARY_DIR}/include)
    target_link_libraries(nd500_cpu PUBLIC nd500_instr)
    target_include_directories(nd500_cpu PUBLIC ${CMAKE_BINARY_DIR}/include)
endif()

if(PLATFORM_WASM OR BUILD_WASM)
    if(TARGET nd500wasm)
        set_property(TARGET nd500wasm APPEND_STRING PROPERTY LINK_FLAGS " -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS','UTF8ToString','lengthBytesUTF8','HEAPU8'] -s ALLOW_MEMORY_GROWTH=1")
        set_property(TARGET nd500wasm APPEND_STRING PROPERTY LINK_FLAGS " -s EXPORTED_FUNCTIONS=['_malloc','_free','_nd500wasm_init','_nd500_dbg_mem_json','_nd500_dbg_disasm_json','_nd500_dbg_regs_json','_nd500_dbg_step_js','_nd500_dbg_run_js','_nd500_dbg_stop_js','_nd500_dbg_load_aout_js','_nd500_dbg_load_aout_path_js','_nd500_dbg_bp_add_js','_nd500_dbg_bp_del_js','_nd500_dbg_bp_enable_js','_nd500_dbg_bp_disable_js','_nd500_dbg_bp_list_json','_nd500_dbg_status_json','_nd500_dbg_traps_json','_nd500_dbg_clear_traps_js','_nd500_dbg_set_reg_js','_nd500_dbg_instr_count_js','_nd500_dbg_mnemonic_js']")
        # Enable modularized factory for Node/web and ensure it works under Node tests
        set_property(TARGET nd500wasm APPEND_STRING PROPERTY LINK_FLAGS " -s MODULARIZE=1 -s EXPORT_NAME='ND500Wasm' -s ENVIRONMENT='web,node'")
        
        # Configure output to bin/ directory with .js extension
        set_target_properties(nd500wasm PROPERTIES 
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
            OUTPUT_NAME "nd500wasm")
        
        # Copy web files to build output
        file(COPY ${CMAKE_SOURCE_DIR}/src/frontend/nd500wasm/web/index.html 
             DESTINATION ${CMAKE_BINARY_DIR}/bin/)
        file(COPY ${CMAKE_SOURCE_DIR}/src/frontend/nd500wasm/web/style.css 
             DESTINATION ${CMAKE_BINARY_DIR}/bin/)
        file(COPY ${CMAKE_SOURCE_DIR}/src/frontend/nd500wasm/web/debugger.js 
             DESTINATION ${CMAKE_BINARY_DIR}/bin/)
    endif()
endif()


enable_testing()
add_subdirectory(test)
