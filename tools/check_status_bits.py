#!/usr/bin/env python3
"""Check the "Data Status Bits" section of every instruction page against the
ND-500 Reference Manual.

Sources compared, per instruction:
  manual   docs/ND-05.009.4 EN ND-500 Reference Manual.md. Each instruction
           block lists the opcode table, then "Data status bits". The manual's
           own rule (section 6.5.1): bits that are not mentioned are reset.
  page     docs/instructions/asm/<name>.md, section "## Data Status Bits".
  emulator src/cpu/instructions/<CLASS>/<FunctionName>.c, flags written as a
           literal ND500_FLAG_Z/S/O/C. Approximate: a file that sets flags
           through a helper shows none here. Printed as information only.

Findings (per page flag Z S O C):
  UNAFFECTED_BUT_RESET   page says "not affected", manual does not mention the
                         bit, so by the manual's rule it is reset
  SET_NOT_IN_MANUAL      page describes the bit as set/changed, manual does not
                         mention it
  CLEARED_BUT_IN_MANUAL  page says always cleared, manual lists the bit
  MISSING_IN_PAGE        manual lists the bit, page has no line for it
  NO_MANUAL_BLOCK        no manual block found for the instruction's opcodes

Usage: python3 tools/check_status_bits.py [REPORT.tsv]
"""
import collections
import json
import os
import re
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
MANUAL = os.path.join(ROOT, "docs/ND-05.009.4 EN ND-500 Reference Manual.md")
FLAGS = ("Z", "S", "O", "C")
DATA = {"Z", "C", "S", "O", "IVO", "DZ", "FU", "FO", "BO"}


def hexcodes(cell):
    """Opcodes named by a 'Hex code' table cell, e.g. 084H+(n-1) or 0FD44H."""
    m = re.match(r"^\s*([0-9A-FO]{2,5})H(\+\(n-1\))?\s*$", cell)
    if not m:
        return []
    v = int(m.group(1).replace("O", "0"), 16)
    return [v + k for k in range(4)] if m.group(2) else [v]


def manual_blocks():
    """[(set of opcodes, set of flags mentioned, text)] in manual order."""
    with open(MANUAL, encoding="utf-8", errors="replace") as fh:
        lines = fh.read().split("\n")
    marker = [i for i, l in enumerate(lines)
              if re.search(r"data status bits", l, re.I) and not l.startswith("|")
              and "6.5.1" not in l and not l.startswith("- ")
              or re.match(r"^\**Data status bits:?\**", l, re.I)]
    blocks = []
    prev = 0
    for i in sorted(set(marker)):
        ops = set()
        for l in lines[prev:i]:
            if l.startswith("|"):
                cells = [c.strip() for c in l.strip().strip("|").split("|")]
                if len(cells) >= 3:
                    ops.update(hexcodes(cells[2]))
        body = []
        for l in lines[i:i + 14]:
            if body and re.match(r"^(\*\*(Trap|Terminating|Example)|---|#|Norsk Data)", l):
                break
            body.append(l)
        text = " ".join(body)
        flags = set(re.findall(r"->\s*(IVO|DZ|FU|FO|BO|[ZCSO])\b", text))
        for l in body:
            if l.startswith("|"):
                cells = [c.strip() for c in l.strip().strip("|").split("|")]
                if cells and cells[-1] in DATA:
                    flags.add(cells[-1])
        blocks.append((ops, flags, text))
        prev = i + 1
    return blocks


def page_claims(path):
    """{flag: 'unaffected' | 'clear' | 'set'} from the page's status section."""
    with open(path, encoding="utf-8", errors="replace") as fh:
        t = fh.read()
    m = re.search(r"^## Data Status Bits\s*\n(.*?)(?=^## |^---|\Z)", t, re.S | re.M)
    if not m:
        return None
    claims = {}
    for l in m.group(1).split("\n"):
        mm = re.match(r"^\s*[-*]\s*\*\*([A-Z]+)\b[^*]*\*\*:?\s*(.*)$", l)
        if not mm or mm.group(1) not in FLAGS:
            continue
        txt = mm.group(2).lower()
        if re.search(r"not affected|unaffected|unchanged|not modified", txt):
            c = "unaffected"
        elif re.match(r"^(always )?(cleared|clear|reset|0)\b", txt) and "set" not in txt.replace("reset", ""):
            c = "clear"
        else:
            c = "set"
        claims[mm.group(1)] = c
    return claims


def emulator_flags(cls, fn):
    d = os.path.join(ROOT, "src/cpu/instructions", cls)
    for name in (fn + ".c", fn.capitalize() + ".c"):
        p = os.path.join(d, name)
        if os.path.isfile(p):
            with open(p, encoding="utf-8", errors="replace") as fh:
                t = fh.read()
            return set(re.findall(r"ND500_FLAG_([ZSOC])\b", t))
    return None


def main():
    with open(os.path.join(ROOT, "docs/instructions/instructions.json"), encoding="utf-8") as fh:
        items = json.load(fh)["instructions"]
    fns = collections.OrderedDict()
    for it in items:
        e = fns.setdefault(it["functionName"], {"mn": it["mnemonic"], "cls": it["class"], "ops": set()})
        e["ops"].add(int(it["opcode"], 16))
    blocks = manual_blocks()
    by_op = collections.defaultdict(list)
    for ops, flags, text in blocks:
        for o in ops:
            by_op[o].append((flags, text))

    asm = os.path.join(ROOT, "docs/instructions/asm")
    names = {f for f in os.listdir(asm)}
    special = {"+": "add", "-": "sub", "*": "mul", "/": "div", ":=": "assignto", "=:": "assignfrom",
               "b:=": "b_assignto", "b=:": "b_assignfrom", "r:=": "r_assignto", "r=:": "r_assignfrom"}
    rows = []
    for fn, e in fns.items():
        stem = special.get(e["mn"].lower(), fn.lower())
        page = os.path.join(asm, stem + ".md")
        if not os.path.isfile(page):
            page = os.path.join(asm, e["mn"].lower().replace(":", "_") + ".md")
        if not os.path.isfile(page):
            continue
        claims = page_claims(page)
        if claims is None:
            continue
        found = [b for o in e["ops"] for b in by_op.get(o, [])]
        emu = emulator_flags(e["cls"], fn)
        if not found:
            rows.append((e["mn"], "NO_MANUAL_BLOCK", "-", "-", os.path.relpath(page, ROOT)))
            continue
        mflags = set().union(*[f for f, _ in found])
        unaff_text = any(re.search(r"unaffected|not affected", t, re.I) for _, t in found)
        for f in FLAGS:
            c = claims.get(f)
            inman = f in mflags
            if c == "unaffected" and not inman and not unaff_text:
                kind = "UNAFFECTED_BUT_RESET"
            elif c == "set" and not inman:
                kind = "SET_NOT_IN_MANUAL"
            elif c == "clear" and inman:
                kind = "CLEARED_BUT_IN_MANUAL"
            elif c is None and inman:
                kind = "MISSING_IN_PAGE"
            else:
                continue
            rows.append((e["mn"], kind, f, "manual:" + ",".join(sorted(mflags & set(FLAGS))) +
                         " emulator:" + (",".join(sorted(emu)) if emu is not None else "?"),
                         os.path.relpath(page, ROOT)))
    cnt = collections.Counter(r[1] for r in rows)
    pages = len({r[4] for r in rows if r[1] != "NO_MANUAL_BLOCK"})
    print(f"instructions with a page and a status section: checked against {len(blocks)} manual blocks")
    for k, v in sorted(cnt.items()):
        print(f"  {k}: {v}")
    print(f"  pages with at least one finding (not counting NO_MANUAL_BLOCK): {pages}")
    out = sys.argv[1] if len(sys.argv) > 1 else None
    if out:
        with open(out, "w", encoding="utf-8") as fh:
            fh.write("mnemonic\tfinding\tflag\tsources\tpage\n")
            for r in sorted(rows):
                fh.write("\t".join(r) + "\n")
        print("report:", out)


if __name__ == "__main__":
    main()
