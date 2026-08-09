/*
 * nd500_ethhub.c - the RetroCore TCP ethernet wire format. See nd500_ethhub.h
 * for where every constant in here was read from.
 */

#include <string.h>
#include <stdlib.h>

#include "nd500_ethhub.h"

static const uint8_t ETHHUB_MAGIC[4] = { 0x52, 0x45, 0x54, 0x48 };   /* "RETH" */

int nd500_ethhub_parse_spec(const char* spec, char* host, size_t hostlen,
                            int* port) {
    const char* body;
    const char* colon;
    int had_tcp_prefix;
    size_t n;

    if (!spec || !host || hostlen == 0 || !port) return 0;

    while (*spec == ' ' || *spec == '\t') spec++;

    had_tcp_prefix = (strncmp(spec, "tcp:", 4) == 0);
    body = had_tcp_prefix ? spec + 4 : spec;
    if (*body == '\0') return 0;

    /* LAST colon, not the first: "tcp:host:port" has already had its prefix
     * removed, but an IPv6-looking host would otherwise be cut in the middle.
     * RetroCore uses LastIndexOf for the same reason (:152). */
    colon = strrchr(body, ':');
    if (!colon) {
        /* No port. Only accept this with an explicit "tcp:" - a bare word is
         * junk, not a hostname (TcpEthernetBackend.cs:155-159). */
        if (!had_tcp_prefix) return 0;
        n = strlen(body);
        if (n == 0 || n >= hostlen) return 0;
        memcpy(host, body, n);
        host[n] = '\0';
        *port = ETHHUB_DEFAULT_PORT;
        return 1;
    }

    n = (size_t)(colon - body);
    if (n == 0 || n >= hostlen) return 0;
    memcpy(host, body, n);
    host[n] = '\0';

    if (colon[1] == '\0') {
        *port = ETHHUB_DEFAULT_PORT;
        return 1;
    }
    {
        char* end = NULL;
        long p = strtol(colon + 1, &end, 10);
        if (!end || *end != '\0' || p < 1 || p > 65535) return 0;
        *port = (int)p;
    }
    return 1;
}

void nd500_ethhub_build_handshake(uint8_t* out, uint8_t version) {
    if (!out) return;
    memcpy(out, ETHHUB_MAGIC, 4);
    out[4] = version;
}

int nd500_ethhub_check_handshake(const uint8_t* in, uint8_t* peer_version) {
    if (peer_version) *peer_version = 0;
    if (!in) return 0;
    if (memcmp(in, ETHHUB_MAGIC, 4) != 0) return 0;
    if (peer_version) *peer_version = in[4];
    return in[4] >= 1;          /* "accept any version >= 1" (:433) */
}

int nd500_ethhub_take_frame(const uint8_t* buf, size_t avail, uint16_t* frame_len) {
    unsigned len;

    if (frame_len) *frame_len = 0;
    if (!buf || avail < 2) return 0;

    len = ((unsigned)buf[0] << 8) | buf[1];
    if (len == 0 || len > ETHHUB_MAX_FRAME) return -1;   /* desync - see header */
    if (avail < len + 2u) return 0;

    if (frame_len) *frame_len = (uint16_t)len;
    return (int)(len + 2u);
}

void nd500_ethhub_put_length(uint8_t* out, uint16_t len) {
    if (!out) return;
    out[0] = (uint8_t)((len >> 8) & 0xFF);
    out[1] = (uint8_t)(len & 0xFF);
}
