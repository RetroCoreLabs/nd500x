/*
 * MON 221B (145 decimal): CreateFile (CRALF)
 *
 * Creates a file. The file may be indexed, contiguous, or allocated.
 * Most files are indexed. The size of indexed files expands automatically
 * when written to.
 *
 * - You need directory access to the user who owns the file.
 * - An indexed file not yet written to may be converted to a contiguous file.
 *
 * Parameters:
 *   [I] FileName (STRING): File name (including optional type after dot)
 *   [I] StartAddress (INTEGER): Start address (for contiguous files)
 *   [I] NoOfPages (INTEGER): Initial size in pages (1 page = 2048 bytes)
 *
 * Returns:
 *   K flag set on error, error code in W1:
 *     52 = Invalid parameter
 *     58 = File already exists
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_file_table.h"
#include <stdio.h>
#include <string.h>

MonResult mon_221B_CreateFile(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 1) {
        mon_log(MON_LOG_WARN, "MON 221B CRALF: Missing parameters (need at least 1, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Read filename string */
    char filename[65];
    mon_read_string(ctx, 0, filename, 65);

    /* Optional parameters */
    uint32_t start_address = (ctx->arg_count > 1) ? mon_read_param_word(ctx, 1) : 0;
    uint32_t num_pages = (ctx->arg_count > 2) ? mon_read_param_word(ctx, 2) : 1;

    mon_log(MON_LOG_DEBUG, "MON 221B CRALF: FileName='%s', StartAddr=%u, NoOfPages=%u",
            filename, start_address, num_pages);

    /* Validate filename */
    if (filename[0] == '\0') {
        mon_log(MON_LOG_WARN, "MON 221B CRALF: Empty filename");
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Build host path using shared utility */
    char host_path[256];
    mon_build_host_path(filename, host_path, sizeof(host_path));

    /* Check if file already exists */
    FILE* test_fp = fopen(host_path, "rb");
    if (test_fp) {
        fclose(test_fp);
        mon_log(MON_LOG_WARN, "MON 221B CRALF: File '%s' already exists", host_path);
        mon_set_error(ctx, 58);  /* File already exists */
        return MON_ERROR;
    }

    /* Create empty file */
    FILE* fp = fopen(host_path, "wb");
    if (!fp) {
        mon_log(MON_LOG_WARN, "MON 221B CRALF: Failed to create file '%s'", host_path);
        mon_set_error(ctx, 52);  /* Invalid parameter (could be permission denied) */
        return MON_ERROR;
    }

    /* If num_pages > 0, pre-allocate space (1 page = 2048 bytes) */
    if (num_pages > 0) {
        uint32_t initial_size = num_pages * SINTRAN_PAGE_SIZE;
        /* Seek to end and write a byte to allocate space */
        if (fseek(fp, initial_size - 1, SEEK_SET) == 0) {
            fputc(0, fp);
        }
    }

    fclose(fp);

    mon_log(MON_LOG_DEBUG, "MON 221B CRALF: Created file '%s' (%u pages)",
            host_path, num_pages);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
