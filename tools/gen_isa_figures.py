#!/usr/bin/env python3
"""Generate the four ND-500 instruction-set figures as standalone SVG.

  docs/figures/nd500-1-instruction-set.svg     class map, every mnemonic
  docs/figures/nd500-2-anatomy.svg             how one instruction is encoded (real bytes)
  docs/figures/nd500-3-address-code-byte.svg   the address-code byte 0x00-0xFF
  docs/figures/nd500-4-effective-address.svg   one drawing per addressing mode

Run from anywhere:  python3 tools/gen_isa_figures.py [outdir]
Paths are derived from this file's location; the default outdir is docs/figures.

Facts come from:
  docs/instructions/instructions.json             252 instr / 1097 variants / classes / opcodes
  test/nd500-conformance.json                     real assembly -> byte strings
  docs/ND-05.009.4 EN ND-500 Reference Manual.md  pages 97-101: address codes, ea formulas
The script asserts the counts and the byte strings it draws.
"""
import json, os, sys, collections, html

REPO = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(REPO, "docs", "figures")
os.makedirs(OUT, exist_ok=True)

# ---- palette (Ronny's WCAG AA set) -----------------------------------------
BLUE   = ("#E3F2FD", "#0D47A1")
TEAL   = ("#E0F7FA", "#00838F")
GREEN  = ("#E8F5E9", "#2E7D32")
PURPLE = ("#F3E5F5", "#7B1FA2")
ORANGE = ("#FFF3E0", "#E65100")
DTEAL  = ("#E0F2F1", "#00695C")
GREY   = ("#F5F5F4", "#616161")
INK    = "#1F2328"
MUTED  = "#5B6470"
RULE   = "#D7DBE0"
SANS = "Inter, 'Segoe UI', Helvetica, Arial, sans-serif"
MONO = "'JetBrains Mono', 'DejaVu Sans Mono', Menlo, Consolas, monospace"
DASH = ' stroke-dasharray="6 4"'
HATCH = ('<defs><pattern id="pi" width="6" height="6" patternUnits="userSpaceOnUse" patternTransform="rotate(45)">'
         '<line x1="0" y1="0" x2="0" y2="6" stroke="#FFFFFF" stroke-width="2.5" stroke-opacity="0.9"/></pattern>'
         f'<marker id="arr" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">'
         f'<path d="M0,0 L10,5 L0,10 z" fill="{INK}"/></marker></defs>')

def esc(s): return html.escape(s, quote=True)

def head(w, h, label):
    return [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="{w}" height="{h}" role="img" '
            f'aria-label="{esc(label)}" font-family="{SANS}">', HATCH, f'<rect width="{w}" height="{h}" fill="#FFFFFF"/>']

def title(s, t, sub, y=52):
    s.append(f'<text x="40" y="{y}" font-size="30" font-weight="700" fill="{INK}">{esc(t)}</text>')
    s.append(f'<text x="40" y="{y+32}" font-size="16" fill="{MUTED}">{esc(sub)}</text>')

def source(s, w, h, txt):
    s.append(f'<text x="{w-40}" y="{h-14}" text-anchor="end" font-size="11" fill="{MUTED}">{esc(txt)}</text>')

def save(name, s):
    open(os.path.join(OUT, name), "w").write("\n".join(s) + "\n</svg>")

# ---- data from instructions.json -------------------------------------------
d = json.load(open(os.path.join(REPO, "docs/instructions/instructions.json")))
items = d["instructions"]
assert d["totalInstructions"] == 1097 == len(items)
cls_fn = collections.defaultdict(dict)
cls_var = collections.Counter()
by_op = {}
for it in items:
    cls_fn[it["class"]].setdefault(it["functionName"], it["mnemonic"])
    cls_var[it["class"]] += 1
    by_op[int(it["opcode"], 16)] = it
n_instr = sum(len(v) for v in cls_fn.values())
assert n_instr == 252, n_instr
one_byte = sum(1 for it in items if int(it["opcode"], 16) < 0x100)
assert by_op[0x54]["mnemonic"] == "+" and by_op[0x6C]["mnemonic"] == "*"
assert by_op[0x20]["mnemonic"] == "=:" and by_op[0xFC40]["mnemonic"] == "-"

# ---- real byte strings from the conformance corpus --------------------------
corpus = json.load(open(os.path.join(REPO, "test/nd500-conformance.json")))
def corpus_bytes(asm):
    for t in corpus:
        if t["assembly"].lower() == asm.lower():
            return t["bytes"]
    raise SystemExit("not in corpus: " + asm)
EX = [  # (assembly as the manual writes it, bytes, split into fields)
    ("W1 + $0",      corpus_bytes("w1 + $0")),        # 54 00
    ("W1 + $1000",   corpus_bytes("w1 + $1000")),     # 54 CE 03 E8
    ("W1 =: B.0",    corpus_bytes("W1 =: b.0")),      # 20 40
    ("H1 - $128",    corpus_bytes("h1 - $128")),      # FC 40 CE 00 80
]
assert EX[0][1] == [0x54, 0x00] and EX[1][1] == [0x54, 0xCE, 0x03, 0xE8]
assert EX[2][1] == [0x20, 0x40] and EX[3][1] == [0xFC, 0x40, 0xCE, 0x00, 0x80]

LINES = {
 "MOVE": [":=  =:  move  bmove  swap  clr  clrk  stz",
          "b:=  b=:  r:=  r=:  l:=  l=:  tos:=  tos=:",
          "a1..a4 :=/=:   e1..e4 :=/=:",
          "ll hl tha ps st1 cad :=/=:   ced p =:",
          "ote1 ote2 :=/=:   mte1 mte2 cte1 cte2 temm1 temm2 =:"],
 "ARITHMETIC": ["+  -  *  /  abs  neg  incr  decr  rem",
                "add2 add3 sub2 sub3 mul2 mul3 mul4",
                "div2 div3 div4  addc subc  umul udiv",
                "mulad  axi  ixi  psum",
                "padd psub pmpy ppack pupack (+r forms)"],
 "SYSTEM": ["laddr rladdr bladdr phyladr  lind cind",
            "int intr  dmon pmon  dmof pmof  dcc pcc",
            "lcntxt scntxt  lregbl sregbl  sloca",
            "rdus wdus  rpgu zpgu cpgu  rwip zwip cwip",
            "rphs wphs  dctsb pctsb  ddirt  freeb  svers  tutti"],
 "FLOAT_MATH": ["sin cos tan  asin acos atan atan2",
                "sqrt exp  alog alog2 alog10  poly",
                "wconv hconv byconv biconv dconv fconv",
                "wconr hconr byconr fconr  pwconv wpconv"],
 "BRANCH": ["go  jumpg jumps",
            "if=go if><go if<go if<=go if>go if>=go",
            "if<<go if<<=go if>>go if>>=go",
            "if-kgo if-stgo ifkgo ifstgo",
            "loop loopi loopd"],
 "CALL": ["call callg chain",
          "entb entd entf entfn entm",
          "ents entsn entt",
          "ret retb retbk retd retk rett ifkret"],
 "STRING": ["smove smovn smvtr smvtu smvun smvwh",
            "sfill sfilln  sscan sskip sspan sspar",
            "smatch schpar scopa scopt scotr scpuno"],
 "CONTROL": ["noop bp init tset", "set1 sete setk clte solo"],
 "BITFIELD": ["getbi putbi setbi clebi", "getbf putbf getb"],
 "COMPARE": ["comp comp2 test", "pcomp scomp"],
 "LOGICAL": ["and or xor", "inv invc"],
 "SHIFT": ["shl shr sha", "pshift pshiftr"],
 "IO": ["riom"],
}
CLASS_LABEL = {"FLOAT_MATH": "FLOAT MATH"}
CLASS_COLOR = {
 "MOVE": BLUE, "STRING": DTEAL,
 "ARITHMETIC": TEAL, "FLOAT_MATH": TEAL, "LOGICAL": TEAL, "SHIFT": TEAL, "BITFIELD": TEAL, "COMPARE": TEAL,
 "BRANCH": GREEN, "CALL": GREEN, "CONTROL": GREEN,
 "SYSTEM": PURPLE, "IO": PURPLE,
}

def wrap(tokens_lines, max_chars):
    out, cur = [], ""
    for ln in tokens_lines:
        for tok in ln.split(" "):
            if not tok: continue
            if cur and len(cur) + 1 + len(tok) > max_chars:
                out.append(cur); cur = tok
            else:
                cur = (cur + " " + tok) if cur else tok
    if cur: out.append(cur)
    return out

# =============================================================================
# IMAGE 1: instruction-set map
# =============================================================================
W1, H1 = 1400, 780
rows = [["MOVE", "ARITHMETIC", "SYSTEM"],
        ["FLOAT_MATH", "BRANCH", "CALL", "STRING"],
        ["CONTROL", "BITFIELD", "COMPARE", "LOGICAL", "SHIFT", "IO"]]
X0, Y0, MAPW, MAPH, GAP = 40, 118, 1320, 580, 8
row_sum = [sum(len(cls_fn[c]) for c in r) for r in rows]
total = sum(row_sum)
s = head(W1, H1, "ND-500 instruction set: 241 instructions in 13 classes, tile area proportional to instruction count")
title(s, "ND-500 instruction set",
      f"{n_instr} instructions in 13 classes, {len(items)} opcode variants once the data type is folded in. Tile area is proportional to the number of instructions.")
y = Y0
min_w = 64
for r, rs in zip(rows, row_sum):
    h = (MAPH - GAP * (len(rows) - 1)) * rs / total
    ws = [(MAPW - GAP * (len(r) - 1)) * len(cls_fn[c]) / rs for c in r]
    for i in range(len(ws)):
        if ws[i] < min_w:
            ws[ws.index(max(ws))] -= (min_w - ws[i]); ws[i] = min_w
    x = X0
    for c, w in zip(r, ws):
        fill, stroke = CLASS_COLOR[c]
        n = len(cls_fn[c]); v = cls_var[c]
        s.append(f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" rx="6" fill="{fill}" stroke="{stroke}" stroke-width="2"/>')
        label = CLASS_LABEL.get(c, c)
        if w < 90:
            s.append(f'<text transform="translate({x+w/2-8:.1f},{y+h/2:.1f}) rotate(-90)" text-anchor="middle" font-size="14" font-weight="700" fill="{stroke}">{label} {n}</text>')
            s.append(f'<text transform="translate({x+w/2+12:.1f},{y+h/2:.1f}) rotate(-90)" text-anchor="middle" font-size="12" font-family="{MONO}" fill="{INK}">{LINES[c][0]}</text>')
        else:
            tall = h > 200; mid = 120 < h <= 200
            big = 24 if tall else (19 if mid else 15)
            cnt_fs = 13 if w > 260 else 11
            cnt = f'{n} instr · {v} variants' if w > 260 else f'{n} · {v}'
            s.append(f'<text x="{x+12:.1f}" y="{y+big+6:.1f}" font-size="{big}" font-weight="700" fill="{stroke}">{label}</text>')
            s.append(f'<text x="{x+w-10:.1f}" y="{y+big+6:.1f}" text-anchor="end" font-size="{cnt_fs}" fill="{MUTED}">{cnt}</text>')
            fs = 17 if tall else (13 if mid else 11)
            lead = fs + 9 if tall else fs + 5
            ty = y + big + 14 + fs
            max_chars = int((w - 24) / (fs * 0.62))
            for ln in wrap(LINES[c], max_chars):
                if ty > y + h - 6: break
                s.append(f'<text x="{x+12:.1f}" y="{ty:.1f}" font-size="{fs}" font-family="{MONO}" fill="{INK}">{esc(ln)}</text>')
                ty += lead
        x += w + GAP
    y += h + GAP
s.append(f'<text x="{X0}" y="{H1-38}" font-size="12" fill="{MUTED}">Each mnemonic is written once per data type it supports (BI, BY, H, W, F, D), which is how {n_instr} instructions become {len(items)} opcode variants. Counts: instructions · variants.</text>')
source(s, W1, H1, "Sources: ND-05.009.4 ND-500 Reference Manual; nd500x instructions.json. Classes follow the nd500x emulator grouping.")
save("nd500-1-instruction-set.svg", s)

# =============================================================================
# IMAGE 2: anatomy of one instruction
# =============================================================================
W2, H2 = 1400, 760
s = head(W2, H2, "Anatomy of an ND-500 instruction: opcode, then per operand a prefix, an address-code byte and a data part; five real byte strings")
title(s, "Anatomy of one instruction",
      "An instruction is an opcode followed by zero or more operand specifiers. Every operand specifier starts with one address-code byte.")

# general shape
gy = 130
s.append(f'<text x="40" y="{gy}" font-size="15" font-weight="700" fill="{INK}">General shape</text>')
boxes = [("opcode", "1 byte 0x00–0xFB, or 2 bytes 0xFC–0xFF xx", 330, ORANGE, False),
         ("prefix", "0, 1 or 2 bytes: ALT, DESC", 250, GREY, True),
         ("address code", "1 byte, picks the mode", 220, BLUE, False),
         ("data part", "0–8 bytes: displacement, address or constant", 380, PURPLE, False)]
bx = 40
for name, sub, w, col, dashed in boxes:
    s.append(f'<rect x="{bx}" y="{gy+16}" width="{w}" height="54" rx="5" fill="{col[0]}" stroke="{col[1]}" stroke-width="2"{DASH if dashed else ""}/>')
    s.append(f'<text x="{bx+12}" y="{gy+38}" font-size="15" font-weight="700" fill="{col[1]}">{name}</text>')
    s.append(f'<text x="{bx+12}" y="{gy+58}" font-size="12" fill="{INK}">{sub}</text>')
    bx += w + 10
# brace: operand specifier, repeated per operand
ox = 40 + 330 + 10; ow = 250 + 220 + 380 + 20
s.append(f'<path d="M{ox},{gy+84} v8 h{ow} v-8" fill="none" stroke="{INK}" stroke-width="1.5"/>')
s.append(f'<text x="{ox+ow/2}" y="{gy+110}" text-anchor="middle" font-size="13" fill="{INK}">one <tspan font-weight="700">operand specifier</tspan>, repeated once per operand (most instructions take 1, string and some arithmetic forms take 2 or 3)</text>')

# real examples
ey = gy + 160
s.append(f'<text x="40" y="{ey}" font-size="15" font-weight="700" fill="{INK}">Real byte strings</text>')
s.append(f'<text x="260" y="{ey}" font-size="12" fill="{MUTED}">first four from the nd500x conformance corpus; the last one is read off the manual’s address-code table</text>')
COLX = {"asm": 40, "op": 330, "code": 560, "data": 720, "note": 980}
hy = ey + 26
for key, lab in [("asm", "WRITTEN"), ("op", "OPCODE"), ("code", "ADDRESS CODE"), ("data", "DATA PART"), ("note", "WHAT THE ADDRESS CODE SAYS")]:
    s.append(f'<text x="{COLX[key]}" y="{hy}" font-size="11" font-weight="700" fill="{MUTED}" letter-spacing="0.6">{lab}</text>')
s.append(f'<line x1="40" y1="{hy+8}" x2="{W2-40}" y2="{hy+8}" stroke="{RULE}"/>')

def bytebox(x, y, txt, col, w=44):
    s.append(f'<rect x="{x}" y="{y}" width="{w}" height="32" rx="4" fill="{col[0]}" stroke="{col[1]}" stroke-width="2"/>')
    s.append(f'<text x="{x+w/2}" y="{y+22}" text-anchor="middle" font-size="16" font-family="{MONO}" font-weight="700" fill="{col[1]}">{txt}</text>')
    return x + w + 6

ROWS = [
  # asm, opcode bytes, code byte, data bytes, note lines
  ("W1 + $0",          [0x54], 0x00, [],               ["00 000000: short constant, value 0 inside the byte", "opcode 54 = W1 +  (W picks word size and register I1)"]),
  ("W1 + $1000",       [0x54], 0xCE, [0x03, 0xE8],     ["CE: constant, halfword data part", "1000 = 03E8, big-endian"]),
  ("W1 =: B.0",        [0x20], 0x40, [],               ["01 000000: short local, B + 0*4", "opcode 20 = W1 =:  (store W1 to the operand)"]),
  ("H1 - $128",        [0xFC, 0x40], 0xCE, [0x00, 0x80], ["FC 40: two-byte opcode, H1 -", "128 does not fit the 6-bit short form, so CE + 2 bytes"]),
  ("W1 + B.12:B(R2)",  [0x54], 0xD5, [0x0C],           ["D5: local, post-indexed, byte displacement, index R2", "ea = B + 12 + 4·R2  (from the manual’s table, D4 + 1)"]),
]
ry = hy + 24
for asm, ops, code, data, notes in ROWS:
    s.append(f'<text x="{COLX["asm"]}" y="{ry+22}" font-size="18" font-family="{MONO}" fill="{INK}">{esc(asm)}</text>')
    x = COLX["op"]
    for b in ops: x = bytebox(x, ry, f"{b:02X}", ORANGE)
    bytebox(COLX["code"], ry, f"{code:02X}", BLUE)
    x = COLX["data"]
    if data:
        for b in data: x = bytebox(x, ry, f"{b:02X}", PURPLE)
    else:
        s.append(f'<text x="{COLX["data"]}" y="{ry+22}" font-size="12" fill="{MUTED}">none</text>')
    s.append(f'<text x="{COLX["note"]}" y="{ry+13}" font-size="12" fill="{INK}">{esc(notes[0])}</text>')
    s.append(f'<text x="{COLX["note"]}" y="{ry+29}" font-size="11.5" fill="{MUTED}">{esc(notes[1])}</text>')
    ry += 58
    s.append(f'<line x1="40" y1="{ry-12}" x2="{W2-40}" y2="{ry-12}" stroke="{RULE}"/>')

# data type note
ny = ry + 18
s.append(f'<text x="40" y="{ny}" font-size="15" font-weight="700" fill="{INK}">The data-type letter is part of the opcode</text>')
s.append(f'<text x="40" y="{ny+24}" font-size="13" fill="{INK}"><tspan font-family="{MONO}" font-weight="700">W1 +</tspan> is opcode 54, '
         f'<tspan font-family="{MONO}" font-weight="700">F1 +</tspan> is 58, <tspan font-family="{MONO}" font-weight="700">W1 *</tspan> is 6C: the type letter and the register number select the opcode byte, they are not a prefix.</text>')
s.append(f'<text x="40" y="{ny+46}" font-size="13" fill="{INK}"><tspan font-family="{MONO}" font-weight="700">BI BY H W</tspan> → integer registers I1–I4 (bit, byte, halfword, word)   '
         f'<tspan font-family="{MONO}" font-weight="700">F</tspan> → A1–A4 (32-bit float)   '
         f'<tspan font-family="{MONO}" font-weight="700">D</tspan> → A+E pair (64-bit float).   '
         f'{one_byte} variants have a one-byte opcode, {len(items)-one_byte} use the 0xFC–0xFF prefix byte.</text>')
source(s, W2, H2, "Sources: nd500x conformance corpus (byte strings), instructions.json (opcodes), ND-05.009.4 chapter 8 (address codes).")
save("nd500-2-anatomy.svg", s)

# =============================================================================
# IMAGE 3: the address-code byte
# =============================================================================
W3, H3 = 1000, 1180
s = head(W3, H3, "The ND-500 address-code byte laid out 0x00 to 0xFF, coloured by addressing mode")
title(s, "The address-code byte",
      "One byte per operand. Top two bits 00 / 01 / 10: one-byte short forms with 6 bits of data. Top bits 11: extended code, data part follows.")
GX, GY, CELL = 100, 170, 52
s.append(f'<text x="{GX}" y="{GY-48}" font-size="12" fill="{MUTED}">hatched = post-indexed, adds p·Rn   ·   dashed = prefix, another operand specifier follows</text>')
s.append(f'<text x="{GX}" y="{GY-32}" font-size="12" fill="{MUTED}">orange bar on a cell = one of the four bytes used in the anatomy figure: 00, 40, CE, D5</text>')
for i in range(16):
    s.append(f'<text x="{GX+i*CELL+CELL/2}" y="{GY-8}" text-anchor="middle" font-size="12" font-family="{MONO}" fill="{MUTED}">_{i:X}</text>')
for r in range(16):
    s.append(f'<text x="{GX-12}" y="{GY+r*CELL+CELL/2+4}" text-anchor="end" font-size="12" font-family="{MONO}" fill="{MUTED}">{r:X}_</text>')

def block(c0, c1, label, sub, col, pi=False, dashed=False, tall=None, sub2=None):
    row, col0 = c0 >> 4, c0 & 15
    n = c1 - c0 + 1
    x = GX + col0 * CELL; y = GY + row * CELL
    w = n * CELL; h = (tall or 1) * CELL
    s.append(f'<rect x="{x+1.5}" y="{y+1.5}" width="{w-3}" height="{h-3}" rx="4" fill="{col[0]}" stroke="{col[1]}" stroke-width="1.8"{DASH if dashed else ""}/>')
    if pi: s.append(f'<rect x="{x+1.5}" y="{y+1.5}" width="{w-3}" height="{h-3}" rx="4" fill="url(#pi)"/>')
    if tall:
        s.append(f'<text x="{x+w/2}" y="{y+h/2-14}" text-anchor="middle" font-size="22" font-weight="700" fill="{col[1]}">{esc(label)}</text>')
        s.append(f'<text x="{x+w/2}" y="{y+h/2+12}" text-anchor="middle" font-size="15" font-family="{MONO}" fill="{INK}">{esc(sub)}</text>')
        if sub2: s.append(f'<text x="{x+w/2}" y="{y+h/2+34}" text-anchor="middle" font-size="12" fill="{MUTED}">{esc(sub2)}</text>')
    else:
        fs1 = 12 if n >= 3 else 10
        fs2 = 10.5 if n >= 3 else 9
        s.append(f'<text x="{x+w/2}" y="{y+21}" text-anchor="middle" font-size="{fs1}" font-weight="700" fill="{col[1]}">{esc(label)}</text>')
        s.append(f'<text x="{x+w/2}" y="{y+37}" text-anchor="middle" font-size="{fs2}" font-family="{MONO}" fill="{INK}">{esc(sub)}</text>')

block(0x00, 0x0F, "CONSTANT, short", "00dddddd    operand = d", ORANGE, tall=4, sub2="d = 0..63 is the value itself; no data part")
block(0x40, 0x4F, "LOCAL, short", "01dddddd    ea = B + d*4", BLUE, tall=4, sub2="d = 0..63 words from the base register B")
block(0x80, 0x8F, "RECORD, short", "10dddddd    ea = R + d*4", DTEAL, tall=4, sub2="d = 0..63 words from the record register R")
block(0xC0, 0xC0, "—", "unused", GREY)
block(0xC1, 0xC3, "LOCAL", "B.d  :B :H :W", BLUE)
block(0xC4, 0xC4, "ABS", "addr", GREEN)
block(0xC5, 0xC7, "LOCAL INDIRECT", "IND(B.d)  :B :H :W", BLUE)
block(0xC8, 0xC8, "ALT", "prefix", GREY, dashed=True)
block(0xC9, 0xCB, "RECORD", "R.d  :B :H :W", DTEAL)
block(0xCC, 0xCF, "CONSTANT", ":D  :B  :H  :W/:F", ORANGE)
block(0xD0, 0xD3, "REGISTER", "R1  R2  R3  R4", PURPLE)
block(0xD4, 0xD7, "LOCAL P.I.", ":B   (R1..R4)", BLUE, pi=True)
block(0xD8, 0xDB, "LOCAL P.I.", ":H   (R1..R4)", BLUE, pi=True)
block(0xDC, 0xDF, "LOCAL P.I.", ":W   (R1..R4)", BLUE, pi=True)
block(0xE0, 0xE3, "ABSOLUTE P.I.", "addr  (R1..R4)", GREEN, pi=True)
block(0xE4, 0xE7, "LOCAL IND. P.I.", ":B   (R1..R4)", BLUE, pi=True)
block(0xE8, 0xEB, "LOCAL IND. P.I.", ":H   (R1..R4)", BLUE, pi=True)
block(0xEC, 0xEF, "LOCAL IND. P.I.", ":W   (R1..R4)", BLUE, pi=True)
block(0xF0, 0xF3, "DESC", "prefix  (R1..R4)", GREY, dashed=True)
block(0xF4, 0xF7, "PRE-INDEXED", "Rn.d  :B", TEAL)
block(0xF8, 0xFB, "PRE-INDEXED", "Rn.d  :H", TEAL)
block(0xFC, 0xFF, "PRE-INDEXED", "Rn.d  :W", TEAL)
# highlight the bytes used in image 2: 00, CE, 40, D5
for code in (0x00, 0x40, 0xCE, 0xD5):
    r, c = code >> 4, code & 15
    s.append(f'<rect x="{GX+c*CELL+5}" y="{GY+r*CELL+4}" width="{CELL-10}" height="6" rx="2" fill="{ORANGE[1]}"/>')
ny = GY + 16 * CELL + 30
s.append(f'<text x="{GX}" y="{ny}" font-size="12.5" fill="{INK}">Rows D–F: the low two bits name the index register (00 = R1 … 11 = R4); columns 4–7, 8–B, C–F are the :B, :H, :W displacement sizes.</text>')
s.append(f'<text x="{GX}" y="{ny+20}" font-size="12.5" fill="{INK}">Columns 0–3 of those rows are the odd ones out: REGISTER, ABSOLUTE P.I. and the DESC prefix.</text>')
s.append(f'<text x="{GX}" y="{ny+48}" font-size="12.5" fill="{INK}">Data part after the code:  :S none (inside the byte, unit = words)   :B 1 byte   :H 2 bytes   :W 4 bytes   address 4 bytes   :D constant 8 bytes.</text>')
s.append(f'<text x="{GX}" y="{ny+68}" font-size="12.5" fill="{INK}">Short, byte and halfword displacements are unsigned.</text>')
source(s, W3, H3, "Source: ND-05.009.4 ND-500 Reference Manual, chapter 8, Figure 29 and the 8.3 survey table (pages 97–101).")
save("nd500-3-address-code-byte.svg", s)

# =============================================================================
# IMAGE 4: effective-address ladder, one drawing per mode
# =============================================================================
W4, H4 = 1400, 1230
s = head(W4, H4, "How each ND-500 addressing mode forms its effective address, one drawing per mode")
title(s, "How the effective address is formed",
      "B = base register, R = record register, Rn = index register R1..R4, d = displacement (unsigned), a = address, p = bytes per element of the data type.")
# column heads
LX = {"name": 40, "asm": 250, "draw": 450, "codes": 1190}
hy = 128
for k, lab in [("name", "MODE"), ("asm", "WRITTEN AS"), ("draw", "HOW ea IS FORMED"), ("codes", "ADDRESS CODES")]:
    s.append(f'<text x="{LX[k]}" y="{hy}" font-size="11" font-weight="700" fill="{MUTED}" letter-spacing="0.6">{lab}</text>')
s.append(f'<line x1="40" y1="{hy+10}" x2="{W4-40}" y2="{hy+10}" stroke="{RULE}"/>')

RH = 72
def node(x, y, txt, col, w=None, hatch=False, dashed=False, mono=True):
    w = w or (len(txt) * 9.5 + 22)
    s.append(f'<rect x="{x}" y="{y-17}" width="{w:.0f}" height="34" rx="5" fill="{col[0]}" stroke="{col[1]}" stroke-width="1.8"{DASH if dashed else ""}/>')
    if hatch: s.append(f'<rect x="{x}" y="{y-17}" width="{w:.0f}" height="34" rx="5" fill="url(#pi)"/>')
    s.append(f'<text x="{x+w/2:.0f}" y="{y+5}" text-anchor="middle" font-size="14" font-family="{MONO if mono else SANS}" font-weight="700" fill="{col[1]}">{esc(txt)}</text>')
    return x + w
def arrow(x0, x1, y, label=None):
    s.append(f'<line x1="{x0}" y1="{y}" x2="{x1}" y2="{y}" stroke="{INK}" stroke-width="1.6" marker-end="url(#arr)"/>')
    if label:
        s.append(f'<text x="{(x0+x1)/2:.0f}" y="{y-7}" text-anchor="middle" font-size="12" font-family="{MONO}" fill="{INK}">{esc(label)}</text>')
def mem(x, y, txt="memory"):
    """A memory cell: the value read at the incoming address."""
    w = max(86, len(txt) * 7 + 24)
    s.append(f'<rect x="{x}" y="{y-17}" width="{w}" height="34" fill="#FFFFFF" stroke="{INK}" stroke-width="1.6"/>')
    s.append(f'<line x1="{x+10}" y1="{y-17}" x2="{x+10}" y2="{y+17}" stroke="{INK}" stroke-width="1"/>')
    s.append(f'<text x="{x+48}" y="{y+5}" text-anchor="middle" font-size="12" fill="{INK}">{txt}</text>')
    return x + w

def chain(y, steps):
    """steps: list of ('reg'|'mem'|'pi'|'ea'|'op'|'desc'|'alt', text, arrow label)"""
    x = LX["draw"]
    first = True
    for kind, txt, lab in steps:
        if kind == "note":
            s.append(f'<text x="{LX["draw"]}" y="{y+30}" font-size="11.5" fill="{MUTED}">{esc(txt)}</text>'); continue
        if not first:
            gap = max(46, len(lab or "") * 7.6 + 18)   # arrow long enough for its label
            arrow(x + 4, x + gap, y, lab); x += gap + 2
        first = False
        if kind == "reg":  x = node(x, y, txt, PURPLE)
        elif kind == "abs": x = node(x, y, txt, GREEN)
        elif kind == "mem": x = mem(x, y, txt)
        elif kind == "pi":  x = node(x, y, txt, BLUE, hatch=True)
        elif kind == "ea":  x = node(x, y, txt, GREEN)
        elif kind == "op":  x = node(x, y, txt, ORANGE)
        elif kind == "desc": x = node(x, y, txt, GREY, dashed=True)
        elif kind == "alt": x = node(x, y, txt, GREY, dashed=True, mono=False)
        elif kind == "txt":
            s.append(f'<text x="{x+6}" y="{y+5}" font-size="12" fill="{MUTED}">{esc(txt)}</text>'); x += len(txt) * 6.4 + 12
        elif kind == "note":   # second line under the chain, no arrow
            s.append(f'<text x="{LX["draw"]}" y="{y+30}" font-size="11.5" fill="{MUTED}">{esc(txt)}</text>')
    return x

MODES = [
 ("LOCAL", "B.d", BLUE, False,
  [("reg", "B", None), ("ea", "ea = B + d", "+ d")],
  "40–7F · C1–C3"),
 ("LOCAL, POST-INDEXED", "B.d(Rn)", BLUE, True,
  [("reg", "B", None), ("pi", "+ p·Rn", "+ d"), ("ea", "ea = B + d + p·Rn", "")],
  "D4–DF"),
 ("LOCAL INDIRECT", "IND(B.d)", BLUE, False,
  [("reg", "B", None), ("mem", "pointer", "+ d"), ("ea", "ea = mem[B + d]", "read")],
  "C5–C7"),
 ("LOCAL INDIRECT, P.I.", "IND(B.d)(Rn)", BLUE, True,
  [("reg", "B", None), ("mem", "pointer", "+ d"), ("pi", "+ p·Rn", "read"), ("ea", "ea = mem[B + d] + p·Rn", "")],
  "E4–EF"),
 ("RECORD", "R.d", DTEAL, False,
  [("reg", "R", None), ("ea", "ea = R + d", "+ d")],
  "80–BF · C9–CB"),
 ("PRE-INDEXED", "Rn.d", TEAL, False,
  [("reg", "Rn", None), ("ea", "ea = Rn + d", "+ d")],
  "F4–FF"),
 ("ABSOLUTE", "a", GREEN, False,
  [("abs", "a (4 bytes)", None), ("ea", "ea = a", "")],
  "C4"),
 ("ABSOLUTE, POST-INDEXED", "a(Rn)", GREEN, True,
  [("abs", "a (4 bytes)", None), ("pi", "+ p·Rn", ""), ("ea", "ea = a + p·Rn", "")],
  "E0–E3"),
 ("CONSTANT", "c", ORANGE, False,
  [("op", "operand = c", None), ("txt", "the value is in the instruction itself; there is no address", None)],
  "00–3F · CC–CF"),
 ("REGISTER", "Rn", PURPLE, False,
  [("op", "operand = Rn", None), ("txt", "the register named in the code; no data part, no address", None)],
  "D0–D3"),
 ("prefix DESC", "DESC(desc)(Rn)", GREY, True,
  [("desc", "descriptor", None), ("mem", "base, length", "points to"), ("pi", "+ p·Rn", "base"), ("ea", "ea", ""), ("note", "Rn is checked against the descriptor length (range trap if over); after a data access Rn += 1", None)],
  "F0–F3 + operand"),
 ("prefix ALT", "ALT(operand)", GREY, False,
  [("alt", "operand as usual", None), ("ea", "ea", "same ea"), ("note", "only the final data access goes to the alternative domain (CAD); indirect pointers and descriptors are read in the current domain", None)],
  "C8 + operand"),
]
ry = hy + 10
for name, asm, col, pi, steps, codes in MODES:
    yc = ry + RH / 2
    s.append(f'<rect x="40" y="{ry}" width="{W4-80}" height="{RH}" fill="{col[0]}" fill-opacity="0.45"/>')
    s.append(f'<rect x="40" y="{ry}" width="6" height="{RH}" fill="{col[1]}"/>')
    if pi: s.append(f'<rect x="40" y="{ry}" width="6" height="{RH}" fill="url(#pi)"/>')
    s.append(f'<text x="{LX["name"]+16}" y="{yc+5}" font-size="13" font-weight="700" fill="{col[1]}">{esc(name)}</text>')
    s.append(f'<text x="{LX["asm"]}" y="{yc+5}" font-size="15" font-family="{MONO}" fill="{INK}">{esc(asm)}</text>')
    chain(yc - (8 if any(k == "note" for k, _, _ in steps) else 0), steps)
    s.append(f'<text x="{LX["codes"]}" y="{yc+5}" font-size="11" font-family="{MONO}" fill="{MUTED}">{esc(codes)}</text>')
    ry += RH
    s.append(f'<line x1="40" y1="{ry}" x2="{W4-40}" y2="{ry}" stroke="{RULE}"/>')

ny = ry + 36
s.append(f'<text x="40" y="{ny}" font-size="14" font-weight="700" fill="{INK}">Post-index scale p, bytes per element</text>')
s.append(f'<text x="40" y="{ny+24}" font-size="14" font-family="{MONO}" fill="{INK}">BI 1/8    BY 1    H 2    W 4    F 4    D 8    descriptor 8</text>')
s.append(f'<text x="40" y="{ny+46}" font-size="13" fill="{MUTED}">Rn holds an element number, not a byte offset; the hardware multiplies by p. The next element is Rn + 1, whatever the type.</text>')
s.append(f'<text x="40" y="{ny+86}" font-size="14" font-weight="700" fill="{INK}">Short forms</text>')
s.append(f'<text x="40" y="{ny+110}" font-size="13" fill="{INK}">LOCAL, RECORD and CONSTANT each have a one-byte short form where the 6-bit field is the data itself.</text>')
s.append(f'<text x="40" y="{ny+132}" font-size="13" fill="{MUTED}">For LOCAL and RECORD the short displacement counts words (ea = reg + d·4), so it reaches 0..63 words. The assembler picks the shortest form that fits.</text>')
source(s, W4, H4, "Source: ND-05.009.4 ND-500 Reference Manual, chapter 8 (pages 97–101).")
save("nd500-4-effective-address.svg", s)

# ---- sanity: every mnemonic written in LINES exists in the json -------------
allm = set(it["mnemonic"] for it in items)
SKIP = {"a1..a4", "e1..e4", ":=/=:", "ll", "hl", "tha", "ps", "st1", "cad", "ced", "p", "ote1", "ote2",
        "mte1", "mte2", "cte1", "cte2", "temm1", "temm2", "(+r", "forms)", "padd", "psub", "pmpy", "ppack", "pupack"}
bad = []
for c, lines in LINES.items():
    for ln in lines:
        for tok in ln.split():
            if tok in SKIP: continue
            if tok not in allm: bad.append((c, tok))
            elif next(it["class"] for it in items if it["mnemonic"] == tok) != c: bad.append((c, tok, "wrong class"))
print("mnemonic check:", bad or "ok")
print("opcodes: 54", by_op[0x54]["mnemonic"], "58", by_op[0x58]["mnemonic"], "6C", by_op[0x6C]["mnemonic"], "20", by_op[0x20]["mnemonic"], "FC40", by_op[0xFC40]["mnemonic"])
