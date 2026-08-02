/*
 * SIMH .tap record-layer tests.
 *
 * The tape device (MON 600 generic 2) cannot be exercised through a boot: the
 * guest image has no mt binary and no /dev/mt nodes, so mtattach's FE_IDEV is
 * the only tape fecall that ever runs. Everything below the attach - reading a
 * record, spacing forward and back over records and files, rewinding - would
 * otherwise be code that has never executed.
 *
 * The fixture is built here rather than read from disk so the test needs no
 * external data and no interpreter.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../src/cpu/nd500_tape.h"

static int passed = 0, failed = 0;

static void check(const char* what, int ok, const char* detail) {
    if (ok) { passed++; printf("  [PASS] %s\n", what); }
    else    { failed++; printf("  [FAIL] %s: %s\n", what, detail ? detail : ""); }
}

static void put_le32(FILE* f, uint32_t v) {
    fputc((int)( v        & 0xFF), f);
    fputc((int)((v >>  8) & 0xFF), f);
    fputc((int)((v >> 16) & 0xFF), f);
    fputc((int)((v >> 24) & 0xFF), f);
}

/* One data record: header, payload, pad to even, trailer. */
static void put_rec(FILE* f, const char* s) {
    uint32_t n = (uint32_t)strlen(s);
    put_le32(f, n);
    fwrite(s, 1, n, f);
    if (n & 1u) fputc(0, f);          /* pad to an even byte count */
    put_le32(f, n);
}

/* Layout written below, and what each test then expects:
 *
 *   "hello tape"     10 bytes, even
 *   "second record"  13 bytes, ODD - exercises the pad byte
 *   <tape mark>
 *   "file two"        8 bytes
 *   <tape mark>
 *   <end of medium>
 */
static const char* R0 = "hello tape";
static const char* R1 = "second record";
static const char* R2 = "file two";

static void build(const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); exit(2); }
    put_rec(f, R0);
    put_rec(f, R1);
    put_le32(f, ND500_TAPE_MARK);
    put_rec(f, R2);
    put_le32(f, ND500_TAPE_MARK);
    put_le32(f, ND500_TAPE_EOM);
    fclose(f);
}

int main(void) {
    printf("ND-500 tape (SIMH .tap) record layer\n====================================\n");

    char path[] = "/tmp/nd500x_tape_testXXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) { perror("mkstemp"); return 2; }
    close(fd);
    build(path);

    Nd500Tape t;
    if (nd500_tape_attach(&t, path) != 0) {
        printf("  [FAIL] attach\n");
        remove(path);
        return 1;
    }
    check("attach", 1, NULL);

    uint8_t buf[64];
    uint32_t actual, reclen;
    int mark, err;
    char det[160];

    /* ---- sequential read of the first two records ---- */
    memset(buf, 0, sizeof buf);
    int ok = nd500_tape_read(&t, buf, sizeof buf, &actual, &reclen, &mark, &err);
    snprintf(det, sizeof det, "ok=%d actual=%u reclen=%u mark=%d", ok, actual, reclen, mark);
    check("read record 1 length", ok && actual == strlen(R0) && reclen == strlen(R0) && !mark, det);
    check("read record 1 content", memcmp(buf, R0, strlen(R0)) == 0, (char*)buf);

    memset(buf, 0, sizeof buf);
    ok = nd500_tape_read(&t, buf, sizeof buf, &actual, &reclen, &mark, &err);
    snprintf(det, sizeof det, "actual=%u reclen=%u", actual, reclen);
    check("read record 2 (odd length, padded on the medium)",
          ok && actual == strlen(R1) && reclen == strlen(R1), det);
    check("read record 2 content", memcmp(buf, R1, strlen(R1)) == 0, (char*)buf);

    /* ---- a tape mark reads as zero bytes, not an error ---- */
    ok = nd500_tape_read(&t, buf, sizeof buf, &actual, &reclen, &mark, &err);
    snprintf(det, sizeof det, "ok=%d actual=%u mark=%d", ok, actual, mark);
    check("tape mark reads as 0 bytes and is flagged", ok && actual == 0 && mark == 1, det);

    /* ---- short read: the record is consumed in full, reclen reports the truth ---- */
    memset(buf, 0, sizeof buf);
    ok = nd500_tape_read(&t, buf, 4, &actual, &reclen, &mark, &err);
    snprintf(det, sizeof det, "actual=%u reclen=%u (want 4 / %zu)", actual, reclen, strlen(R2));
    check("short read returns 4 bytes but the record's real length",
          ok && actual == 4 && reclen == strlen(R2), det);
    check("short read content", memcmp(buf, R2, 4) == 0, (char*)buf);

    /* After a short read the position must be past the WHOLE record, so the
     * next thing on the tape is the second filemark - not the tail of R2. */
    ok = nd500_tape_read(&t, buf, sizeof buf, &actual, &reclen, &mark, &err);
    snprintf(det, sizeof det, "ok=%d mark=%d actual=%u", ok, mark, actual);
    check("short read still consumed the whole record", ok && mark == 1 && actual == 0, det);

    /* ---- end of medium ---- */
    ok = nd500_tape_read(&t, buf, sizeof buf, &actual, &reclen, &mark, &err);
    check("end of medium returns 0", ok == 0, "expected 0 at EOM");

    /* ---- rewind, then space forward record by record ---- */
    nd500_tape_rewind(&t);
    check("rewind returns to load point", t.pos == 0, "pos != 0");

    uint32_t h;
    check("FSR over record 1", nd500_tape_fwd(&t, &h) && h == strlen(R0), "hdr mismatch");
    check("FSR over record 2", nd500_tape_fwd(&t, &h) && h == strlen(R1), "hdr mismatch");
    long after_two = t.pos;

    /* ---- backspace must land exactly where we were ---- */
    check("BSR back over record 2", nd500_tape_back(&t, &h) && h == strlen(R1), "hdr mismatch");
    check("BSR then FSR is a round trip",
          nd500_tape_fwd(&t, &h) && t.pos == after_two, "position drifted");

    /* ---- BSR to BOT, and refuse to go past it ---- */
    nd500_tape_back(&t, &h);
    nd500_tape_back(&t, &h);
    snprintf(det, sizeof det, "pos=%ld", t.pos);
    check("BSR twice returns to BOT", t.pos == 0, det);
    check("BSR at BOT fails rather than going negative", nd500_tape_back(&t, &h) == 0, det);

    /* ---- FSF: skip to just past the first filemark ---- */
    nd500_tape_rewind(&t);
    int guard = 0;
    for (;;) {
        if (!nd500_tape_fwd(&t, &h)) { check("FSF found a filemark", 0, "hit EOM first"); break; }
        if (h == ND500_TAPE_MARK) { check("FSF stops at the filemark", 1, NULL); break; }
        if (++guard > 10) { check("FSF terminates", 0, "runaway"); break; }
    }
    /* The next record after file 1 must be R2. */
    memset(buf, 0, sizeof buf);
    nd500_tape_read(&t, buf, sizeof buf, &actual, &reclen, &mark, &err);
    check("record after the filemark is file two's first record",
          actual == strlen(R2) && memcmp(buf, R2, strlen(R2)) == 0, (char*)buf);

    nd500_tape_detach(&t);
    remove(path);

    /* ---- a missing image must fail, not crash ---- */
    Nd500Tape missing;
    check("attaching a nonexistent image fails cleanly",
          nd500_tape_attach(&missing, "/nonexistent/nd500x-no-such.tap") != 0, NULL);

    printf("\n====================================\nResults: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
