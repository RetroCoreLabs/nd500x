#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <errno.h>

/* Platform-specific directory creation */
#ifdef _WIN32
#include <direct.h>
#define mkdir_p(path) _mkdir(path)
#else
#include <sys/types.h>
#define mkdir_p(path) mkdir(path, 0755)
#endif

/* Instruction metadata extracted from JSON */
typedef struct {
    uint16_t opcode;
    char mnemonic[64];
    char functionName[64];
    char class[32];
    uint8_t operandCount;
    uint8_t prefixMask;
    uint8_t variant;
    uint32_t op_templates[4];
} InstructionMeta;

/* Helper: Create directory and all parent directories */
static int mkdirp(const char* path) {
    char tmp[512];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/' || tmp[len - 1] == '\\')
        tmp[len - 1] = 0;

    for (p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = 0;
            mkdir_p(tmp);
            *p = '/';
        }
    }
    return mkdir_p(tmp);
}

/* Helper: Check if file exists */
static int file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

/* Helper: Extract string value from JSON field */
static char* extract_string(const char* json, const char* field, const char* next_instr) {
    char* fld = strstr(json, field);
    if (!fld || (next_instr && fld > next_instr)) return NULL;

    char* colon = strchr(fld, ':');
    if (!colon) return NULL;

    char* start = strchr(colon, '"');
    if (!start) return NULL;

    char* end = strchr(start + 1, '"');
    if (!end) return NULL;

    size_t len = end - start - 1;
    char* result = (char*)malloc(len + 1);
    memcpy(result, start + 1, len);
    result[len] = '\0';
    return result;
}

/* Helper: Extract integer value from JSON field */
static int extract_int(const char* json, const char* field, const char* next_instr) {
    char* fld = strstr(json, field);
    if (!fld || (next_instr && fld > next_instr)) return 0;

    char* colon = strchr(fld, ':');
    if (!colon) return 0;

    return atoi(colon + 1);
}

/* Parse instructions.json and extract metadata */
static int parse_instructions(const char* json_path, InstructionMeta** out_instrs, int* out_count) {
    FILE* f = fopen(json_path, "rb");
    if (!f) {
        perror("open json");
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = (char*)malloc(sz + 1);
    if (!buf) {
        fclose(f);
        return -1;
    }

    if (fread(buf, 1, sz, f) != (size_t)sz) {
        fclose(f);
        free(buf);
        return -1;
    }
    buf[sz] = '\0';
    fclose(f);

    /* Allocate array for instructions */
    const int max_instrs = 1200;
    InstructionMeta* instrs = (InstructionMeta*)calloc(max_instrs, sizeof(InstructionMeta));
    if (!instrs) {
        free(buf);
        return -1;
    }

    int count = 0;
    char* p = buf;

    while ((p = strstr(p, "\"opcode\"")) && count < max_instrs) {
        char* next_instr = strstr(p + 1, "\"opcode\"");

        /* Extract opcode */
        char* opc_str = extract_string(p, "\"opcode\"", next_instr);
        if (opc_str) {
            instrs[count].opcode = (uint16_t)strtol(opc_str, NULL, 0);
            free(opc_str);
        }

        /* Extract mnemonic */
        char* mnem_str = extract_string(p, "\"mnemonic\"", next_instr);
        if (mnem_str) {
            snprintf(instrs[count].mnemonic, sizeof(instrs[count].mnemonic), "%s", mnem_str);
            free(mnem_str);
        }

        /* Extract functionName */
        char* func_str = extract_string(p, "\"functionName\"", next_instr);
        if (func_str) {
            snprintf(instrs[count].functionName, sizeof(instrs[count].functionName), "%s", func_str);
            free(func_str);
        }

        /* Extract class */
        char* class_str = extract_string(p, "\"class\"", next_instr);
        if (class_str) {
            snprintf(instrs[count].class, sizeof(instrs[count].class), "%s", class_str);
            free(class_str);
        }

        /* Extract operandCount, prefixMask, variant */
        instrs[count].operandCount = (uint8_t)extract_int(p, "\"operandCount\"", next_instr);

        char* pmask_str = extract_string(p, "\"prefixMask\"", next_instr);
        if (pmask_str) {
            instrs[count].prefixMask = (uint8_t)strtol(pmask_str, NULL, 0);
            free(pmask_str);
        }

        instrs[count].variant = (uint8_t)extract_int(p, "\"variantNumber\"", next_instr);

        /* Extract operandTemplates array */
        char* tmpl = strstr(p, "\"operandTemplates\"");
        if (tmpl && (!next_instr || tmpl < next_instr)) {
            char* bracket = strchr(tmpl, '[');
            if (bracket) {
                char* close = strchr(bracket, ']');
                if (close) {
                    char* t = bracket + 1;
                    for (int ti = 0; ti < 4; ti++) {
                        while (*t && (*t == ' ' || *t == '\t' || *t == ',' || *t == '"')) t++;
                        if (t >= close) break;
                        if (*t == '0' && (t[1] == 'x' || t[1] == 'X')) {
                            instrs[count].op_templates[ti] = (uint32_t)strtoul(t, &t, 0);
                        } else break;
                    }
                }
            }
        }

        count++;
        p = next_instr ? next_instr : (p + strlen(p));
    }

    free(buf);
    *out_instrs = instrs;
    *out_count = count;

    fprintf(stderr, "[GEN] Parsed %d instructions from %s\n", count, json_path);
    return 0;
}

/* Generate stub .c file for an instruction function */
static void generate_stub_file(const char* class_name, const char* func_name,
                                 InstructionMeta* variants, int variant_count) {
    char dir_path[512];
    char file_path[512];

    /* Create directory: src/cpu/instructions/<CLASS>/ */
    snprintf(dir_path, sizeof(dir_path), "src/cpu/instructions/%s", class_name);
    mkdirp(dir_path);

    /* Create file path */
    snprintf(file_path, sizeof(file_path), "%s/%s.c", dir_path, func_name);

    /* Check if file already exists - NEVER overwrite */
    if (file_exists(file_path)) {
        fprintf(stderr, "[GEN] Skipping existing file: %s\n", file_path);
        return;
    }

    FILE* f = fopen(file_path, "w");
    if (!f) {
        fprintf(stderr, "[GEN] Warning: Could not create %s: %s\n", file_path, strerror(errno));
        return;
    }

    /* Write file header
     * Note: Stubs are generated in build directory (${CMAKE_BINARY_DIR}/src/cpu/instructions/)
     * but need to include headers from source directory
     */
    fprintf(f, "#include \"cpu_protos.h\"\n");
    fprintf(f, "#include \"machine_protos.h\"\n");
    fprintf(f, "#include <stdio.h>\n\n");

    /* Write documentation comment */
    fprintf(f, "/**\n");
    fprintf(f, " * %s instruction - %s class\n", func_name, class_name);
    fprintf(f, " * \n");

    /* List all variants */
    if (variant_count == 1) {
        fprintf(f, " * Mnemonic: %s\n", variants[0].mnemonic);
        fprintf(f, " * Operands: %d\n", variants[0].operandCount);
        fprintf(f, " * Opcode: 0x%04X\n", variants[0].opcode);
    } else {
        fprintf(f, " * Variants: %d\n", variant_count);
        fprintf(f, " * Mnemonics:");
        for (int i = 0; i < variant_count && i < 6; i++) {
            fprintf(f, " %s", variants[i].mnemonic);
        }
        if (variant_count > 6) fprintf(f, " ...");
        fprintf(f, "\n");
        fprintf(f, " * Operands: %d\n", variants[0].operandCount);
        fprintf(f, " * \n * Opcodes:\n");
        for (int i = 0; i < variant_count && i < 10; i++) {
            fprintf(f, " *   0x%04X (%s)\n", variants[i].opcode, variants[i].mnemonic);
        }
        if (variant_count > 10) fprintf(f, " *   ... (%d more)\n", variant_count - 10);
    }

    fprintf(f, " */\n");

    /* Write function signature and body */
    fprintf(f, "void nd500_instr_%s(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {\n", func_name);
    fprintf(f, "    /* TODO: Implement %s instruction\n", func_name);
    fprintf(f, "     * \n");
    fprintf(f, "     * Implementation notes:\n");
    fprintf(f, "     * - Operand count: %d\n", variants[0].operandCount);
    fprintf(f, "     * - Access operands via: fi->operands[0..%d]\n", variants[0].operandCount - 1);
    fprintf(f, "     * - Use read_operand_w() / write_operand_w() helpers from cpu_instr.c\n");
    fprintf(f, "     * - Update CPU registers and FLAGS as needed\n");
    fprintf(f, "     * - PC will be advanced automatically by cpu_step()\n");
    fprintf(f, "     * \n");
    fprintf(f, "     * Current status: STUB - Not implemented\n");
    fprintf(f, "     */\n");
    fprintf(f, "    \n");
    fprintf(f, "    static int warned = 0;\n");
    fprintf(f, "    if (!warned) {\n");
    fprintf(f, "        printf(\"[STUB] %s instruction not implemented (mnemonic: %%s, opcode: 0x%%04X)\\n\", \n", func_name);
    fprintf(f, "               fi->mnemonic, fi->opcode);\n");
    fprintf(f, "        warned = 1;\n");
    fprintf(f, "    }\n");
    fprintf(f, "    \n");
    fprintf(f, "    /* Stub does nothing - PC will be advanced by cpu_step() */\n");
    fprintf(f, "}\n");

    fclose(f);
    fprintf(stderr, "[GEN] Created stub: %s\n", file_path);
}

/* Generate all stub files organized by class */
static void generate_stubs(InstructionMeta* instrs, int count) {
    /* Track unique function names per class */
    typedef struct {
        char class_name[32];
        char func_name[64];
        InstructionMeta variants[32];
        int variant_count;
    } FunctionGroup;

    FunctionGroup groups[512];
    int group_count = 0;

    /* Group instructions by class + functionName */
    for (int i = 0; i < count; i++) {
        int found = -1;
        for (int g = 0; g < group_count; g++) {
            if (strcmp(groups[g].class_name, instrs[i].class) == 0 &&
                strcmp(groups[g].func_name, instrs[i].functionName) == 0) {
                found = g;
                break;
            }
        }

        if (found == -1) {
            /* New function group */
            snprintf(groups[group_count].class_name, sizeof(groups[group_count].class_name),
                    "%s", instrs[i].class);
            snprintf(groups[group_count].func_name, sizeof(groups[group_count].func_name),
                    "%s", instrs[i].functionName);
            groups[group_count].variants[0] = instrs[i];
            groups[group_count].variant_count = 1;
            group_count++;
        } else {
            /* Add to existing group */
            if (groups[found].variant_count < 32) {
                groups[found].variants[groups[found].variant_count++] = instrs[i];
            }
        }
    }

    fprintf(stderr, "[GEN] Found %d unique functions across all classes\n", group_count);

    /* Generate stub file for each unique function */
    for (int g = 0; g < group_count; g++) {
        generate_stub_file(groups[g].class_name, groups[g].func_name,
                          groups[g].variants, groups[g].variant_count);
    }
}

/* Generate dispatch table header */
static void generate_dispatch_header(const char* out_path, InstructionMeta* instrs, int count) {
    FILE* f = fopen(out_path, "w");
    if (!f) {
        fprintf(stderr, "[GEN] Error: Could not create %s\n", out_path);
        return;
    }

    fprintf(f, "/*\n");
    fprintf(f, " * AUTO-GENERATED FILE - DO NOT EDIT\n");
    fprintf(f, " * Generated by tools/gen_instructions from src/cpu/instructions.json\n");
    fprintf(f, " */\n\n");
    fprintf(f, "#pragma once\n");
    fprintf(f, "#include <stdint.h>\n\n");

    /* Original struct for metadata table */
    fprintf(f, "typedef struct { uint16_t opcode; const char* mnemonic; uint8_t operands; uint8_t prefixes_mask; uint8_t variant; uint32_t op_templates[4]; } Nd500Instr;\n");
    fprintf(f, "extern const Nd500Instr g_nd500_instrs[];\n");
    fprintf(f, "extern const unsigned g_nd500_instrs_count;\n\n");

    /* New dispatch table */
    fprintf(f, "/* Forward declaration for CPU types */\n");
    fprintf(f, "typedef struct Nd500Cpu Nd500Cpu;\n");
    fprintf(f, "typedef struct Nd500FetchedInstruction Nd500FetchedInstruction;\n\n");

    fprintf(f, "/* Instruction execution function pointer type */\n");
    fprintf(f, "typedef void (*InstrExecFunc)(Nd500Cpu*, const Nd500FetchedInstruction*);\n\n");

    fprintf(f, "/* Dispatch table: 65536 entries indexed by opcode (sparse, mostly NULL) */\n");
    fprintf(f, "extern InstrExecFunc g_instr_exec_table[65536];\n");

    fclose(f);
    fprintf(stderr, "[GEN] Generated dispatch header: %s\n", out_path);
}

/* Generate dispatch table implementation */
static void generate_dispatch_source(const char* out_path, const char* json_path,
                                     InstructionMeta* instrs, int count) {
    FILE* f = fopen(out_path, "w");
    if (!f) {
        fprintf(stderr, "[GEN] Error: Could not create %s\n", out_path);
        return;
    }

    fprintf(f, "/*\n");
    fprintf(f, " * AUTO-GENERATED FILE - DO NOT EDIT\n");
    fprintf(f, " * Generated by tools/gen_instructions from %s\n", json_path);
    fprintf(f, " */\n\n");
    fprintf(f, "#include \"nd500_instructions_gen.h\"\n\n");

    /* Generate original metadata table */
    fprintf(f, "const Nd500Instr g_nd500_instrs[] = {\n");
    int total_count = 0;
    for (int i = 0; i < count; i++) {
        uint8_t mask = instrs[i].prefixMask;
        int has_reg_variants = (mask & 0x40) ? 1 : 0;
        int reg_count = has_reg_variants ? 4 : 1;

        for (int r = 0; r < reg_count; r++) {
            uint16_t actual_opcode = instrs[i].opcode + r;
            fprintf(f, "  { 0x%04X, \"%s\", %d, 0x%02X, %d, {0x%08X, 0x%08X, 0x%08X, 0x%08X} },\n",
                    actual_opcode, instrs[i].mnemonic, instrs[i].operandCount, mask, instrs[i].variant,
                    instrs[i].op_templates[0], instrs[i].op_templates[1],
                    instrs[i].op_templates[2], instrs[i].op_templates[3]);
            total_count++;
        }
    }
    fprintf(f, "};\n");
    fprintf(f, "const unsigned g_nd500_instrs_count = %d;\n\n", total_count);

    /* Collect unique functions for forward declarations */
    typedef struct { char class_name[32]; char func_name[64]; } UniqueFunc;
    UniqueFunc funcs[512];
    int func_count = 0;

    for (int i = 0; i < count; i++) {
        int found = 0;
        for (int j = 0; j < func_count; j++) {
            if (strcmp(funcs[j].class_name, instrs[i].class) == 0 &&
                strcmp(funcs[j].func_name, instrs[i].functionName) == 0) {
                found = 1;
                break;
            }
        }
        if (!found && func_count < 512) {
            snprintf(funcs[func_count].class_name, sizeof(funcs[func_count].class_name),
                    "%s", instrs[i].class);
            snprintf(funcs[func_count].func_name, sizeof(funcs[func_count].func_name),
                    "%s", instrs[i].functionName);
            func_count++;
        }
    }

    /* Forward declarations */
    fprintf(f, "/* Forward declarations for instruction implementations */\n");
    for (int i = 0; i < func_count; i++) {
        fprintf(f, "void nd500_instr_%s(Nd500Cpu*, const Nd500FetchedInstruction*);\n",
                funcs[i].func_name);
    }
    fprintf(f, "\n");

    /* Generate dispatch table with designated initializers */
    fprintf(f, "/* Dispatch table: 65536 entries indexed by opcode (sparse, mostly NULL) */\n");
    fprintf(f, "InstrExecFunc g_instr_exec_table[65536] = {\n");

    /* Track which opcodes have been written to avoid duplicates */
    uint8_t* written_opcodes = (uint8_t*)calloc(65536, sizeof(uint8_t));
    if (!written_opcodes) {
        fprintf(stderr, "[GEN] Error: Failed to allocate opcode tracking array\n");
        fclose(f);
        return;
    }

    int skipped_duplicates = 0;
    for (int i = 0; i < count; i++) {
        uint8_t mask = instrs[i].prefixMask;
        int has_reg_variants = (mask & 0x40) ? 1 : 0;
        int reg_count = has_reg_variants ? 4 : 1;

        for (int r = 0; r < reg_count; r++) {
            uint16_t actual_opcode = instrs[i].opcode + r;

            /* Skip if this opcode has already been written */
            if (written_opcodes[actual_opcode]) {
                skipped_duplicates++;
                continue;
            }

            fprintf(f, "    [0x%04X] = nd500_instr_%s,  /* %s */\n",
                    actual_opcode, instrs[i].functionName, instrs[i].mnemonic);
            written_opcodes[actual_opcode] = 1;
        }
    }

    free(written_opcodes);
    fprintf(f, "};\n");

    if (skipped_duplicates > 0) {
        fprintf(stderr, "[GEN] Note: Skipped %d duplicate opcode entries\n", skipped_duplicates);
    }

    fclose(f);
    fprintf(stderr, "[GEN] Generated dispatch source: %s (%d instructions)\n", out_path, total_count);
}

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: gen_instructions <instructions.json> <out.h> <out.c>\n");
        return 1;
    }

    const char* json_path = argv[1];
    const char* out_h = argv[2];
    const char* out_c = argv[3];

    /* Parse instructions.json */
    InstructionMeta* instrs = NULL;
    int count = 0;

    if (parse_instructions(json_path, &instrs, &count) != 0) {
        fprintf(stderr, "[GEN] Error: Failed to parse %s\n", json_path);
        return 1;
    }

    /* Generate stub files (only if they don't exist) */
    generate_stubs(instrs, count);

    /* Generate dispatch header and source */
    generate_dispatch_header(out_h, instrs, count);
    generate_dispatch_source(out_c, json_path, instrs, count);

    free(instrs);

    fprintf(stderr, "[GEN] Code generation complete!\n");
    return 0;
}
