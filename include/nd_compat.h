/*
 * nd_compat.h - small POSIX-vs-Win32 gaps that are not about terminals or
 * sockets (those are nd_tty.h and net_compat.h respectively).
 *
 * Everything here keeps POSIX SEMANTICS, not just POSIX spelling. That matters
 * most for nd_realpath(): the Win32 _fullpath() it is built on happily returns
 * an absolute path for a file that does not exist, while realpath() fails - and
 * nd500x_ndix.c relies on that failure to report "disk image not found". A
 * shim that only made the code compile would have turned a clear error message
 * into a confusing one later.
 */
#ifndef ND_COMPAT_H
#define ND_COMPAT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Resolve <path> to an absolute path with no symlinks, "." or ".." left in it,
 * writing the result into <resolved>, which must hold at least PATH_MAX bytes.
 *
 * Returns <resolved> on success and NULL if the path does not exist - the same
 * contract as POSIX realpath(path, resolved), which callers already test. */
char* nd_realpath(const char* path, char* resolved);

/* Set environment variable <name> to <value>. overwrite == 0 leaves an existing
 * value alone. Returns 0 on success, -1 on failure - same as POSIX setenv(). */
int nd_setenv(const char* name, const char* value, int overwrite);

#ifdef __cplusplus
}
#endif

#endif /* ND_COMPAT_H */
