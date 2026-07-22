/*
 * SINTRAN III Configuration Module
 *
 * Stores configuration for SINTRAN file system emulation.
 *
 * Reference: SINTRAN III System Supervisor (ND-830003)
 */

#ifndef MON_CONFIG_H
#define MON_CONFIG_H

/**
 * Get SINTRAN root directory.
 *
 * All SINTRAN file operations are relative to this directory.
 * Default: "." (current working directory)
 *
 * Returns: Root directory path (never NULL)
 */
const char* mon_config_get_sintran_root(void);

/**
 * Set SINTRAN root directory.
 *
 * Parameters:
 *   path - Root directory path (NULL resets to default)
 */
void mon_config_set_sintran_root(const char* path);

/**
 * Get current SINTRAN user.
 *
 * Used when filename doesn't specify a user, e.g., "FILE:DATA"
 * maps to "{root}/{current_user}/FILE.DATA"
 *
 * Default: "GUEST"
 *
 * Returns: Current user name (never NULL)
 */
const char* mon_config_get_current_user(void);

/**
 * Set current SINTRAN user.
 *
 * Parameters:
 *   user - User name (NULL resets to default)
 */
void mon_config_set_current_user(const char* user);

/**
 * Get automatic scratch file 64 setting.
 *
 * When enabled, scratch file 64 is automatically opened
 * when the MON subsystem initializes.
 *
 * Default: 1 (enabled)
 *
 * Returns: 1 if enabled, 0 if disabled
 */
int mon_config_get_auto_scratch_64(void);

/**
 * Set automatic scratch file 64 setting.
 *
 * Parameters:
 *   enabled - 1 to enable, 0 to disable
 */
void mon_config_set_auto_scratch_64(int enabled);

/**
 * Reset all configuration to defaults.
 */
void mon_config_reset(void);

#endif /* MON_CONFIG_H */
