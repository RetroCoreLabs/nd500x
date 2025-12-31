/*
 * SINTRAN III Configuration Module
 *
 * Stores configuration for SINTRAN file system emulation.
 *
 * Reference: SINTRAN III System Supervisor (ND-830003)
 */

#include "mon_config.h"
#include <string.h>
#include <stdlib.h>

/* Default values */
#define DEFAULT_SINTRAN_ROOT "."
#define DEFAULT_CURRENT_USER "GUEST"
#define DEFAULT_AUTO_SCRATCH_64 1

/* Maximum lengths */
#define MAX_ROOT_PATH 512
#define MAX_USER_NAME 32

/* Static storage */
static char g_sintran_root[MAX_ROOT_PATH] = DEFAULT_SINTRAN_ROOT;
static char g_current_user[MAX_USER_NAME] = DEFAULT_CURRENT_USER;
static int g_auto_scratch_64 = DEFAULT_AUTO_SCRATCH_64;

/* ============================================================
 * SINTRAN Root Directory
 * ============================================================ */

const char* mon_config_get_sintran_root(void) {
    return g_sintran_root;
}

void mon_config_set_sintran_root(const char* path) {
    if (path && path[0] != '\0') {
        strncpy(g_sintran_root, path, MAX_ROOT_PATH - 1);
        g_sintran_root[MAX_ROOT_PATH - 1] = '\0';

        /* Remove trailing slash/backslash if present */
        size_t len = strlen(g_sintran_root);
        if (len > 1 && (g_sintran_root[len - 1] == '/' ||
                        g_sintran_root[len - 1] == '\\')) {
            g_sintran_root[len - 1] = '\0';
        }
    } else {
        strcpy(g_sintran_root, DEFAULT_SINTRAN_ROOT);
    }
}

/* ============================================================
 * Current User
 * ============================================================ */

const char* mon_config_get_current_user(void) {
    return g_current_user;
}

void mon_config_set_current_user(const char* user) {
    if (user && user[0] != '\0') {
        strncpy(g_current_user, user, MAX_USER_NAME - 1);
        g_current_user[MAX_USER_NAME - 1] = '\0';

        /* Convert to uppercase */
        for (char* p = g_current_user; *p; p++) {
            if (*p >= 'a' && *p <= 'z') {
                *p = *p - 'a' + 'A';
            }
        }
    } else {
        strcpy(g_current_user, DEFAULT_CURRENT_USER);
    }
}

/* ============================================================
 * Auto Scratch File 64
 * ============================================================ */

int mon_config_get_auto_scratch_64(void) {
    return g_auto_scratch_64;
}

void mon_config_set_auto_scratch_64(int enabled) {
    g_auto_scratch_64 = enabled ? 1 : 0;
}

/* ============================================================
 * Reset
 * ============================================================ */

void mon_config_reset(void) {
    strcpy(g_sintran_root, DEFAULT_SINTRAN_ROOT);
    strcpy(g_current_user, DEFAULT_CURRENT_USER);
    g_auto_scratch_64 = DEFAULT_AUTO_SCRATCH_64;
}
