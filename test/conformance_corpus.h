/*
 * conformance_corpus.h - load the ND500 Conformance Corpus for a test program
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * Shared by test_conformance (runs every vector) and test_corpus_coverage
 * (measures what the vectors cover), so both read the same file the same way.
 */

#ifndef CONFORMANCE_CORPUS_H
#define CONFORMANCE_CORPUS_H

#include <cjson/cJSON.h>

/**
 * @brief Find, read and parse the conformance corpus.
 *
 * The file is tried as given first (relative to the current directory) and,
 * if it is not readable there, in the directory of the running executable:
 * CMake copies nd500-conformance.json next to the test binaries. Prints
 * "Loading <path>..." and "Loaded <n> bytes" to stdout, and the reason for a
 * failure to stderr.
 *
 * @param argv0      argv[0] of the test program, used to find its directory.
 * @param json_path  File name or path of the corpus.
 * @return The parsed root array, which the caller frees with cJSON_Delete(),
 *         or NULL if the file cannot be read, is not valid JSON, or its root
 *         is not an array.
 */
cJSON *conformance_corpus_load(const char *argv0, const char *json_path);

#endif /* CONFORMANCE_CORPUS_H */
