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

message(STATUS "NC sandbox reset at ${SANDBOX}")
