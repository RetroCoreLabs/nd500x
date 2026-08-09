# no_getenv_gate.cmake - run with: cmake -DSRC_DIR=<repo> -P no_getenv_gate.cmake
#
# The emulator libraries must not read the environment. Settings arrive through
# Nd500Settings (src/cpu/nd500_settings.h); the ONLY place allowed to call
# getenv is the loader that fills that struct.
#
# This gate exists because the previous arrangement - every file reaching for
# getenv on its own - drifted badly: knobs that nothing read, knobs nothing
# documented, four flags silently inverted, and one that never had any effect.
# A raw getenv creeping back in is how that starts again, and it is invisible
# in review, so it is checked mechanically instead.

set(ALLOWED "nd500_settings.c")

file(GLOB_RECURSE SOURCES
     "${SRC_DIR}/src/cpu/*.c"  "${SRC_DIR}/src/cpu/*.h"
     "${SRC_DIR}/src/machine/*.c" "${SRC_DIR}/src/machine/*.h")

set(OFFENDERS "")
foreach(F ${SOURCES})
    get_filename_component(NAME_ONLY "${F}" NAME)
    list(FIND ALLOWED "${NAME_ONLY}" IS_ALLOWED)
    if(NOT IS_ALLOWED EQUAL -1)
        continue()
    endif()

    file(STRINGS "${F}" HITS REGEX "getenv[ \t]*\\(")
    foreach(LINE ${HITS})
        # Skip comment lines - the word getenv appears in several explanations
        # of why it is no longer called.
        string(REGEX MATCH "^[ \t]*(\\*|/\\*|//)" IS_COMMENT "${LINE}")
        if(IS_COMMENT STREQUAL "")
            file(RELATIVE_PATH REL "${SRC_DIR}" "${F}")
            string(STRIP "${LINE}" LINE)
            list(APPEND OFFENDERS "  ${REL}:  ${LINE}")
        endif()
    endforeach()
endforeach()

if(OFFENDERS)
    string(REPLACE ";" "\n" OFFENDERS_TEXT "${OFFENDERS}")
    message(FATAL_ERROR
        "getenv() found in the emulator libraries:\n${OFFENDERS_TEXT}\n\n"
        "Settings belong in Nd500Settings - add a field in "
        "src/cpu/nd500_settings.h, map it in nd500_settings_load_env(), and "
        "read the field here. See the catalog note at the top of that header.")
endif()

message(STATUS "settings_no_getenv: clean - no getenv outside nd500_settings.c")
