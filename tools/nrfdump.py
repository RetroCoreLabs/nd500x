#!/usr/bin/env python3
"""Sequential NRF (ND Relocatable Format) dumper.

Full path: /home/ronny/repos/nd500x/tools/nrfdump.py

Decodes an NRF file strictly per Appendix D of
/home/ronny/repos/nd500x/docs/ND-860289-2-EN ND Linker User Guide and Reference Manual.md

Control field = one byte: high 5 bits = control number, low 3 bits = numeric
length NL (0..7).  Numeric field = NL bytes, 2's complement.  Symbolic field,
present only for the control numbers marked (S), = one length byte SL followed
by SL ASCII characters.

Control numbers in the manual are written in OCTAL.

Usage:  python3 nrfdump.py <file.nrf> [max_groups]
"""

import sys

# control number (decimal) -> (mnemonic, has_symbolic_field)
CTRL = {
    0:  ("NUL", False),
    1:  ("BEG", False),
    2:  ("END", False),
    3:  ("MSA", False),
    4:  ("LIB", True),
    5:  ("DEF", True),
    6:  ("REF", True),
    7:  ("LRF", True),
    8:  ("DDF", True),   # 10 octal
    9:  ("DRF", True),   # 11 octal
    10: ("RMV", True),   # 12 octal
    11: ("SLA", True),   # 13 octal
    12: ("AJS", False),  # 14 octal
    13: ("PMO", False),  # 15 octal
    14: ("DMO", False),  # 16 octal
    15: ("FMO", True),   # 17 octal
    16: ("REP", False),  # 20 octal
    17: ("LDI", False),  # 21 octal
    18: ("ADI", False),  # 22 octal
    19: ("APA", False),  # 23 octal
    20: ("ADA", False),  # 24 octal
    21: ("IHB", False),  # 25 octal
    22: ("EOF", False),  # 26 octal
    23: ("DBG", False),  # 27 octal
    24: ("LBB", True),   # 30 octal
    25: ("MSG", True),   # 31 octal
    26: ("MIS", False),  # 32 octal
    27: ("LDN", False),  # 33 octal
    28: ("IL1", False),  # 34 octal
    29: ("IL2", False),  # 35 octal
    30: ("IL3", False),  # 36 octal
    31: ("IL4", False),  # 37 octal
}

LANG = {0: "ASSEMBLY", 1: "FORTRAN", 2: "PLANC", 3: "COBOL", 4: "PASCAL",
        5: "SIMULA", 6: "ADA", 7: "CORAL", 8: "C", 9: "BASIC"}

MIS_SUB = {0: "CGR0 start-compound", 1: "CGR1 end-compound", 2: "ADD",
           3: "SUB", 4: "MUL", 5: "DIV"}


def dump(path, limit, parity):
    d = open(path, "rb").read()
    i = 0
    n_groups = 0
    while i < len(d) and n_groups < limit:
        raw = d[i]
        cb = raw & 0x7F if parity else raw
        ctr = cb >> 3
        nl = cb & 7
        mnem, has_sym = CTRL[ctr]
        off = i
        i += 1
        num_bytes = d[i:i + nl]
        i += nl
        num = int.from_bytes(num_bytes, "big", signed=True) if nl else 0
        sym = ""
        if has_sym:
            if i >= len(d):
                break
            sl = d[i]
            i += 1
            sym = d[i:i + sl].decode("latin-1")
            i += sl
        extra = ""
        if mnem == "BEG" and nl >= 2:
            extra = "  prio=%d language=%d(%s)" % (
                num_bytes[0], num_bytes[1], LANG.get(num_bytes[1], "?"))
            if nl >= 3:
                extra += " addrlen=%d" % num_bytes[2]
            if nl >= 4:
                extra += " TMa=0x%02X" % num_bytes[3]
            if nl >= 5:
                extra += " OSID=%d" % num_bytes[4]
        if mnem == "MIS":
            extra = "  " + MIS_SUB.get(num, "?")
        if mnem == "LDN":
            # LDN's numeric field is a byte COUNT, so read it unsigned; a
            # signed read makes a count >= 0x80 rewind the parse position.
            count = int.from_bytes(num_bytes, "big") if nl else 0
            payload = d[i:i + count]
            i += count
            extra = "  bytes=" + payload.hex(" ")
        if mnem == "LDI":
            extra = "  imm=" + num_bytes.hex(" ")
        print("%06X: %-4s NL=%d N=%-12s %s%s"
              % (off, mnem, nl, num_bytes.hex(" ") or "-",
                 ('"%s"' % sym) if has_sym else "", extra))
        n_groups += 1
        if mnem == "EOF":
            break


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    lim = int(sys.argv[2]) if len(sys.argv) > 2 else 100000
    par = len(sys.argv) > 3 and sys.argv[3] == "parity"
    dump(sys.argv[1], lim, par)
