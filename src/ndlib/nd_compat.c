/*
 * nd_compat.c - the two implementations behind include/nd_compat.h.
 */
#include "nd_compat.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef _WIN32
#  include <windows.h>
#  include <io.h>         /* _access */
#  ifndef PATH_MAX
#    define PATH_MAX MAX_PATH
#  endif
#else
#  include <unistd.h>
#endif

char* nd_realpath(const char* path, char* resolved) {
    if (!path || !resolved) return NULL;

#ifdef _WIN32
    /* _fullpath() does the "make it absolute and fold away . and .." half. It
     * does NOT check that the result exists - unlike realpath(), which fails
     * with ENOENT. Callers depend on that failure to detect a missing file, so
     * the existence test has to be added by hand. */
    if (!_fullpath(resolved, path, PATH_MAX)) return NULL;
    if (_access(resolved, 0) != 0) return NULL;   /* mode 0 = "does it exist" */
    return resolved;
#else
    return realpath(path, resolved);
#endif
}

int nd_setenv(const char* name, const char* value, int overwrite) {
    if (!name || !name[0] || !value) return -1;

#ifdef _WIN32
    /* Win32 has no setenv. _putenv_s always overwrites, so the overwrite==0
     * case has to be handled here rather than by the call. */
    if (!overwrite) {
        const char* cur = getenv(name);
        if (cur && cur[0]) return 0;              /* already set: leave it */
    }
    return _putenv_s(name, value) == 0 ? 0 : -1;
#else
    return setenv(name, value, overwrite);
#endif
}
