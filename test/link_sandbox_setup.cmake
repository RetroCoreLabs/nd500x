# Builds the ND linker sandbox, the directory the linker has to be RUN FROM.
#
#   cmake -DSANDBOX=<dir> -DTESTDATA=<dir> -P link_sandbox_setup.cmake
#
# Normally invoked as `make link-sandbox`, which fills both in.
#
# Why this exists: build/link_sandbox is where the ND linker works and nowhere
# else. Run it from build/nc_sandbox instead - that is the COMPILER's sandbox -
# and it fails at startup in a way that reads like a real defect. Until now
# nothing in the repository created the directory, it lives under gitignored
# build/, and `make clean` deletes it, so the knowledge of what belongs in it
# survived only in prose in four documents.
#
# The counterpart for the compiler is nc_sandbox_setup.cmake, which is simpler
# because everything it needs is tracked in test/nc_fixtures/. The linker needs
# real 1980s vendor binaries that are far too large to vendor, so they come from
# $ND500_TESTDATA, indexed in docs/EXTERNAL-ARTIFACTS.md.
#
# Layout, from docs/CARVE-QUESTION-USER-SYSTEM-FILE-LOOKUP.md lines 62-64, which
# recorded it from a live trace:
#
#   <sandbox>/GUEST/    the user's own files - LINKER.INIT, LINKER.HELP, the
#                       DDBTABLES terminal table, .NRF objects being linked,
#                       and the .DOM the linker creates
#   <sandbox>/SYSTEM/   the libraries: CAT-LIB.NRF, NC-LIB.NRF and the rest of
#                       the C runtime the linked program pulls in
#   <sandbox>/SCRATCH/  scratch files
#
# Files are copied by glob, not by name. Naming files this script has never
# seen would be guessing, and a wrong name here produces a sandbox that looks
# populated and fails later as something else. What is actually copied is
# printed, so a short list is visible rather than silent.

if(NOT DEFINED SANDBOX OR NOT DEFINED TESTDATA)
    message(FATAL_ERROR "SANDBOX and TESTDATA must be defined")
endif()

if(NOT IS_DIRECTORY "${TESTDATA}")
    message(FATAL_ERROR
        "ND500_TESTDATA does not point at a directory: ${TESTDATA}\n"
        "This tree holds the vendor binaries and is not part of the repository.\n"
        "Run 'make doctor' to see which outside trees this checkout can reach.")
endif()

set(LINKER_DIR "${TESTDATA}/nd-linker")
set(CLIBS_DIR  "${TESTDATA}/c-libs")

if(NOT IS_DIRECTORY "${LINKER_DIR}")
    message(FATAL_ERROR
        "Missing ${LINKER_DIR}\n"
        "Expected the ND Linker and its .init/.help files there - see the\n"
        "'Vendor programs and libraries' table in docs/EXTERNAL-ARTIFACTS.md.")
endif()

# Start from nothing. A sandbox half-left-over from a previous run is the same
# class of problem as a stale binary: it tests a state nobody chose.
file(REMOVE_RECURSE "${SANDBOX}")
file(MAKE_DIRECTORY "${SANDBOX}/GUEST")
file(MAKE_DIRECTORY "${SANDBOX}/SYSTEM")
file(MAKE_DIRECTORY "${SANDBOX}/SCRATCH")

# GUEST: the linker's own init and help text, plus any auto-job scripts.
# SINTRAN writes LINKER:INIT where the host filesystem holds LINKER.INIT, so the
# separator is translated on the way in.
set(_copied "")
file(GLOB _linker_files
    "${LINKER_DIR}/*.init" "${LINKER_DIR}/*.INIT"
    "${LINKER_DIR}/*.help" "${LINKER_DIR}/*.HELP"
    "${LINKER_DIR}/*.job"  "${LINKER_DIR}/*.JOB")
foreach(_f IN LISTS _linker_files)
    get_filename_component(_n "${_f}" NAME)
    string(TOUPPER "${_n}" _n)
    string(REPLACE ":" "." _n "${_n}")
    configure_file("${_f}" "${SANDBOX}/GUEST/${_n}" COPYONLY)
    list(APPEND _copied "GUEST/${_n}")

    # The vendor files carry a revision in the name, linker-b01.init, but the
    # linker opens LINKER:INIT - no revision - which the host filesystem sees as
    # LINKER.INIT. docs/CARVE-QUESTION-USER-SYSTEM-FILE-LOOKUP.md line 63 records
    # the sandbox holding exactly that name. Both are placed: the revisioned file
    # under its own name, plus this alias, so it works whichever name the build of
    # the linker in use actually asks for.
    get_filename_component(_ext "${_n}" LAST_EXT)
    string(TOUPPER "${_ext}" _ext)
    if(_ext STREQUAL ".INIT" OR _ext STREQUAL ".HELP")
        if(NOT _n STREQUAL "LINKER${_ext}")
            configure_file("${_f}" "${SANDBOX}/GUEST/LINKER${_ext}" COPYONLY)
            list(APPEND _copied "GUEST/LINKER${_ext} (alias of ${_n})")
        endif()
    endif()
endforeach()

# SYSTEM: the C and CAT runtime libraries the linked program needs. Unqualified
# opens fall back to SYSTEM inside the resolver, which is why these do not go in
# GUEST - see docs/CARVE-QUESTION-USER-SYSTEM-FILE-LOOKUP.md.
if(IS_DIRECTORY "${CLIBS_DIR}")
    file(GLOB _libs "${CLIBS_DIR}/*.nrf" "${CLIBS_DIR}/*.NRF")
    foreach(_f IN LISTS _libs)
        get_filename_component(_n "${_f}" NAME)
        string(TOUPPER "${_n}" _n)
        configure_file("${_f}" "${SANDBOX}/SYSTEM/${_n}" COPYONLY)
        list(APPEND _copied "SYSTEM/${_n}")
    endforeach()
else()
    message(WARNING
        "No ${CLIBS_DIR} - the sandbox has no libraries in SYSTEM/.\n"
        "The linker will start and accept commands, but LOAD of a library\n"
        "object will fail -46.")
endif()

# The DDBTABLES terminal table. The linker reads its terminal geometry from this
# file and never from a built-in table (docs/SINTRAN-CONVENTIONS.md line 52), so
# without it the linker misdraws its screen. It is not a loose file anywhere: it
# has to be extracted out of a SINTRAN disk image first, which is why it is
# looked for rather than copied from a fixed place.
set(_ddb_found FALSE)
foreach(_dir "${SANDBOX}/GUEST" "${LINKER_DIR}" "${TESTDATA}")
    file(GLOB _ddb "${_dir}/DDBTABLES*.VTM" "${_dir}/DDBTABLES*.vtm")
    if(_ddb)
        list(GET _ddb 0 _ddb_one)
        get_filename_component(_n "${_ddb_one}" NAME)
        string(TOUPPER "${_n}" _n)
        configure_file("${_ddb_one}" "${SANDBOX}/GUEST/${_n}" COPYONLY)
        list(APPEND _copied "GUEST/${_n}")
        set(_ddb_found TRUE)
        break()
    endif()
endforeach()

if(NOT _ddb_found)
    message(WARNING
        "No DDBTABLES-*.VTM found under ${TESTDATA}.\n"
        "It is not shipped loose - extract it from the SINTRAN disk image\n"
        "ND-disk-00047.img. The linker runs without it but draws its screen\n"
        "from a terminal table it then does not have.")
endif()

list(LENGTH _copied _n_copied)
message(STATUS "Link sandbox built at ${SANDBOX} (${_n_copied} files)")
foreach(_c IN LISTS _copied)
    message(STATUS "  ${_c}")
endforeach()
message(STATUS "Run the linker from that directory, not from nc_sandbox.")
