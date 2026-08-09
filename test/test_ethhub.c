/*
 * test_ethhub.c - the RetroCore TCP ethernet wire format.
 *
 * WHY THIS IS WORTH A TEST FILE
 * -----------------------------
 * Every byte here is a claim about somebody ELSE's code - RetroCore's
 * TcpEthernetBackend.cs and TcpEthernetRelay.cs - and the failure mode of
 * getting one wrong is a connection that opens, says hello, and then silently
 * carries nothing: the peer drops the link on a bad length and never explains.
 * There is nothing to see from either side, so it is pinned here instead.
 *
 * The socket half (uplink_tcp.c) is connect/read/write around these functions
 * and has nothing in it that can be wrong quietly.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../src/cpu/nd500_ethhub.h"

static int passed = 0, failed = 0;

static void check(const char* what, int ok) {
    if (ok) { passed++; printf("  [PASS] %s\n", what); }
    else    { failed++; printf("  [FAIL] %s\n", what); }
}
static void check_eq(const char* what, long expect, long got) {
    if (expect == got) { passed++; printf("  [PASS] %s\n", what); }
    else { failed++; printf("  [FAIL] %s: expected %ld, got %ld\n", what, expect, got); }
}
static void check_str(const char* what, const char* expect, const char* got) {
    if (strcmp(expect, got) == 0) { passed++; printf("  [PASS] %s\n", what); }
    else { failed++; printf("  [FAIL] %s: expected \"%s\", got \"%s\"\n", what, expect, got); }
}

int main(void) {
    char host[256];
    int port;
    uint8_t buf[64];
    uint8_t ver;
    uint16_t flen;

    printf("RETH wire format\n");

    /* ---- the handshake ----------------------------------------------------
     * 5 bytes: 'R','E','T','H' then the version. Version 1 announces "an
     * ordinary member with one emulated NIC", which is what NDIX is; 2 would
     * claim to be a hub carrying the inter-hub header
     * (TcpEthernetBackend.cs:386-392). Announcing the wrong one would make the
     * relay expect a header we never send. */
    printf("\nthe handshake\n");
    memset(buf, 0, sizeof buf);
    nd500_ethhub_build_handshake(buf, ETHHUB_VERSION_MEMBER);
    check("the magic is \"RETH\"", buf[0] == 0x52 && buf[1] == 0x45 &&
                                   buf[2] == 0x54 && buf[3] == 0x48);
    check_eq("we announce version 1 - one NIC, not a hub", 1, buf[4]);

    check("our own handshake is one we would accept",
          nd500_ethhub_check_handshake(buf, &ver) == 1);
    check_eq("and the version is reported back", 1, ver);

    /* "accept any version >= 1" (TcpEthernetBackend.cs:433) - so a relay that
     * has moved on must still be talked to, not refused. */
    buf[4] = 2;
    check("a version 2 peer is accepted", nd500_ethhub_check_handshake(buf, &ver) == 1);
    check_eq("and identified as version 2", 2, ver);

    buf[4] = 0;
    check("version 0 is refused", nd500_ethhub_check_handshake(buf, &ver) == 0);
    buf[4] = 1; buf[0] = 'X';
    check("wrong magic is refused", nd500_ethhub_check_handshake(buf, &ver) == 0);
    check_eq("and no version is reported for it", 0, ver);

    /* ---- the frame prefix -------------------------------------------------
     * [u16 BIG-ENDIAN length][bytes]. Big-endian: TcpEthernetBackend.cs:463
     * writes (length >> 8) first. Getting the byte order wrong turns a 58-byte
     * frame into a 14848-byte one, which the peer rejects as desync. */
    printf("\nthe length prefix\n");
    nd500_ethhub_put_length(buf, 58);
    check("the length is big-endian", buf[0] == 0x00 && buf[1] == 0x3A);
    nd500_ethhub_put_length(buf, 0x0102);
    check("both bytes, high first", buf[0] == 0x01 && buf[1] == 0x02);

    /* ---- taking frames out of a stream ------------------------------------
     * A frame can be split across any number of reads, and one read can carry
     * several frames. Both are normal on TCP and both have to work. */
    printf("\ntaking frames out of a stream\n");
    memset(buf, 0, sizeof buf);
    check_eq("an empty buffer yields nothing", 0,
             nd500_ethhub_take_frame(buf, 0, &flen));
    check_eq("one byte is not even a length", 0,
             nd500_ethhub_take_frame(buf, 1, &flen));

    nd500_ethhub_put_length(buf, 4);
    buf[2] = 0xAA; buf[3] = 0xBB; buf[4] = 0xCC; buf[5] = 0xDD;
    check_eq("a length with no body yet yields nothing", 0,
             nd500_ethhub_take_frame(buf, 2, &flen));
    check_eq("a body one byte short yields nothing", 0,
             nd500_ethhub_take_frame(buf, 5, &flen));
    check_eq("a complete frame consumes length + body", 6,
             nd500_ethhub_take_frame(buf, 6, &flen));
    check_eq("and reports the body length", 4, flen);
    check("the frame itself starts two bytes in",
          buf[2] == 0xAA && buf[5] == 0xDD);

    /* Two frames in one read: the second must still be there afterwards. */
    nd500_ethhub_put_length(buf, 2);
    buf[2] = 0x11; buf[3] = 0x22;
    nd500_ethhub_put_length(buf + 4, 3);
    buf[6] = 0x33; buf[7] = 0x44; buf[8] = 0x55;
    check_eq("the first of two frames consumes only its own bytes", 4,
             nd500_ethhub_take_frame(buf, 9, &flen));
    check_eq("length 2", 2, flen);
    check_eq("and the second is taken next", 5,
             nd500_ethhub_take_frame(buf + 4, 5, &flen));
    check_eq("length 3", 3, flen);

    /* Desync. There is no marker to hunt for in this protocol, so a length
     * that cannot be right means the stream is lost - RetroCore drops the link
     * on exactly this (TcpEthernetBackend.cs:446-449), and so must we, rather
     * than reading rubbish for ever. */
    printf("\ndesync\n");
    nd500_ethhub_put_length(buf, 0);
    check_eq("a zero length is desync, not an empty frame", -1,
             nd500_ethhub_take_frame(buf, 8, &flen));
    buf[0] = 0xFF; buf[1] = 0xFF;                      /* 65535 */
    check_eq("a length above the 2048 maximum is desync", -1,
             nd500_ethhub_take_frame(buf, 8, &flen));
    nd500_ethhub_put_length(buf, ETHHUB_MAX_FRAME);
    check_eq("exactly the maximum is legal, just incomplete here", 0,
             nd500_ethhub_take_frame(buf, 8, &flen));

    /* ---- the spec string --------------------------------------------------
     * TcpEthernetBackend.Parse's CONNECT forms (:142-176). The interesting
     * rule is the last one: a bare word with no port is REJECTED, so a typo is
     * an error message rather than a DNS lookup for "lop". */
    printf("\nthe uplink spec\n");
    check("tcp:host:port parses",
          nd500_ethhub_parse_spec("tcp:relay.example:4001", host, sizeof host, &port) == 1);
    check_str("host", "relay.example", host);
    check_eq("port", 4001, port);

    check("host:port parses without the prefix",
          nd500_ethhub_parse_spec("192.168.1.5:3094", host, sizeof host, &port) == 1);
    check_str("host", "192.168.1.5", host);
    check_eq("port", 3094, port);

    check("tcp:host with no port parses",
          nd500_ethhub_parse_spec("tcp:relay", host, sizeof host, &port) == 1);
    check_str("host", "relay", host);
    check_eq("and defaults to 3094 - the ND Ethernet II PCB number",
             ETHHUB_DEFAULT_PORT, port);

    check("a bare word with no port and no prefix is REFUSED",
          nd500_ethhub_parse_spec("relay", host, sizeof host, &port) == 0);
    check("so is an empty spec",
          nd500_ethhub_parse_spec("", host, sizeof host, &port) == 0);
    check("so is a missing host",
          nd500_ethhub_parse_spec(":3094", host, sizeof host, &port) == 0);
    check("a port of 0 is refused - it means \"any\" to bind, nothing to dial",
          nd500_ethhub_parse_spec("tcp:relay:0", host, sizeof host, &port) == 0);
    check("so is a port above 65535",
          nd500_ethhub_parse_spec("tcp:relay:70000", host, sizeof host, &port) == 0);
    check("and so is a port that is not a number",
          nd500_ethhub_parse_spec("tcp:relay:hub", host, sizeof host, &port) == 0);

    /* A host longer than the caller's buffer must be refused, not truncated -
     * a truncated hostname would resolve to something else entirely. */
    check("a host too long for the buffer is refused, not cut short",
          nd500_ethhub_parse_spec("tcp:aaaaaaaaaaaaaaaaaaaa:1", host, 8, &port) == 0);

    /* ---- the listen forms -------------------------------------------------
     * TcpEthernetBackend.Parse:120-140. These let two emulators pair up with
     * nothing else running - one listens, the other dials. */
    printf("\nthe listen forms\n");
    port = -1;
    check("bare \"listen\" parses", nd500_ethhub_parse_listen("listen", &port) == 1);
    check_eq("and defaults to 3094", ETHHUB_DEFAULT_PORT, port);
    check("\"tcp-listen\" parses too", nd500_ethhub_parse_listen("tcp-listen", &port) == 1);
    check_eq("same default", ETHHUB_DEFAULT_PORT, port);
    check("listen:<port> parses", nd500_ethhub_parse_listen("listen:4100", &port) == 1);
    check_eq("with that port", 4100, port);
    check("tcp-listen:<port> parses",
          nd500_ethhub_parse_listen("tcp-listen:4101", &port) == 1);
    check_eq("with that port", 4101, port);

    /* 0 means "any free port" to bind. It is legal HERE and refused on the
     * connect side, where it means nothing at all. */
    check("listen:0 is legal - the OS picks the port",
          nd500_ethhub_parse_listen("listen:0", &port) == 1);
    check_eq("and it is reported as 0 until the bind says otherwise", 0, port);

    check("a connect spec is not a listen spec",
          nd500_ethhub_parse_listen("tcp:relay:3094", &port) == 0);
    check("and a word that merely starts with \"listen\" is not one either",
          nd500_ethhub_parse_listen("listening", &port) == 0);
    check("nor is a listen with a junk port",
          nd500_ethhub_parse_listen("listen:abc", &port) == 0);
    check("a listen spec is not a connect spec",
          nd500_ethhub_parse_spec("listen", host, sizeof host, &port) == 0);

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
