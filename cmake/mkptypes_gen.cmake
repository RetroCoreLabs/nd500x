# mkptypes_gen.cmake - run mkptypes over every .c file under a directory and
# write the result as one generated prototype header.
#
# SPDX-License-Identifier: MIT
# Copyright (c) 2025-2026 Ronny Hansen
#
# See LICENSE in the repository root for the full text.
#
# The output is exactly what nd100x's per-module rule produces:
#     cmake -E echo "/* AUTO-GENERATED FILE. DO NOT EDIT! */" > x_protos.h
#     mkptypes a.c >> x_protos.h
#     mkptypes b.c >> x_protos.h
# with the per-file loop run here instead of as one COMMAND per file. The
# Ninja generator joins all COMMANDs of a custom command into a single command
# line, and on Windows that line goes through cmd.exe, whose limit is 8191
# characters. nd500x has 242 instruction handler files.
#
# Usage, from add_custom_command:
#   cmake -DMKPTYPES=<tool> -DSRC_DIR=<dir> -DOUT=<header> -P mkptypes_gen.cmake

foreach(required MKPTYPES SRC_DIR OUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "mkptypes_gen.cmake: -D${required}=... is required")
    endif()
endforeach()

file(GLOB_RECURSE sources LIST_DIRECTORIES false "${SRC_DIR}/*.c")
list(SORT sources)

set(text "/* AUTO-GENERATED FILE. DO NOT EDIT! */\n")
foreach(src IN LISTS sources)
    execute_process(
        COMMAND "${MKPTYPES}" "${src}"
        OUTPUT_VARIABLE protos
        RESULT_VARIABLE rc)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "mkptypes failed (${rc}) on ${src}")
    endif()
    string(APPEND text "${protos}")
endforeach()

file(WRITE "${OUT}" "${text}")
