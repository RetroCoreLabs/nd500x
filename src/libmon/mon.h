/*
 * libmon - SINTRAN III Monitor Call Emulation Library
 *
 * Public API for MON call handling in ND-500 emulators.
 *
 * Usage:
 *   1. Call mon_init() at startup
 *   2. Set up memory access callbacks in MonContext
 *   3. Call mon_dispatch() when CALL/CALLG targets segment 31
 *   4. Check ctx->halt_requested and ctx->break_requested after dispatch
 *
 * Reference: ND-860228.2 EN (SINTRAN III Monitor Calls)
 */

#ifndef MON_H
#define MON_H

#include "mon_types.h"
#include "mon_log.h"

/* =========================================================================
 * INITIALIZATION
 * ========================================================================= */

/* Initialize the MON subsystem. Call once at startup. */
void mon_init(void);

/* Shutdown and cleanup */
void mon_shutdown(void);

/* =========================================================================
 * MON CALL DISPATCH
 *
 * Main entry point - called when CALL/CALLG targets segment 31.
 * ========================================================================= */

/**
 * Dispatch a MON call to its handler.
 *
 * @param ctx  Fully populated MonContext with:
 *             - cpu/machine pointers
 *             - mon_number (from address offset)
 *             - arg_count and arg_addresses (from CALL/CALLG)
 *             - Memory access callbacks
 *
 * @return     MON_SUCCESS or MON_ERROR
 *
 * After call, check:
 *   - ctx->halt_requested: CPU should halt (LEAVE or unimplemented)
 *   - ctx->break_requested: Should break into debugger
 *   - ctx->halt_reason: Human-readable reason for halt
 */
MonResult mon_dispatch(MonContext* ctx);

/* =========================================================================
 * MON CALL INFORMATION
 * ========================================================================= */

/* Get MON call name by number (e.g., 9 -> "TIME") */
const char* mon_get_name(uint32_t mon_number);

/* Get MON call long name (e.g., 9 -> "GetBasicTime") */
const char* mon_get_long_name(uint32_t mon_number);

/* Get MON call octal string (e.g., 9 -> "11B") */
const char* mon_get_octal(uint32_t mon_number);

/* Get implementation status */
MonImplStatus mon_get_status(uint32_t mon_number);

/* Check if MON call is implemented (status >= IN_PROGRESS) */
int mon_is_implemented(uint32_t mon_number);

/* Check if MON call is validated (status == VALIDATED) */
int mon_is_validated(uint32_t mon_number);

/* Get registry entry for a MON call (NULL if not found) */
const MonRegistryEntry* mon_get_entry(uint32_t mon_number);

/* Get registry entry by short name or long name (case-insensitive, NULL if not found) */
const MonRegistryEntry* mon_get_entry_by_name(const char* name);

/* =========================================================================
 * IMPLEMENTATION STATUS QUERIES
 * ========================================================================= */

/* Count MON calls by status */
int mon_count_by_status(MonImplStatus status);

/* Get total number of registered MON calls */
int mon_get_total_count(void);

/* Iterate over MON calls by status (callback receives each entry) */
typedef void (*MonEnumCallback)(const MonRegistryEntry* entry, void* user_data);
void mon_enumerate_by_status(MonImplStatus status, MonEnumCallback callback, void* user_data);

/* Print list of MON calls by status to log */
void mon_list_by_status(MonImplStatus status);

/* =========================================================================
 * UNIMPLEMENTED MON BEHAVIOR
 * ========================================================================= */

/* Set behavior when unimplemented MON is called */
void mon_set_unimpl_behavior(MonUnimplBehavior behavior);
MonUnimplBehavior mon_get_unimpl_behavior(void);

/* Set behavior when in-progress MON is called (separate from unimplemented) */
void mon_set_inprogress_behavior(MonUnimplBehavior behavior);
MonUnimplBehavior mon_get_inprogress_behavior(void);

/* =========================================================================
 * PARAMETER ACCESS HELPERS
 *
 * Use these from within MON handlers to read/write parameters.
 * ========================================================================= */

/* Read a 32-bit word from parameter at index */
uint32_t mon_read_param_word(MonContext* ctx, int idx);

/* Read a 64-bit double-word from parameter at index */
uint64_t mon_read_param_dword(MonContext* ctx, int idx);

/* Read a byte from parameter at index */
uint8_t mon_read_param_byte(MonContext* ctx, int idx);

/* Read a 16-bit halfword from parameter at index */
uint16_t mon_read_param_halfword(MonContext* ctx, int idx);

/* Write a 32-bit word to parameter at index */
void mon_write_param_word(MonContext* ctx, int idx, uint32_t value);

/* Write a 16-bit halfword to parameter at index */
void mon_write_param_halfword(MonContext* ctx, int idx, uint16_t value);

/* Write a 64-bit double-word to parameter at index */
void mon_write_param_dword(MonContext* ctx, int idx, uint64_t value);

/* Write a byte to parameter at index */
void mon_write_param_byte(MonContext* ctx, int idx, uint8_t value);

/* =========================================================================
 * STRING HELPERS
 * ========================================================================= */

/**
 * Read a SINTRAN string from parameter (direct address, 0x27 terminated).
 *
 * Use this when the parameter IS the string (not a descriptor).
 * String is terminated by 0x00, 0x27 ('), or 0xFF.
 *
 * @param ctx    MON context
 * @param idx    Parameter index
 * @param buf    Output buffer
 * @param max    Maximum bytes to read (including null terminator)
 * @return       Actual string length (not including null)
 */
int mon_read_sintran_string(MonContext* ctx, int idx, char* buf, int max);

/**
 * Read a string from a descriptor [Length:4][Pointer:4] format.
 *
 * Used by FORTRAN-500 and Pascal compilers. The parameter points to
 * an 8-byte descriptor containing the string length and pointer.
 * String is terminated by 0x00, 0x27 ('), 0xFF, or the length limit.
 *
 * @param ctx    MON context
 * @param idx    Parameter index
 * @param buf    Output buffer
 * @param max    Maximum bytes to read (including null terminator)
 * @return       Actual string length (not including null), or -1 on error
 */
int mon_read_descriptor_string(MonContext* ctx, int idx, char* buf, int max);

/**
 * Read a SINTRAN string from parameter (auto-detect format).
 *
 * DEPRECATED: Use mon_read_sintran_string or mon_read_descriptor_string.
 * This function attempts to auto-detect descriptor vs direct format,
 * which can fail with certain memory patterns.
 *
 * @param ctx    MON context
 * @param idx    Parameter index
 * @param buf    Output buffer
 * @param max    Maximum bytes to read (including null terminator)
 * @return       Actual string length (not including null)
 */
int mon_read_string(MonContext* ctx, int idx, char* buf, int max);

/**
 * Write a string to parameter address.
 *
 * @param ctx    MON context
 * @param idx    Parameter index
 * @param str    String to write
 * @return       Bytes written
 */
int mon_write_string(MonContext* ctx, int idx, const char* str);

/* =========================================================================
 * ERROR HANDLING HELPERS
 * ========================================================================= */

/* Set K flag and error code in I1 register */
void mon_set_error(MonContext* ctx, int32_t error_code);

/* Set K=0 (success) */
void mon_set_success(MonContext* ctx);

/* Request halt (used by MON 0 LEAVE) */
void mon_request_halt(MonContext* ctx, const char* reason);

/* =========================================================================
 * HANDLER REGISTRATION (for generated handlers)
 * ========================================================================= */

/**
 * Register a MON handler.
 *
 * @param mon_number   MON number (decimal)
 * @param octal_str    Octal string (e.g., "11B")
 * @param name         Short name (e.g., "TIME")
 * @param long_name    Long name (e.g., "GetBasicTime")
 * @param description  Description
 * @param handler      Handler function
 * @param status       Implementation status
 * @param param_count  Expected parameter count
 */
void mon_register(
    uint32_t mon_number,
    const char* octal_str,
    const char* name,
    const char* long_name,
    const char* description,
    MonHandler handler,
    MonImplStatus status,
    uint8_t param_count
);

/**
 * Register a MON handler with parameter description.
 *
 * Same as mon_register but includes params_desc for detailed parameter info.
 *
 * @param params_desc  Parameter description (e.g., "[O] Result (INTEGER): ...")
 */
void mon_register_ex(
    uint32_t mon_number,
    const char* octal_str,
    const char* name,
    const char* long_name,
    const char* description,
    const char* params_desc,
    MonHandler handler,
    MonImplStatus status,
    uint8_t param_count
);

/* Register all handlers (called by mon_init, defined in mon_registry.c) */
void mon_register_all_handlers(void);

#endif /* MON_H */
