# radix_gate.cmake - run with: cmake -DSRC_DIR=<repo> -P radix_gate.cmake
#
# Three number systems share this source tree and its logs: hex (0x8E30),
# C octal (a LEADING ZERO is octal), and ND octal written with a B suffix
# (46B, 102B). Confusing them has produced wrong conclusions that then
# survived several rounds of analysis - see docs/INVESTIGATION-TRAPS.md.
#
# WHAT THIS DOES NOT DO. It does not ban C octal literals. There are 779 of
# them across 190 files and nearly all are CORRECT: the ND machine's addresses
# and field offsets are octal in every manual and every carve note, so
# 012243 is the honest way to write them. A gate that rejected them would be
# a huge mechanical refactor that made the source LESS faithful to its sources.
#
# WHAT IT CATCHES is the measured bug, and only that: a literal written as
# OCTAL on a line whose own comment documents the same digits as DECIMAL. The
# incident was a structure stride written
#
#     static const uint32_t desc_stride = 0100u;   /* 0o144 = 100 bytes */
#
# where the literal is 64 and the comment says 100. The author wrote one radix
# and documented the other, on one line, and the dump then read every entry at
# the wrong address and reported them absent. That contradiction is mechanical
# to find and is never intentional.
#
# NARROW BEATS CLEVER. A wider check - "flag any octal whose digits could be
# read as decimal" - fires on hundreds of correct ND addresses, and a gate that
# fires on good code is one people learn to skip, which is the failure the gate
# exists to prevent.

if(NOT DEFINED SRC_DIR)
    message(FATAL_ERROR "radix_gate: SRC_DIR not set")
endif()

if(NOT DEFINED SCAN_DIRS)
    set(SCAN_DIRS "src")
endif()

set(SOURCES "")
foreach(D ${SCAN_DIRS})
    file(GLOB_RECURSE FOUND "${SRC_DIR}/${D}/*.c" "${SRC_DIR}/${D}/*.h")
    list(APPEND SOURCES ${FOUND})
endforeach()

set(OFFENDERS "")

foreach(F ${SOURCES})
    get_filename_component(NAME_ONLY "${F}" NAME)

    # Generated files are not hand-written and carry no comments to contradict.
    if(NAME_ONLY MATCHES "^nd500_instructions\\.(c|h)$")
        continue()
    endif()

    file(STRINGS "${F}" LINES)
    foreach(LINE ${LINES})
        # A C octal literal: a leading 0 followed by at least one octal digit,
        # not preceded by an identifier character, a dot or another digit (so
        # 0x..., 10, 1.0 and FOO0123 are all left alone).
        if(NOT LINE MATCHES "(^|[^0-9A-Za-z_.])0([0-7]+)[uUlL]*")
            continue()
        endif()
        set(OCTAL_DIGITS "${CMAKE_MATCH_2}")

        # A single-digit literal cannot be a decimal/octal contradiction: 00
        # through 07 have the same value in both radices.
        string(LENGTH "${OCTAL_DIGITS}" NDIGITS)
        if(NDIGITS LESS 2)
            continue()
        endif()

        # Only lines that carry their own comment can contradict themselves.
        if(NOT LINE MATCHES "(/\\*|//|%)")
            continue()
        endif()
        string(REGEX REPLACE "^.*(/\\*|//|%)" "" COMMENT_TEXT "${LINE}")

        # The contradiction: the comment states the SAME digit string as a
        # plain decimal number. Written with a leading zero, or as 0oNNN, or
        # with a B suffix, the comment agrees with the literal and is fine.
        if(COMMENT_TEXT MATCHES "(^|[^0-9A-Za-z_.])${OCTAL_DIGITS}([^0-9A-Za-z_]|$)")
            string(STRIP "${LINE}" TRIMMED)
            file(RELATIVE_PATH REL "${SRC_DIR}" "${F}")
            list(APPEND OFFENDERS "  ${REL}:  ${TRIMMED}")
        endif()
    endforeach()
endforeach()

if(OFFENDERS)
    string(REPLACE ";" "\n" OFFENDERS_TEXT "${OFFENDERS}")
    message(FATAL_ERROR
        "A literal is written in OCTAL while the comment on the same line "
        "documents the same digits as DECIMAL:\n${OFFENDERS_TEXT}\n\n"
        "A leading zero makes a C literal octal, so 0100 is 64. Write the "
        "decimal value and put the octal in the comment, not the reverse. "
        "See docs/INVESTIGATION-TRAPS.md section 1.")
endif()

message(STATUS "radix_gate: clean - no octal literal contradicts its own comment")
