#!/usr/bin/env python3
"""Write docs/ND5000-87-EXTENSION-INSTRUCTIONS.md.

Source of truth: the ND-500 Reference Manual (ND-05.009.4, June 1987 edition),
docs/ND-05.009.4 EN ND-500 Reference Manual.md. The manual labels every
instruction that came with the ND-5000 with "('87 extension)" in its section
heading and says they also run on the ND-500/1 and ND-500/2.

The script
  1. finds every section whose heading carries that label,
  2. reads the opcodes from the section's code table (the octal column is used
     when there is one; the hex column of the manual has misprints),
  3. compares them with docs/instructions/instructions.json,
  4. computes the counts (ND-500 base, '87 extension, ND-5000 total),
  5. copies the manual text of each section into the document.

Nothing in the document is typed by hand except the introduction.

Usage: python3 tools/gen_87_extension_doc.py          writes the document
       python3 tools/gen_87_extension_doc.py --check  exit 1 if it is out of date
"""
import json
import os
import re
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
MANUAL = os.path.join(ROOT, "docs/ND-05.009.4 EN ND-500 Reference Manual.md")
OUT = os.path.join(ROOT, "docs/ND5000-87-EXTENSION-INSTRUCTIONS.md")

HEAD = re.compile(r"^(#+)\s*(\d+(?:\.\d+)*)\s+(.*)$")
LABEL = re.compile(r"^(.*?)\s+-\s+(.*?)\s*\('87 extension\)\s*$")
OCT = re.compile(r"^(\d{3,6})B(\+\(?n-1\)?)?$")
HEX = re.compile(r"^([0-9A-FO]{2,6})H?(\+\(?n-1\)?)?$")
JUNK = re.compile(r"^(Norsk Data ND-05\.009\.4 EN|---|## Page \d+|# ND-500 Reference Manual|## [A-Z][A-Z ,&/'-]+)$")


def expand(base, plus):
    return [base + k for k in range(4)] if plus else [base]


def section_opcodes(lines):
    """(set of opcodes, mnemonic rows, notes) from the code table of a section."""
    ops, rows, notes = set(), [], []
    for l in lines:
        if not l.startswith("|"):
            continue
        cells = [c.strip() for c in l.strip().strip("|").split("|")]
        if len(cells) < 3 or set("".join(cells)) <= set("-: "):
            continue
        if re.search(r"Assembly notation|Hex code|Octal code|^Condition", " ".join(cells), re.I):
            continue
        oct_v = hex_v = None
        for c in cells[1:]:
            m = OCT.match(c.replace(" ", ""))
            if m:
                oct_v = expand(int(m.group(1), 8), m.group(2))
        for c in cells[1:]:
            c2 = c.replace(" ", "")
            m = HEX.match(c2)
            if m and not OCT.match(c2) and not re.match(r"^\d+$", c2) or (m and c2.endswith("H")):
                hex_v = expand(int(m.group(1).replace("O", "0"), 16), m.group(2))
        if oct_v is None and hex_v is None:
            continue
        use = oct_v if oct_v is not None else hex_v
        if oct_v is not None and hex_v is not None and oct_v != hex_v:
            notes.append(f"{cells[0]}: hex column gives {', '.join('0x%X' % o for o in hex_v)}, octal column gives "
                         f"{', '.join('0x%X' % o for o in oct_v)}; octal used")
        ops.update(use)
        rows.append(cells[0])
    return ops, rows, notes


def main():
    with open(MANUAL, encoding="utf-8", errors="replace") as fh:
        L = fh.read().split("\n")
    with open(os.path.join(ROOT, "docs/instructions/instructions.json"), encoding="utf-8") as fh:
        items = json.load(fh)["instructions"]

    # --- the data side
    by_mn = {}
    for it in items:
        by_mn.setdefault(it["mnemonic"].lower(), []).append(it)
    all_fn = {it["functionName"] for it in items}
    all_ops = {int(it["opcode"], 16) for it in items}

    # --- the manual side: labelled sections
    starts = []
    seen = set()
    for n, l in enumerate(L):
        m = HEAD.match(l)
        if not m:
            continue
        lm = LABEL.match(m.group(3))
        if lm:
            # the same heading can also appear in the manual's table of contents; the later one is the section
            starts = [x for x in starts if x[1] != m.group(2)]
            starts.append((n, m.group(2), lm.group(1).strip(), lm.group(2).strip()))
    numbered = [n for n, l in enumerate(L) if HEAD.match(l)]

    def toc_page(sec):
        for l in L[:760]:
            if re.match(r"^\|\s*" + re.escape(sec) + r"\s*(\||\s)", l):
                m = re.search(r"\|\s*(\d{1,3})\s*\|\s*$", l)
                if m:
                    return int(m.group(1))
        return None

    secs = []
    for n, sec, name, desc in starts:
        end = next((x for x in numbered if x > n), len(L))
        body = L[n + 1:end]
        # the last section of a chapter runs into the scanned page footer and the next chapter heading
        for q, l in enumerate(body):
            if l.startswith("![scanned page]") or re.match(r"^#+\s*CHAPTER\b", l):
                body = body[:q]
                break
        ops, rows, notes = section_opcodes(body)
        mn = name.split()[-1].lower()
        jit = by_mn.get(mn, [])
        if not jit and ops:
            # the data may spell the mnemonic differently (CAD is "cad:=" in the data)
            jit = [i for i in items if int(i["opcode"], 16) in ops]
        jops = {int(i["opcode"], 16) for i in jit}
        if not jit:
            status = "missing from instructions.json"
        elif ops and ops == jops:
            status = "in instructions.json, same opcodes"
        elif ops and ops < jops:
            status = "in instructions.json, with the manual's opcodes and also " + \
                ", ".join("0x%X" % x for x in sorted(jops - ops))
        elif ops:
            status = "in instructions.json, DIFFERENT opcodes"
        else:
            status = "in instructions.json, manual opcodes not read by the script"
        secs.append(dict(sec=sec, name=name, mn=mn, desc=desc, ops=ops, rows=rows, notes=notes, body=body,
                         jit=jit, jops=jops, status=status, page=toc_page(sec),
                         fn=jit[0]["functionName"] if jit else None, cls=jit[0]["class"] if jit else None))

    fn87 = {s["fn"] for s in secs if s["fn"]}
    # the data tags the '87 entries itself (field "extension"); the counts come from the tag,
    # and the tag must agree with the manual's labels
    e87 = [it for it in items if it.get("extension") == "87"]
    ebase = [it for it in items if it.get("extension") != "87"]
    tag_fn = {it["functionName"] for it in e87}
    if tag_fn != fn87:
        sys.exit("the extension tag in instructions.json disagrees with the manual labels: "
                 "only tagged: %s; only in the manual and found in the data: %s"
                 % (sorted(tag_fn - fn87), sorted(fn87 - tag_fn)))
    ops87_data = {int(i["opcode"], 16) for i in e87}
    opsbase = {int(i["opcode"], 16) for i in ebase}
    ops87_manual = set()
    unread = 0
    for s in secs:
        if s["ops"]:
            ops87_manual |= s["ops"]
        elif s["jops"]:
            ops87_manual |= s["jops"]
        else:
            unread += 1
    total_ops_5000 = len(opsbase | ops87_manual)

    def doc():
        o = []
        w = o.append
        w("<!-- generated by tools/gen_87_extension_doc.py from the ND-500 Reference Manual and instructions.json; edit the script, not this file -->")
        w("")
        w("# ND-5000 '87 extension instructions")
        w("")
        w("The ND-500 Reference Manual (ND-05.009.4, June 1987 edition) says the ND-5000 uses the same instructions as the ND-500, "
          "and that a number of new instructions were introduced with the ND-5000. These also run on ND-500/1 and ND-500/2 CPUs. "
          "The manual labels each of them **('87 extension)** in its section heading. This page lists all of them, "
          "with the manual's own text for each, and says which are already in "
          "[instructions.json](instructions/instructions.json) and which are not.")
        w("")
        w("## Counts")
        w("")
        w("Counted by `tools/gen_87_extension_doc.py` from the manual's labels and `instructions.json`. "
          "\"Opcode entries\" are the entries of `instructions.json` (what the emulator's tables call variants); "
          "\"distinct opcodes\" count each opcode value once, because some entries share an opcode.")
        w("")
        w("| | Instructions | Opcode entries | Distinct opcodes |")
        w("|---|---:|---:|---:|")
        w(f"| ND-500 base (not labelled '87), in instructions.json | {len(all_fn) - len(fn87)} | {len(ebase)} | {len(opsbase)} |")
        w(f"| '87 extension, already in instructions.json | {len(fn87)} | {len(e87)} | {len(ops87_data)} |")
        w(f"| All of instructions.json today | {len(all_fn)} | {len(items)} | {len(all_ops)} |")
        w(f"| '87 extension according to the manual | {len(secs)} | not counted | {len(ops87_manual)} (+ {unread} instructions whose opcodes the script could not read) |")
        w(f"| ND-5000 total according to the manual (base + all '87) | {len(all_fn) - len(fn87) + len(secs)} | not counted | {total_ops_5000} (+ {unread} unread) |")
        w("")
        w("The ND-500 base row assumes `instructions.json` holds every non-'87 instruction of the manual. "
          "That was not checked here; only the '87 instructions were.")
        w("")
        w("## The instructions")
        w("")
        w("| Section | Instruction | What it does | Manual page | Opcodes in the manual | Status in instructions.json |")
        w("|---|---|---|---:|---|---|")
        for s in secs:
            ops = ", ".join("0x%X" % x for x in sorted(s["ops"])) if s["ops"] else "not read"
            if len(s["ops"]) > 4:
                ops = "0x%X to 0x%X" % (min(s["ops"]), max(s["ops"]))
            anchor = "s" + s["sec"].replace(".", "-")
            w(f"| {s['sec']} | [{s['name']}](#{anchor}) | {s['desc']} | {s['page'] or ''} | {ops} | {s['status']} |")
        w("")
        notes = [f"- {s['name']} ({s['sec']}): {x}" for s in secs for x in s["notes"]]
        if notes:
            w("Misprints in the manual's code tables found by the script:")
            w("")
            for x in notes:
                w(x)
            w("")
        w("## The manual text for each instruction")
        w("")
        w("Copied from the manual's Markdown. Page headers and footers are dropped; the manual's tables are as printed.")
        w("")
        for s in secs:
            anchor = "s" + s["sec"].replace(".", "-")
            w(f'<a id="{anchor}"></a>')
            w("")
            w(f"### {s['sec']} {s['name']} - {s['desc']}")
            w("")
            w(f"Status in instructions.json: **{s['status']}**." +
              (f" Class in the data: {s['cls']}." if s["cls"] else ""))
            w("")
            for l in s["body"]:
                if JUNK.match(l.strip()):
                    continue
                if l.startswith("#"):
                    l = "####" + l.lstrip("#")
                w(l.rstrip())
            w("")
        # collapse runs of blank lines
        text = "\n".join(o)
        text = re.sub(r"\n{3,}", "\n\n", text).rstrip() + "\n"
        return text

    text = doc()
    if "--check" in sys.argv:
        cur = open(OUT, encoding="utf-8").read() if os.path.isfile(OUT) else ""
        sys.exit(0 if cur == text else 1)
    with open(OUT, "w", encoding="utf-8") as fh:
        fh.write(text)
    print(f"gen_87_extension_doc: {len(secs)} '87 instructions; {sum(1 for s in secs if s['status'].startswith('missing'))} missing, "
          f"{sum(1 for s in secs if 'DIFFERENT' in s['status'])} different opcodes, {unread} unread; wrote {OUT}", file=sys.stderr)
    for s in secs:
        print(f"  {s['sec']:8} {s['name']:10} ops={len(s['ops'])} {s['status']}", file=sys.stderr)


if __name__ == "__main__":
    main()
