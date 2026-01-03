/*
 * MON 221B [CRALF/CreateFile]
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
#include "../mon_log.h"
#include "../mon_file_table.h"
#include "../mon_path.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>  /* unlink */

MonResult mon_221B_CreateFile(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 1) {
        mon_log(MON_LOG_WARN, MON_ID_221B ": Missing parameters (need at least 1, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Read filename string from descriptor [Length:4][Pointer:4]
     * High-level languages (FORTRAN/Pascal) pass string descriptors */
    char filename[65];
    mon_read_descriptor_string(ctx, 0, filename, 65);

    /* Optional parameters */
    uint32_t start_address = (ctx->arg_count > 1) ? mon_read_param_word(ctx, 1) : 0;
    uint32_t num_pages = (ctx->arg_count > 2) ? mon_read_param_word(ctx, 2) : 1;

    mon_log(MON_LOG_DEBUG, MON_ID_221B ": IN: FileName='%s', StartAddr=%o, NoOfPages=%o",
            filename, start_address, num_pages);

    /* Validate filename */
    if (filename[0] == '\0') {
        mon_log(MON_LOG_WARN, MON_ID_221B ": Empty filename");
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Parse filename to check if it's a scratch file */
    char parsed_name[32] = {0};
    mon_parse_sintran_name(filename, NULL, 0, parsed_name, sizeof(parsed_name), NULL, 0);
    int is_scratch_file = (strncmp(parsed_name, "SCRATCH-", 8) == 0);

    /* Translate SINTRAN path to host path
     * mon_translate_path handles:
     * - SCRATCH-NNNNN files auto-routed to SCRATCH directory
     * - Parent directory auto-creation
     * - Extension from filename (NAME:EXT format)
     */
    char host_path[256];
    if (mon_translate_path(filename, NULL, host_path, sizeof(host_path)) != 0) {
        /* Fallback to simple path construction */
        mon_build_host_path(filename, host_path, sizeof(host_path));
    }

    /* Check if file already exists */
    FILE* test_fp = fopen(host_path, "rb");
    if (test_fp) {
        fclose(test_fp);

        /* For scratch files, delete and recreate (they are temporary) */
        if (is_scratch_file) {
            mon_log(MON_LOG_DEBUG, MON_ID_221B ": Deleting existing scratch file '%s'", host_path);
            unlink(host_path);
        } else {
            mon_log(MON_LOG_WARN, MON_ID_221B ": File '%s' already exists", host_path);
            mon_set_error(ctx, 58);  /* File already exists */
            return MON_ERROR;
        }
    }

    /* Create empty file */
    FILE* fp = fopen(host_path, "wb");
    if (!fp) {
        mon_log(MON_LOG_WARN, MON_ID_221B ": Failed to create file '%s'", host_path);
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

    mon_log(MON_LOG_DEBUG, MON_ID_221B ": OUT: Created file '%s' (%o pages)",
            host_path, num_pages);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
