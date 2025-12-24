/*
 * MON 256B [DEABF/FullFileName]
 *
 * Returns a complete file name from an abbreviated one. The directory, the user,
 * the file name, the file type, and the version are returned.
 *
 * Parameters:
 *   [I] AbbrevFileName (STRING): Abbreviated file name
 *   [O] FileName (STRING): Full file name output
 *   [I] FileType (STRING): Default file type
 *
 * Note: Simplified implementation - just copies input to output with defaults.
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_log.h"
#include <stdio.h>
#include <string.h>

MonResult mon_256B_FullFileName(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 3) {
        mon_log(MON_LOG_WARN, MON_ID_256B ": Missing parameters (need 3, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Read input strings */
    char abbrev_name[65];
    char file_type[5];
    mon_read_string(ctx, 0, abbrev_name, sizeof(abbrev_name));
    mon_read_string(ctx, 2, file_type, sizeof(file_type));

    uint32_t output_addr = ctx->arg_addresses[1];

    mon_log(MON_LOG_DEBUG, MON_ID_256B ": IN: AbbrevName='%s', FileType='%s'",
            abbrev_name, file_type);

    /* Build full filename
     * Format: (PACK-ONE:RT)USER:FILENAME:TYPE;VERSION
     * Simplified: just use filename.type format
     */
    char full_name[128];
    if (strchr(abbrev_name, '.') || strchr(abbrev_name, ':')) {
        /* Already has type or directory - use as-is */
        strncpy(full_name, abbrev_name, sizeof(full_name) - 1);
    } else if (file_type[0] != '\0') {
        /* Add default type */
        snprintf(full_name, sizeof(full_name), "%s.%s", abbrev_name, file_type);
    } else {
        /* No type - use as-is */
        strncpy(full_name, abbrev_name, sizeof(full_name) - 1);
    }
    full_name[sizeof(full_name) - 1] = '\0';

    /* Write full name to output with SINTRAN string terminator */
    size_t len = strlen(full_name);
    for (size_t i = 0; i < len; i++) {
        ctx->write_byte(ctx->cpu, output_addr + (uint32_t)i, (uint8_t)full_name[i]);
    }
    /* Add SINTRAN string terminator (0x27 = apostrophe) */
    ctx->write_byte(ctx->cpu, output_addr + (uint32_t)len, 0x27);

    mon_log(MON_LOG_DEBUG, MON_ID_256B ": OUT: FullName='%s'", full_name);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
