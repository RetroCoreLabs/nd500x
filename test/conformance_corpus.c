/*
 * conformance_corpus.c - load the ND500 Conformance Corpus for a test program
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "conformance_corpus.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>  /* dirname() */
#include <unistd.h>  /* access() */

/* Read a whole file into a NUL-terminated heap buffer. */
static char *load_file(const char *path, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        fprintf(stderr, "Failed to open file: %s\n", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *data = (char *)malloc((size_t)size + 1);
    if (!data)
    {
        fclose(f);
        return NULL;
    }

    size_t read = fread(data, 1, (size_t)size, f);
    fclose(f);

    data[read] = '\0';
    if (out_size)
    {
        *out_size = read;
    }
    return data;
}

cJSON *conformance_corpus_load(const char *argv0, const char *json_path)
{
    /* Try the path as given first, then the executable's directory. */
    char resolved_path[4096];
    snprintf(resolved_path, sizeof resolved_path, "%s", json_path);

    if (access(resolved_path, R_OK) != 0)
    {
        char exe_path[4096];
        snprintf(exe_path, sizeof exe_path, "%s", argv0);
        const char *dir = dirname(exe_path);
        snprintf(resolved_path, sizeof resolved_path, "%s/%s", dir, json_path);
    }

    printf("Loading %s...\n", resolved_path);
    size_t json_size;
    char *json_data = load_file(resolved_path, &json_size);
    if (!json_data)
    {
        return NULL;
    }
    printf("Loaded %zu bytes\n", json_size);

    cJSON *root = cJSON_Parse(json_data);
    free(json_data);

    if (!root)
    {
        const char *error = cJSON_GetErrorPtr();
        fprintf(stderr, "JSON parse error: %s\n", error ? error : "unknown");
        return NULL;
    }

    if (!cJSON_IsArray(root))
    {
        fprintf(stderr, "JSON root is not an array\n");
        cJSON_Delete(root);
        return NULL;
    }
    return root;
}
