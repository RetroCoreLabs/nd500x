/*
 * Locating the ND-500 test data tree (DOM and NRF files) WITHOUT putting a
 * machine-specific path in the repository.
 *
 * The diag_* harnesses used to carry absolute literals like
 * "/mnt/d/ND/500/FraTor/nc/nc-a06.dom". A repo that hardcodes a drive letter
 * or a home directory only works on the machine it was written on, and these
 * files are checked in.
 *
 * The data tree itself is large and lives outside this repo, so it cannot be
 * vendored. It is located through the environment instead:
 *
 *     export ND500_TESTDATA=/path/to/ND/500
 *
 * and a harness asks for a path RELATIVE to that root:
 *
 *     const char* dom = nd500_testdata("FraTor/nc/nc-a06.dom");
 *
 * A missing variable is a hard, explanatory failure rather than a confusing
 * "file not found" on a path the user never chose.
 */

#ifndef ND500_TESTDATA_H
#define ND500_TESTDATA_H

#include <stdio.h>
#include <stdlib.h>

/* Returns an absolute path to `rel` inside the data tree.
 *
 * The returned pointer is one of a small rotating set of buffers, so a caller
 * may hold a few results at once (e.g. two DOM paths passed to one function)
 * without the earlier ones being overwritten. Do not keep more than
 * ND500_TESTDATA_SLOTS of them alive. */
#define ND500_TESTDATA_SLOTS 4

static const char* nd500_testdata(const char* rel) {
    static char buf[ND500_TESTDATA_SLOTS][1024];
    static unsigned next = 0;

    const char* root = getenv("ND500_TESTDATA");
    if (!root || !root[0]) {
        fprintf(stderr,
                "error: ND500_TESTDATA is not set.\n"
                "       It must point at the ND-500 data tree holding %s\n"
                "       e.g. export ND500_TESTDATA=/path/to/ND/500\n", rel);
        exit(2);
    }

    char* out = buf[next];
    next = (next + 1u) % ND500_TESTDATA_SLOTS;
    snprintf(out, sizeof buf[0], "%s/%s", root, rel);
    return out;
}

#endif /* ND500_TESTDATA_H */
