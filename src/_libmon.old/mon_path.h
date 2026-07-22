/*
 * SINTRAN III Path Translation Module
 *
 * Converts SINTRAN filenames to host filesystem paths.
 *
 * SINTRAN format: (USER)NAME:EXT
 * Host format:    {root}/USER/NAME.EXT
 *
 * Reference: SINTRAN III System Supervisor (ND-830003)
 */

#ifndef MON_PATH_H
#define MON_PATH_H

#include <stddef.h>
#include <stdint.h>

/* Maximum lengths for SINTRAN name components */
#define SINTRAN_MAX_USER     16   /* Max user/directory name */
#define SINTRAN_MAX_NAME     16   /* Max filename */
#define SINTRAN_MAX_TYPE      4   /* Max file type/extension */
#define SINTRAN_MAX_PATH    256   /* Max full path */

/* SINTRAN string terminator */
#define SINTRAN_STRING_END  0x27  /* ASCII apostrophe */

/**
 * Parse SINTRAN filename into components.
 *
 * Input formats:
 *   "(USER)NAME:EXT"  -> user="USER", name="NAME", ext="EXT"
 *   "NAME:EXT"        -> user=NULL, name="NAME", ext="EXT"
 *   "NAME"            -> user=NULL, name="NAME", ext=NULL
 *
 * Handles 0x27 (apostrophe) as string terminator.
 *
 * Parameters:
 *   full_name - SINTRAN filename (may be 0x27 terminated)
 *   user      - Output buffer for user (can be NULL to skip)
 *   user_max  - Size of user buffer
 *   name      - Output buffer for filename
 *   name_max  - Size of name buffer
 *   ext       - Output buffer for extension (can be NULL to skip)
 *   ext_max   - Size of ext buffer
 *
 * Returns: 0 on success, -1 on error (invalid format, buffer too small)
 */
int mon_parse_sintran_name(const char* full_name,
                           char* user, size_t user_max,
                           char* name, size_t name_max,
                           char* ext, size_t ext_max);

/**
 * Translate SINTRAN path to host filesystem path.
 *
 * Input:  sintran_name="(SCRATCH)SCRATCH64", sintran_type="DATA"
 * Output: host_path="{sintran_root}/SCRATCH/SCRATCH64.DATA"
 *
 * If user not specified in sintran_name, uses current user from config.
 * If sintran_type is NULL or empty, no extension is added.
 *
 * Parameters:
 *   sintran_name - SINTRAN filename (may include (USER) prefix)
 *   sintran_type - SINTRAN file type/extension (can be NULL)
 *   host_path    - Output buffer for host path
 *   max_len      - Size of output buffer
 *
 * Returns: 0 on success, -1 on error (buffer too small, invalid input)
 */
int mon_translate_path(const char* sintran_name, const char* sintran_type,
                       char* host_path, size_t max_len);

/**
 * Resolve a SINTRAN name for LOOKUP (opening an existing file), applying the
 * verified GFILI fallback: search the caller's own directory first, and if the
 * file is not found there AND no (USER) was named, retry under user SYSTEM.
 * On success host_path is set to a path that exists (own or SYSTEM); if the file
 * is found in neither, host_path holds the own-directory path (so create/error
 * paths behave as before). Suppressed when a user is named or for SCRATCH- names.
 * Use mon_translate_path (not this) for creates - the fallback is lookup-only.
 *
 * Returns: 0 on success (path built), -1 on error (invalid input/buffer).
 */
int mon_translate_path_lookup(const char* sintran_name, const char* sintran_type,
                              char* host_path, size_t max_len);

/**
 * Ensure directory exists, creating if necessary.
 *
 * Creates parent directories as needed (like mkdir -p).
 *
 * Parameters:
 *   path - Directory path to create
 *
 * Returns: 0 on success, -1 on error
 */
int mon_ensure_directory(const char* path);

/**
 * Copy SINTRAN string, handling 0x27 terminator.
 *
 * Copies up to max_len-1 characters or until 0x27/0x00 is encountered.
 * Output is always null-terminated.
 *
 * Parameters:
 *   dst     - Destination buffer
 *   src     - Source SINTRAN string
 *   max_len - Size of destination buffer
 *
 * Returns: Number of characters copied (not including null terminator)
 */
size_t mon_strcpy_sintran(char* dst, const char* src, size_t max_len);

/**
 * Get length of SINTRAN string (up to 0x27 or 0x00 terminator).
 *
 * Parameters:
 *   str - SINTRAN string
 *
 * Returns: Length of string (not including terminator)
 */
size_t mon_strlen_sintran(const char* str);

#endif /* MON_PATH_H */
