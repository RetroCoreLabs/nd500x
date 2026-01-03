/*
 * SINTRAN III Path Translation Module
 *
 * Converts SINTRAN filenames to host filesystem paths.
 *
 * Reference: SINTRAN III System Supervisor (ND-830003)
 */

#include "mon_path.h"
#include "mon_config.h"
#include "mon_log.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#define PATH_SEP '\\'
#else
#include <unistd.h>
#define PATH_SEP '/'
#endif

/* ============================================================
 * String Helpers
 * ============================================================ */

size_t mon_strlen_sintran(const char* str) {
    if (!str) return 0;

    size_t len = 0;
    while (str[len] != '\0' && (uint8_t)str[len] != SINTRAN_STRING_END) {
        len++;
    }
    return len;
}

size_t mon_strcpy_sintran(char* dst, const char* src, size_t max_len) {
    if (!dst || max_len == 0) return 0;
    if (!src) {
        dst[0] = '\0';
        return 0;
    }

    size_t i = 0;
    while (i < max_len - 1 && src[i] != '\0' && (uint8_t)src[i] != SINTRAN_STRING_END) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
    return i;
}

/* Convert character to uppercase */
static char to_upper(char c) {
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 'A';
    }
    return c;
}

/* Copy string converting to uppercase */
static size_t strcpy_upper(char* dst, const char* src, size_t max_len) {
    if (!dst || max_len == 0) return 0;
    if (!src) {
        dst[0] = '\0';
        return 0;
    }

    size_t i = 0;
    while (i < max_len - 1 && src[i] != '\0') {
        dst[i] = to_upper(src[i]);
        i++;
    }
    dst[i] = '\0';
    return i;
}

/* ============================================================
 * Path Parsing
 * ============================================================ */

int mon_parse_sintran_name(const char* full_name,
                           char* user, size_t user_max,
                           char* name, size_t name_max,
                           char* ext, size_t ext_max) {
    if (!full_name || !name || name_max == 0) {
        return -1;
    }

    /* Initialize outputs */
    if (user && user_max > 0) user[0] = '\0';
    name[0] = '\0';
    if (ext && ext_max > 0) ext[0] = '\0';

    /* Get effective length (handle 0x27 terminator) */
    size_t len = mon_strlen_sintran(full_name);
    if (len == 0) {
        return -1;  /* Empty filename */
    }

    const char* p = full_name;
    const char* end = full_name + len;

    /* Check for (USER) prefix */
    if (*p == '(') {
        p++;
        const char* user_start = p;

        /* Find closing parenthesis */
        while (p < end && *p != ')') {
            p++;
        }

        if (p >= end) {
            return -1;  /* Missing closing parenthesis */
        }

        /* Extract user */
        if (user && user_max > 0) {
            size_t user_len = p - user_start;
            if (user_len >= user_max) user_len = user_max - 1;
            for (size_t i = 0; i < user_len; i++) {
                user[i] = to_upper(user_start[i]);
            }
            user[user_len] = '\0';
        }

        p++;  /* Skip ')' */
    }

    /* Find filename (up to ':' or end) */
    const char* name_start = p;
    while (p < end && *p != ':') {
        p++;
    }

    /* Extract name */
    size_t name_len = p - name_start;
    if (name_len == 0) {
        return -1;  /* Empty filename */
    }
    if (name_len >= name_max) name_len = name_max - 1;
    for (size_t i = 0; i < name_len; i++) {
        name[i] = to_upper(name_start[i]);
    }
    name[name_len] = '\0';

    /* Check for :EXT */
    if (p < end && *p == ':') {
        p++;  /* Skip ':' */
        if (ext && ext_max > 0 && p < end) {
            size_t ext_len = end - p;
            if (ext_len >= ext_max) ext_len = ext_max - 1;
            for (size_t i = 0; i < ext_len; i++) {
                ext[i] = to_upper(p[i]);
            }
            ext[ext_len] = '\0';
        }
    }

    return 0;
}

/* ============================================================
 * Path Translation
 * ============================================================ */

int mon_translate_path(const char* sintran_name, const char* sintran_type,
                       char* host_path, size_t max_len) {
    if (!sintran_name || !host_path || max_len == 0) {
        return -1;
    }

    char user[SINTRAN_MAX_USER + 1] = {0};
    char name[SINTRAN_MAX_NAME + 1] = {0};
    char ext[SINTRAN_MAX_TYPE + 1] = {0};

    /* Parse the SINTRAN name */
    if (mon_parse_sintran_name(sintran_name, user, sizeof(user),
                               name, sizeof(name), ext, sizeof(ext)) != 0) {
        /* Fallback: use filename directly */
        mon_strcpy_sintran(name, sintran_name, sizeof(name));
    }

    /* Use sintran_type if extension not in name */
    if (ext[0] == '\0' && sintran_type && sintran_type[0] != '\0' &&
        (uint8_t)sintran_type[0] != SINTRAN_STRING_END) {
        mon_strcpy_sintran(ext, sintran_type, sizeof(ext));
        /* Convert to uppercase */
        for (size_t i = 0; ext[i]; i++) {
            ext[i] = to_upper(ext[i]);
        }
    }

    /* Handle SCRATCH file naming convention:
     * Files starting with "SCRATCH-" should go to SCRATCH directory
     * even if no (SCRATCH) prefix was given. This handles cases where
     * programs create companion files for scratch files. */
    if (user[0] == '\0' && strncmp(name, "SCRATCH-", 8) == 0) {
        strcpy(user, "SCRATCH");
        mon_log(MON_LOG_DEBUG, "Auto-routing '%s' to SCRATCH directory", name);
    }

    /* Use current user if not specified */
    if (user[0] == '\0') {
        const char* current_user = mon_config_get_current_user();
        if (current_user) {
            strcpy_upper(user, current_user, sizeof(user));
        }
    }

    /* Build host path */
    const char* root = mon_config_get_sintran_root();
    if (!root || root[0] == '\0') {
        root = ".";
    }

    int written;
    if (user[0] && ext[0]) {
        written = snprintf(host_path, max_len, "%s%c%s%c%s.%s",
                          root, PATH_SEP, user, PATH_SEP, name, ext);
    } else if (user[0]) {
        written = snprintf(host_path, max_len, "%s%c%s%c%s",
                          root, PATH_SEP, user, PATH_SEP, name);
    } else if (ext[0]) {
        written = snprintf(host_path, max_len, "%s%c%s.%s",
                          root, PATH_SEP, name, ext);
    } else {
        written = snprintf(host_path, max_len, "%s%c%s",
                          root, PATH_SEP, name);
    }

    if (written < 0 || (size_t)written >= max_len) {
        return -1;  /* Buffer too small */
    }

    mon_log(MON_LOG_DEBUG, "Path translation: '%s' + '%s' -> '%s'",
            sintran_name, sintran_type ? sintran_type : "", host_path);

    /* Ensure parent directory exists */
    char dir_path[SINTRAN_MAX_PATH];
    strncpy(dir_path, host_path, sizeof(dir_path) - 1);
    dir_path[sizeof(dir_path) - 1] = '\0';

    /* Find last path separator */
    char* last_sep = strrchr(dir_path, PATH_SEP);
#ifdef _WIN32
    char* last_sep2 = strrchr(dir_path, '/');
    if (last_sep2 > last_sep) last_sep = last_sep2;
#endif
    if (last_sep) {
        *last_sep = '\0';
        mon_ensure_directory(dir_path);
    }

    return 0;
}

/* ============================================================
 * Directory Creation
 * ============================================================ */

int mon_ensure_directory(const char* path) {
    if (!path || path[0] == '\0') {
        return -1;
    }

    /* Make a mutable copy */
    char tmp[SINTRAN_MAX_PATH];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) {
        return -1;
    }
    strcpy(tmp, path);

    /* Remove trailing separator if present */
    if (len > 0 && (tmp[len - 1] == '/' || tmp[len - 1] == '\\')) {
        tmp[len - 1] = '\0';
        len--;
    }

    /* Create each directory component */
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';

            /* Try to create directory (ignore if exists) */
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                /* Check if it already exists as a directory */
                struct stat st;
                if (stat(tmp, &st) != 0 || !S_ISDIR(st.st_mode)) {
                    mon_log(MON_LOG_ERROR, "Failed to create directory '%s': %s",
                            tmp, strerror(errno));
                    return -1;
                }
            }

            *p = PATH_SEP;
        }
    }

    /* Create final directory */
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        struct stat st;
        if (stat(tmp, &st) != 0 || !S_ISDIR(st.st_mode)) {
            mon_log(MON_LOG_ERROR, "Failed to create directory '%s': %s",
                    tmp, strerror(errno));
            return -1;
        }
    }

    return 0;
}
