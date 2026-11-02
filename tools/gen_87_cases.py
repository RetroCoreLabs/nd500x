#!/usr/bin/env python3
"""
gen_87_cases.py - ND-5000 '87 extension instructions the emulator implements

Adds single-instruction cases to test/nd500-conformance.json for the '87 instructions that
nd500x runs and that the corpus had no case for (ND-05.009.4 section numbers in brackets):

  PLCCN  [15.15]  N = u-l+1, and 0 when that is negative; Z when N = 0
  ENTIER [10.37]  F ENTIER and D ENTIER, the largest integer not greater than the source;
                  Z for a zero result, S for a negative one
  DDIRT  [16.11]  at the manual's opcode 0xFFFA (the corpus already has it at 0xFE1E)
  CLINIT [15.17], CLREAD [15.18], REXT [16.28], WEXT [16.29]  decode and, for the
                  privileged ones, the trap in user mode

The expected values come from the manual text and from the exact reference arithmetic of
gen_float_const_cases.py. They never come from running the emulator.

What is deliberately NOT asserted, because the manual and the microcode do not settle it:
  * PLCCN word A (the manual prints "a + 1", the microcode scales the lower bound by the
    element size) and the other status bits;
  * the byte offsets of a, l, u (0, 4, 8) and of N, A (0, 4) in the two descriptors are
    read from the order of the manual's drawing, not from a byte-level source;
  * ENTIER for a result outside 32 bits;
  * the value CLREAD returns (it depends on time), and what REXT and WEXT do with a device.

Usage:  python3 tools/gen_87_cases.py [path-to-nd500-conformance.json]
The script removes every case whose name starts with "Ext87_" and appends the fresh ones,
so running it twice gives the same file.
"""
import json
import math
import os
import sys
from fractions import Fraction

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_float_const_cases as fc

PREFIX = "Ext87_"
PIA = 0x2                       # ST1 bit 1, privileged instructions allowed
B0 = fc.B0
ILLEGAL = ("IllegalInstruction", 8589934592)   # what the corpus expects for a privileged instruction in user mode
POISON_WORD = [0xDE, 0xAD, 0xBE, 0xEF]


def case(name, assembly, nbytes, initial_regs, initial_ram, final_regs, final_ram, trap=None, st=0):
    c = fc.case(name, assembly, nbytes, initial_regs, initial_ram, final_regs, final_ram, trap=trap)
    c["name"] = PREFIX + name[len(fc.PREFIX):] if name.startswith(fc.PREFIX) else PREFIX + name
    c["initial"]["regs"]["st"] = st
    return c


def cases_plccn():
    out = []
    # (a, l, u): the PLANC descriptor at b.0 .. b.0+11, three words
    for tag, a, l, u in (("Normal", 0x2000, 1, 10), ("OneElement", 0x2000, 0, 0),
                         ("Empty", 0x3000, 5, 4), ("NegativeCount", 0x3000, 5, 2),
                         ("NegativeLower", 0x2000, 0xFFFFFFFD, 4)):
        sl = l - (1 << 32) if l >= 1 << 31 else l
        n = max(u - sl + 1, 0)
        ram = fc.ram_bytes(B0, fc.be(a, 4) + fc.be(l, 4) + fc.be(u, 4)) + fc.ram_bytes(B0 + 16, POISON_WORD * 2)
        out.append(case("Plccn_" + tag, "w plccn b.0,b.16", [0xFF, 0xFD, 0x40, 0x44], {}, ram,
                        {"st": fc.Z if n == 0 else 0}, fc.ram_bytes(B0 + 16, fc.be(n, 4))))
    return out


def cases_entier():
    out = []
    for dbl in (False, True):
        for tag, v in (("TwoPointFive", Fraction(5, 2)), ("MinusTwoPointFive", Fraction(-5, 2)),
                       ("MinusTwo", Fraction(-2)), ("ThreeQuarters", Fraction(3, 4)),
                       ("MinusQuarter", Fraction(-1, 4)), ("One", Fraction(1)), ("Zero", Fraction(0)),
                       ("Big", Fraction(10099, 100)), ("MinusBig", Fraction(-10001, 100))):
            bits = fc.encode(v, dbl)
            result = math.floor(fc.decode(bits, dbl))      # SIMULA Entier: floor of the stored value
            nbytes = 8 if dbl else 4
            flags = (fc.Z if result == 0 else 0) | (fc.S if result < 0 else 0)
            out.append(case("Entier_%s_%s" % ("D" if dbl else "F", tag),
                            "%s ENTIER b.0,I1" % ("D" if dbl else "F"),
                            [0xFD, 0xC8 if dbl else 0xC7, 0x40, 0xD0],
                            {"i1": 0x12345678}, fc.ram_bytes(B0, fc.be(bits, nbytes)),
                            {"i1": result & 0xFFFFFFFF, "st": flags}, []))
    return out


def cases_system():
    out = []
    # the assembler writes DDIRT as 0xFE1E (the NDIX-C tables); 0xFFFA is the manual's opcode
    out.append(case("Ddirt_FFFA", "ddirt", [0xFF, 0xFA], {}, [], {}, []))
    out.append(case("Clinit_Privileged", "clinit", [0xFF, 0x1E], {}, [], {}, [], st=PIA))
    out.append(case("Clinit_NotPrivileged", "clinit", [0xFF, 0x1E], {}, [], {}, [], trap=ILLEGAL))
    out.append(case("Clread_Decodes", "clread", [0xFF, 0x1F], {}, [], {}, []))
    for n in range(1, 5):
        out.append(case("Rext_W%d_NotPrivileged" % n, "w%d rext b.0" % n, [0xFF, 0xE8 + n - 1, 0x40],
                        {}, fc.ram_bytes(B0, [0, 0, 0, 0]), {}, [], trap=ILLEGAL))
        out.append(case("Wext_W%d_NotPrivileged" % n, "w%d wext b.0" % n, [0xFF, 0xEC + n - 1, 0x40],
                        {}, fc.ram_bytes(B0, [0, 0, 0, 0]), {}, [], trap=ILLEGAL))
    return out


def all_cases():
    return cases_plccn() + cases_entier() + cases_system()


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "test/nd500-conformance.json"
    with open(path) as f:
        data = json.load(f)
    data = [c for c in data if not c["name"].startswith(PREFIX)]
    fresh = all_cases()
    names = set(c["name"] for c in fresh)
    assert len(names) == len(fresh), "duplicate case names"
    data += fresh
    with open(path, "w") as f:
        f.write(json.dumps(data, indent=2))
    print("added %d cases, file now has %d" % (len(fresh), len(data)))


if __name__ == "__main__":
    main()
