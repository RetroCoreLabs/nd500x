# libmon - SINTRAN III Monitor Call Emulation Library
#
# Provides MON call handling for ND-500 emulation.
# Can be used standalone or linked into nd500x.

# Collect all handler source files
file(GLOB MON_HANDLER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/handlers/*.c
)

# Create the library
add_library(mon STATIC
    mon_config.c
    mon_dispatch.c
    mon_file_table.c
    mon_log.c
    mon_params.c
    mon_path.c
    mon_registry.c
    mon_terminal_state.c
    ${MON_HANDLER_SOURCES}
)

# Public include directory
target_include_directories(mon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Compile options
target_compile_options(mon PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
)

# Count handlers for status message
list(LENGTH MON_HANDLER_SOURCES MON_HANDLER_COUNT)
message(STATUS "libmon: Found ${MON_HANDLER_COUNT} MON handlers")
