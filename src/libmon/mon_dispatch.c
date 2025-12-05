/*
 * MON Call Dispatcher
 *
 * Handles MON call registration and dispatch.
 */

#include "mon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * REGISTRY STORAGE
 *
 * Simple hash table for O(1) lookup by MON number.
 * MON numbers range from 0 to ~500 (octal 0 to ~764B).
 * ========================================================================= */

#define MON_REGISTRY_SIZE 512

static MonRegistryEntry* g_registry[MON_REGISTRY_SIZE];
static int g_registry_count = 0;

/* Behavior settings */
static MonUnimplBehavior g_unimpl_behavior = MON_UNIMPL_BREAK;  /* Default: break on unimpl */
static MonUnimplBehavior g_inprogress_behavior = MON_UNIMPL_CONTINUE;  /* Default: continue on in-progress */

/* =========================================================================
 * INITIALIZATION
 * ========================================================================= */

void mon_init(void) {
    /* Clear registry */
    memset(g_registry, 0, sizeof(g_registry));
    g_registry_count = 0;

    /* Register all handlers from generated code */
    mon_register_all_handlers();

    mon_log(MON_LOG_INFO, "MON subsystem initialized with %d handlers", g_registry_count);
}

void mon_shutdown(void) {
    /* Free registry entries */
    for (int i = 0; i < MON_REGISTRY_SIZE; i++) {
        if (g_registry[i]) {
            free(g_registry[i]);
            g_registry[i] = NULL;
        }
    }
    g_registry_count = 0;
}

/* =========================================================================
 * HANDLER REGISTRATION
 * ========================================================================= */

void mon_register(
    uint32_t mon_number,
    const char* octal_str,
    const char* name,
    const char* long_name,
    const char* description,
    MonHandler handler,
    MonImplStatus status,
    uint8_t param_count)
{
    if (mon_number >= MON_REGISTRY_SIZE) {
        mon_log(MON_LOG_ERROR, "MON %u (%s) exceeds registry size", mon_number, octal_str);
        return;
    }

    /* Allocate entry */
    MonRegistryEntry* entry = (MonRegistryEntry*)malloc(sizeof(MonRegistryEntry));
    if (!entry) {
        mon_log(MON_LOG_ERROR, "Failed to allocate registry entry for MON %u", mon_number);
        return;
    }

    entry->mon_number = mon_number;
    entry->octal_str = octal_str;
    entry->name = name;
    entry->long_name = long_name;
    entry->description = description;
    entry->handler = handler;
    entry->status = status;
    entry->param_count = param_count;
    entry->nd100_compat = 1;  /* Default to compatible */
    entry->nd500_compat = 1;

    /* Free existing entry if any */
    if (g_registry[mon_number]) {
        free(g_registry[mon_number]);
    }

    g_registry[mon_number] = entry;
    g_registry_count++;
}

/* =========================================================================
 * MON CALL DISPATCH
 * ========================================================================= */

MonResult mon_dispatch(MonContext* ctx) {
    if (!ctx) {
        return MON_ERROR;
    }

    /* Lookup handler */
    MonRegistryEntry* entry = NULL;
    if (ctx->mon_number < MON_REGISTRY_SIZE) {
        entry = g_registry[ctx->mon_number];
    }

    /* Convert MON number to octal for logging */
    char octal[16];
    snprintf(octal, sizeof(octal), "%oB", ctx->mon_number);

    /* =====================================================================
     * HANDLE UNIMPLEMENTED MON CALLS
     * ===================================================================== */

    if (!entry || entry->status == MON_STATUS_NOT_IMPLEMENTED) {
        mon_log(MON_LOG_ERROR,
                "=== UNIMPLEMENTED MON CALL ===");
        mon_log(MON_LOG_ERROR,
                "  MON %s (%s) called",
                octal,
                entry ? entry->name : "UNKNOWN");
        mon_log(MON_LOG_ERROR,
                "  Return PC: 0x%08X",
                ctx->return_address);
        mon_log(MON_LOG_ERROR,
                "  Arguments: %u",
                ctx->arg_count);

        /* Log argument addresses at DEBUG level */
        if (mon_log_get_level() >= MON_LOG_DEBUG) {
            for (uint32_t i = 0; i < ctx->arg_count && i < 8; i++) {
                mon_log(MON_LOG_DEBUG,
                        "    Arg[%u] @ 0x%08X",
                        i, ctx->arg_addresses[i]);
            }
        }

        mon_log(MON_LOG_ERROR,
                "  Please implement this MON call!");

        /* Apply unimplemented behavior */
        switch (g_unimpl_behavior) {
            case MON_UNIMPL_BREAK:
                ctx->break_requested = 1;
                ctx->halt_reason = "Unimplemented MON call";
                break;
            case MON_UNIMPL_HALT:
                ctx->halt_requested = 1;
                ctx->halt_reason = "Unimplemented MON call";
                break;
            case MON_UNIMPL_CONTINUE:
            default:
                /* Just continue with error */
                break;
        }

        /* Set K flag to indicate error */
        if (ctx->set_k_flag) {
            ctx->set_k_flag(ctx->cpu, 1);
        }

        return MON_ERROR;
    }

    /* =====================================================================
     * HANDLE IN-PROGRESS MON CALLS
     * ===================================================================== */

    if (entry->status == MON_STATUS_IN_PROGRESS) {
        mon_log(MON_LOG_WARN,
                "=== IN-PROGRESS MON CALL ===");
        mon_log(MON_LOG_WARN,
                "  MON %s %s - implementation incomplete",
                octal, entry->name);

        /* Apply in-progress behavior */
        if (g_inprogress_behavior == MON_UNIMPL_BREAK) {
            ctx->break_requested = 1;
        } else if (g_inprogress_behavior == MON_UNIMPL_HALT) {
            ctx->halt_requested = 1;
            ctx->halt_reason = "In-progress MON call";
        }
        /* Continue to execute the handler anyway */
    }

    /* =====================================================================
     * LOG ENTRY
     * ===================================================================== */

    mon_log_entry(ctx, entry->name);

    /* =====================================================================
     * CALL THE HANDLER
     * ===================================================================== */

    MonResult result = MON_ERROR;

    if (entry->handler) {
        result = entry->handler(ctx);
    } else {
        mon_log(MON_LOG_ERROR, "MON %s has no handler function", octal);
        ctx->break_requested = 1;
    }

    /* =====================================================================
     * LOG EXIT
     * ===================================================================== */

    mon_log_exit(ctx, entry->name, result);

    return result;
}

/* =========================================================================
 * BEHAVIOR CONFIGURATION
 * ========================================================================= */

void mon_set_unimpl_behavior(MonUnimplBehavior behavior) {
    g_unimpl_behavior = behavior;
}

MonUnimplBehavior mon_get_unimpl_behavior(void) {
    return g_unimpl_behavior;
}

void mon_set_inprogress_behavior(MonUnimplBehavior behavior) {
    g_inprogress_behavior = behavior;
}

MonUnimplBehavior mon_get_inprogress_behavior(void) {
    return g_inprogress_behavior;
}

/* =========================================================================
 * INFORMATION QUERIES
 * ========================================================================= */

const char* mon_get_name(uint32_t mon_number) {
    if (mon_number < MON_REGISTRY_SIZE && g_registry[mon_number]) {
        return g_registry[mon_number]->name;
    }
    return NULL;
}

const char* mon_get_long_name(uint32_t mon_number) {
    if (mon_number < MON_REGISTRY_SIZE && g_registry[mon_number]) {
        return g_registry[mon_number]->long_name;
    }
    return NULL;
}

const char* mon_get_octal(uint32_t mon_number) {
    if (mon_number < MON_REGISTRY_SIZE && g_registry[mon_number]) {
        return g_registry[mon_number]->octal_str;
    }
    return NULL;
}

MonImplStatus mon_get_status(uint32_t mon_number) {
    if (mon_number < MON_REGISTRY_SIZE && g_registry[mon_number]) {
        return g_registry[mon_number]->status;
    }
    return MON_STATUS_NOT_IMPLEMENTED;
}

int mon_is_implemented(uint32_t mon_number) {
    MonImplStatus status = mon_get_status(mon_number);
    return status >= MON_STATUS_IN_PROGRESS;
}

int mon_is_validated(uint32_t mon_number) {
    return mon_get_status(mon_number) == MON_STATUS_VALIDATED;
}

const MonRegistryEntry* mon_get_entry(uint32_t mon_number) {
    if (mon_number < MON_REGISTRY_SIZE) {
        return g_registry[mon_number];
    }
    return NULL;
}

/* =========================================================================
 * STATUS ENUMERATION
 * ========================================================================= */

int mon_count_by_status(MonImplStatus status) {
    int count = 0;
    for (int i = 0; i < MON_REGISTRY_SIZE; i++) {
        if (g_registry[i] && g_registry[i]->status == status) {
            count++;
        }
    }
    return count;
}

int mon_get_total_count(void) {
    return g_registry_count;
}

void mon_enumerate_by_status(MonImplStatus status, MonEnumCallback callback, void* user_data) {
    if (!callback) return;

    for (int i = 0; i < MON_REGISTRY_SIZE; i++) {
        if (g_registry[i] && g_registry[i]->status == status) {
            callback(g_registry[i], user_data);
        }
    }
}

void mon_list_by_status(MonImplStatus status) {
    const char* status_name;
    switch (status) {
        case MON_STATUS_NOT_IMPLEMENTED: status_name = "NOT_IMPLEMENTED"; break;
        case MON_STATUS_IN_PROGRESS:     status_name = "IN_PROGRESS"; break;
        case MON_STATUS_VALIDATED:       status_name = "VALIDATED"; break;
        default:                         status_name = "UNKNOWN"; break;
    }

    int count = mon_count_by_status(status);
    mon_log(MON_LOG_INFO, "%s MON calls (%d):", status_name, count);

    for (int i = 0; i < MON_REGISTRY_SIZE; i++) {
        if (g_registry[i] && g_registry[i]->status == status) {
            mon_log(MON_LOG_INFO, "  %s %-12s %s",
                    g_registry[i]->octal_str,
                    g_registry[i]->name,
                    g_registry[i]->long_name ? g_registry[i]->long_name : "");
        }
    }
}
