#!/usr/bin/env python3
"""Verify NRF module checksums.

Full path: tools/nrfcheck.py

Appendix D of the ND Linker manual, END group:
  "The numeric length (NL) specifies the size of the checksum in bytes.
   0 - no checksum test is performed.  2 - default value.
   The numeric value contains the checksum in 2's complement form.
   The checksum is calculated by adding the binary byte values from BEG to
   END, trailing fields included, ignoring overflow."

The exact span ("from BEG to END") is ambiguous about which end bytes are
included, so this script tries several candidate spans and reports which one
verifies for every module in the file.  A file whose modules all verify under
one consistent rule is intact; a file that cannot be made to verify is either
damaged or uses a construct the reader does not model.

Usage:  python3 nrfcheck.py <file.nrf>
"""

import sys
sys.path.insert(0, __file__.rsplit("/", 1)[0])
from nrfdump import CTRL


def groups(d):
    """Yield (offset, mnemonic, nl, numeric_bytes, end_offset) sequentially."""
    i = 0
    while i < len(d):
        start = i
        cb = d[i]
        ctr, nl = cb >> 3, cb & 7
        mnem, has_sym = CTRL[ctr]
        i += 1
        num = d[i:i + nl]
        i += nl
        if has_sym:
            if i >= len(d):
                return
            sl = d[i]
            i += 1 + sl
        if mnem == "LDN":
            i += int.from_bytes(num, "big", signed=True) if nl else 0
        yield start, mnem, nl, num, i
        if mnem == "EOF":
            return


SPANS = {
    "BEGctl..ENDctl-1":  lambda b, ec, ee: (b, ec),
    "BEGctl..ENDgrp":    lambda b, ec, ee: (b, ee),
    "BEGctl..ENDctl":    lambda b, ec, ee: (b, ec + 1),
}


def main(path):
    d = open(path, "rb").read()
    mods = []          # (beg_off, end_ctl_off, end_grp_off, nl, checksum)
    beg = None
    for off, mnem, nl, num, nxt in groups(d):
        if mnem == "BEG":
            beg = off
        elif mnem == "END" and beg is not None:
            mods.append((beg, off, nxt, nl,
                         int.from_bytes(num, "big") if nl else None))
            beg = None
    print("%s: %d modules with BEG..END" % (path, len(mods)))
    checked = [m for m in mods if m[3] == 2]
    print("  %d carry a 2-byte checksum" % len(checked))
    if not checked:
        return
    for name, span in SPANS.items():
        ok = bad = 0
        first_bad = None
        for b, ec, ee, nl, cks in checked:
            lo, hi = span(b, ec, ee)
            s = sum(d[lo:hi]) & 0xFFFF
            if (s + cks) & 0xFFFF == 0 or s == cks:
                ok += 1
            else:
                bad += 1
                if first_bad is None:
                    first_bad = b
        print("  span %-18s ok=%-5d bad=%-5d%s"
              % (name, ok, bad,
                 "  first bad module at 0x%06X" % first_bad if first_bad else ""))


if __name__ == "__main__":
    for p in sys.argv[1:]:
        main(p)
