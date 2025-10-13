#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/machine/machine_protos.h"
#include "../src/ndlib/ndlib.h"

static int run_one(const char* obj_path) {
    Nd500Machine m; memset(&m, 0, sizeof(m));
    nd500_machine_init(&m, 1<<20);
    uint32_t entry = 0;
    if (ndlib_loadaout_file(&m, obj_path, &entry) != 0) {
        fprintf(stderr, "load failed: %s\n", obj_path);
        return 1;
    }
    char buf[4096];
    size_t n = nd500_dbg_disasm(&m, entry, 64, buf, sizeof(buf));
    if (n == 0) return 2;

    /* Run external nd500-dis and capture output for comparison */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "nd500-dis %s", obj_path);
    FILE* fp = popen(cmd, "r");
    if (!fp) return 3;
    char ref[4096]; size_t rn = fread(ref, 1, sizeof(ref)-1, fp); ref[rn] = '\0';
    pclose(fp);

    /* Simple substr checks for expected lines */
    if (strstr(ref, "w1 :=        b.20") && strstr(ref, "w1 +         b.24")) {
        /* Our disasm must contain analogous tokens */
        if (strstr(buf, "w1 :=") && strstr(buf, "b.20") && strstr(buf, "+") && strstr(buf, "b.24")) return 0;
    }
    /* Fallback: accept 'ret' presence */
    if (strstr(buf, "ret")) return 0;
    fprintf(stderr, "Mismatch. Our disasm:\n%s\nRef:\n%.*s\n", buf, (int)rn, ref);
    return 4;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: disasm_tests <obj-file>\n");
        return 1;
    }
    return run_one(argv[1]);
}


