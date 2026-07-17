# Resets the NC compile-test sandbox before the dom_nc_compile_* tests run.
# Invoked as a CTest fixture setup:
#   cmake -DSANDBOX=<dir> -DFIXTURES=<dir> -P nc_sandbox_setup.cmake

if(NOT DEFINED SANDBOX OR NOT DEFINED FIXTURES)
    message(FATAL_ERROR "SANDBOX and FIXTURES must be defined")
endif()

file(REMOVE_RECURSE "${SANDBOX}")
file(MAKE_DIRECTORY "${SANDBOX}/SCRATCH")
file(COPY "${FIXTURES}/GUEST" DESTINATION "${SANDBOX}")
file(COPY "${FIXTURES}/expected" DESTINATION "${SANDBOX}")

# Pre-create NC's output files. NC opens A:NRF / A:LIST unquoted for write, and
# SINTRAN has no write-open-creates: an unquoted open of a missing file fails
# 056B. On a real ND system these output files pre-exist (or are @CREATE-FILEd);
# the sandbox mirrors that. Byte-verified: CARVE-ANSWER-OPEN-QUOTED-FILENAME.md.
foreach(f A.NRF A.LIST B.NRF B.LIST)
    file(WRITE "${SANDBOX}/GUEST/${f}" "")
endforeach()

message(STATUS "NC sandbox reset at ${SANDBOX}")
