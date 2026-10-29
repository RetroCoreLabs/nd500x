#!/usr/bin/env python3
"""Generate the ND-500 opcode matrix page (one self-contained HTML file).

The page shows the five 16 x 16 opcode maps of the ND-500: the first byte
0x00-0xFF, and the second byte after each of the four prefix bytes 0xFC,
0xFD, 0xFE and 0xFF. Every cell is coloured by instruction class, carries
the class code as text, and opens a detail panel with the instruction page
link.

Data: docs/instructions/instructions.json (1,078 entries). Labels:
  data type  = the variantNumber-th data type in the entry's "prefixes"
               field (BI BY H W F D, in that order);
  register   = position (1-4) of the opcode inside its group of four
               consecutive opcodes with the same function and variant.
Both are cross-checked against test/nd500-conformance.json, and the result
of that check is printed on the page itself, so a reader sees how far the
labels are verified.

Usage:
  python3 tools/gen_opcode_matrix.py [OUTFILE]    writes a standalone file
                                                  (default: stdout is not used;
                                                  OUTFILE is required)
tools/gen_docs_site.py imports build_page() and publishes the result on the
site, with instruction links pointing at the site's own pages.
"""
import collections
import html
import json
import os
import re
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
TYPES = ["BI", "BY", "H", "W", "F", "D"]

# class -> (colour family, three-letter code shown in the cell)
CLASSES = {
    "MOVE": ("blue", "MOV"),
    "STRING": ("dteal", "STR"),
    "ARITHMETIC": ("teal", "ARI"),
    "FLOAT_MATH": ("teal", "FLT"),
    "LOGICAL": ("teal", "LOG"),
    "SHIFT": ("teal", "SHF"),
    "BITFIELD": ("teal", "BIT"),
    "COMPARE": ("teal", "CMP"),
    "BRANCH": ("green", "BRA"),
    "CALL": ("green", "CAL"),
    "CONTROL": ("green", "CTL"),
    "SYSTEM": ("purple", "SYS"),
    "IO": ("purple", "IO"),
}
CLASS_ORDER = list(CLASSES)
CLASS_NAME = {"FLOAT_MATH": "Float math"}


def load_items():
    path = os.path.join(ROOT, "docs/instructions/instructions.json")
    with open(path, encoding="utf-8") as fh:
        d = json.load(fh)
    items = d["instructions"]
    assert d["totalInstructions"] == len(items) == 1078, len(items)
    return items


def derive(items):
    """opcode -> list of cell entries (more than one when json lists a shared opcode)."""
    groups = collections.defaultdict(list)
    for it in items:
        groups[(it["functionName"], it["variantNumber"])].append(it)
    cells = collections.defaultdict(list)
    for (fn, vn), es in groups.items():
        es.sort(key=lambda e: int(e["opcode"], 16))
        toks = [t.strip() for t in es[0]["prefixes"].replace("InstructionPrefixes.", "").split("|")]
        types = [t for t in TYPES if t in toks]
        dtype = types[vn] if vn < len(types) else None
        ops = [int(e["opcode"], 16) for e in es]
        if len(es) == 4:
            assert ops == list(range(ops[0], ops[0] + 4)), (fn, vn, ops)
            assert dtype is not None, (fn, vn)
        else:
            assert len(es) == 1, (fn, vn, len(es))
        for i, e in enumerate(es):
            reg = i + 1 if len(es) == 4 else None
            mn = e["mnemonic"].upper()
            if dtype and reg:
                label = f"{dtype}{reg} {mn}"
            elif dtype:
                label = f"{dtype} {mn}"
            else:
                label = mn
            cells[int(e["opcode"], 16)].append({
                "label": label, "mn": e["mnemonic"], "fn": e["functionName"],
                "cls": e["class"], "type": dtype, "reg": reg,
                "n": e["operandCount"], "v": f"{vn + 1}/{e['totalVariants']}",
            })
    return cells


def cross_check(cells):
    """(agree, total, [opcodes that disagree]) against the conformance corpus."""
    with open(os.path.join(ROOT, "test/nd500-conformance.json"), encoding="utf-8") as fh:
        corpus = json.load(fh)
    rx = re.compile(r"^(bi|by|h|w|f|d)([1-4])\s+(\S+)", re.I)
    truth = collections.defaultdict(set)
    for c in corpus:
        m = rx.match(c["assembly"].strip())
        b = c["bytes"]
        if not m:
            continue
        op = b[0] if b[0] < 0xFC else (b[0] << 8) | b[1]
        truth[op].add((m.group(1).upper(), int(m.group(2)), m.group(3).upper()))

    def agrees(c, t):
        # The corpus always writes a register digit; a label without a register
        # (a single opcode for the instruction) only has to match type and mnemonic.
        return c["type"] == t[0] and c["mn"].upper() == t[2] and (c["reg"] is None or c["reg"] == t[1])

    bad = [op for op, ts in sorted(truth.items())
           if not any(agrees(c, t) for c in cells.get(op, []) for t in ts)]
    return len(truth) - len(bad), len(truth), bad


CSS = """
:root{
 --bg:#ffffff;--fg:#1b1f23;--muted:#4a5560;--line:#c9d1d9;--panel:#f6f8fa;--empty:#eef1f4;--focus:#0d47a1;
 --blue-bg:#E3F2FD;--blue-ln:#0D47A1;--teal-bg:#E0F7FA;--teal-ln:#00838F;--green-bg:#E8F5E9;--green-ln:#2E7D32;
 --purple-bg:#F3E5F5;--purple-ln:#7B1FA2;--orange-bg:#FFF3E0;--orange-ln:#E65100;--dteal-bg:#E0F2F1;--dteal-ln:#00695C;
}
@media (prefers-color-scheme:dark){:root:not([data-theme="light"]){
 --bg:#14181d;--fg:#e8eaed;--muted:#aab3bd;--line:#3a434d;--panel:#1c2229;--empty:#1a1f25;--focus:#90caf9;
 --blue-bg:#10304f;--blue-ln:#64B5F6;--teal-bg:#0d3a40;--teal-ln:#4DD0E1;--green-bg:#14361a;--green-ln:#81C784;
 --purple-bg:#35183c;--purple-ln:#CE93D8;--orange-bg:#4a2a08;--orange-ln:#FFB74D;--dteal-bg:#0c3a35;--dteal-ln:#4DB6AC;
 color-scheme:dark}}
:root[data-theme="dark"]{
 --bg:#14181d;--fg:#e8eaed;--muted:#aab3bd;--line:#3a434d;--panel:#1c2229;--empty:#1a1f25;--focus:#90caf9;
 --blue-bg:#10304f;--blue-ln:#64B5F6;--teal-bg:#0d3a40;--teal-ln:#4DD0E1;--green-bg:#14361a;--green-ln:#81C784;
 --purple-bg:#35183c;--purple-ln:#CE93D8;--orange-bg:#4a2a08;--orange-ln:#FFB74D;--dteal-bg:#0c3a35;--dteal-ln:#4DB6AC;
 color-scheme:dark}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:14px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;
 padding-block:12px;padding-left:max(16px,env(safe-area-inset-left,0px));padding-right:max(16px,env(safe-area-inset-right,0px))}
h1{font-size:1.25rem;margin:0 0 4px;text-wrap:balance}
p{margin:0 0 8px;max-width:75ch}
.muted{color:var(--muted)}
.bar{display:flex;flex-wrap:wrap;gap:8px;align-items:center;margin:10px 0}
.tabs{display:flex;flex-wrap:wrap;gap:6px}
button,input{font:inherit;color:var(--fg)}
.tab,.chip{background:var(--panel);border:1px solid var(--line);border-radius:6px;padding:4px 10px;cursor:pointer}
.tab[aria-selected="true"]{background:var(--orange-bg);border-color:var(--orange-ln);font-weight:600}
.chip{display:inline-flex;gap:6px;align-items:center;padding:3px 8px;font-size:12.5px}
.chip[aria-pressed="true"]{outline:2px solid var(--focus);outline-offset:1px}
.sw{width:12px;height:12px;border-radius:3px;border:2px solid;display:inline-block}
input[type=search]{background:var(--panel);border:1px solid var(--line);border-radius:6px;padding:4px 10px;min-width:12rem}
.legend{display:flex;flex-wrap:wrap;gap:6px;margin:6px 0 10px}
.wrap{overflow-x:auto;padding-bottom:6px}
.grid{display:grid;grid-template-columns:28px repeat(16,minmax(0,1fr));gap:3px;min-width:800px}
.hd{font:600 12px/1 ui-monospace,Menlo,Consolas,monospace;color:var(--muted);display:flex;align-items:center;justify-content:center;min-height:20px}
.cell{display:flex;flex-direction:column;justify-content:center;gap:1px;min-height:46px;padding:3px 4px;border-radius:5px;
 border:2px solid var(--line);background:var(--empty);text-align:left;cursor:pointer;overflow:hidden;min-width:0}
.cell .l{font:600 12px/1.15 ui-monospace,Menlo,Consolas,monospace;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.cell .h{font:11px/1.1 ui-monospace,Menlo,Consolas,monospace;color:var(--muted);white-space:nowrap}
.cell.none{cursor:default;border-style:dashed;background:transparent}
.cell.none .l{font-weight:400;color:var(--muted)}
.cell.shared{border-style:double;border-width:4px}
.cell[data-f=blue]{background:var(--blue-bg);border-color:var(--blue-ln)}
.cell[data-f=teal]{background:var(--teal-bg);border-color:var(--teal-ln)}
.cell[data-f=green]{background:var(--green-bg);border-color:var(--green-ln)}
.cell[data-f=purple]{background:var(--purple-bg);border-color:var(--purple-ln)}
.cell[data-f=dteal]{background:var(--dteal-bg);border-color:var(--dteal-ln)}
.cell[data-f=orange]{background:var(--orange-bg);border-color:var(--orange-ln)}
.cell.dim{opacity:.18}
.cell:focus-visible,.tab:focus-visible,.chip:focus-visible,input:focus-visible{outline:3px solid var(--focus);outline-offset:1px}
.cell[aria-current=true]{box-shadow:0 0 0 3px var(--focus)}
.sw[data-f=blue]{background:var(--blue-bg);border-color:var(--blue-ln)}
.sw[data-f=teal]{background:var(--teal-bg);border-color:var(--teal-ln)}
.sw[data-f=green]{background:var(--green-bg);border-color:var(--green-ln)}
.sw[data-f=purple]{background:var(--purple-bg);border-color:var(--purple-ln)}
.sw[data-f=dteal]{background:var(--dteal-bg);border-color:var(--dteal-ln)}
.sw[data-f=orange]{background:var(--orange-bg);border-color:var(--orange-ln)}
#detail{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:10px 14px;margin:12px 0;min-height:5.5rem}
#detail h2{font:700 1.05rem ui-monospace,Menlo,Consolas,monospace;margin:0 0 4px}
#detail dl{display:grid;grid-template-columns:max-content 1fr;gap:2px 12px;margin:6px 0 0}
#detail dt{color:var(--muted)}
#detail dd{margin:0;font-family:ui-monospace,Menlo,Consolas,monospace}
#detail a{color:var(--focus)}
#detail hr{border:0;border-top:1px solid var(--line);margin:8px 0}
.foot{font-size:12.5px;color:var(--muted);max-width:90ch}
.foot p{margin:0 0 4px}
@media (prefers-reduced-motion:no-preference){.cell{transition:opacity .15s}}
"""

JS = r"""
(function(){
var D=JSON.parse(document.getElementById('data').textContent);
var CL=D.classes, MAPS=D.maps, cur='one', sel=null, cls=null, q='';
var grid=document.getElementById('grid'), detail=document.getElementById('detail');
function hex(n,w){var s=n.toString(16).toUpperCase();while(s.length<w)s='0'+s;return s}
function bin(n,w){var s=n.toString(2);while(s.length<w)s='0'+s;return s.replace(/(.{4})(?=.)/g,'$1_')}
function el(t,c,x){var e=document.createElement(t);if(c)e.className=c;if(x!==undefined)e.textContent=x;return e}
function opcode(map,i){return map.prefix===null?i:(map.prefix<<8)|i}
function matches(c){
 if(cls&&c.cls!==cls)return false;
 if(q){var s=(c.label+' '+c.fn+' '+c.mn+' '+c.cls).toLowerCase();if(s.indexOf(q)<0)return false}
 return true}
function build(){
 var map=MAPS[cur];grid.textContent='';
 grid.appendChild(el('div','hd',''));
 for(var c=0;c<16;c++)grid.appendChild(el('div','hd','x'+hex(c,1)));
 for(var r=0;r<16;r++){
  grid.appendChild(el('div','hd',hex(r,1)+'x'));
  for(var c2=0;c2<16;c2++){
   var i=r*16+c2, ents=map.cells[i]||null, b=el('button','cell');
   b.type='button';b.dataset.i=i;b.tabIndex=(i===0?0:-1);
   var hx=(map.prefix===null?'':hex(map.prefix,2)+' ')+hex(i,2);
   if(map.prefix===null&&i>=0xFC){
    b.dataset.f='orange';b.appendChild(el('span','l',hex(i,2)+' xx'));b.appendChild(el('span','h','prefix'));
    b.setAttribute('aria-label','Prefix byte '+hex(i,2)+', opens the second-byte map');
   }else if(ents){
    var e=ents[0], f=CL[e.cls][0];b.dataset.f=f;
    b.appendChild(el('span','l',e.label));b.appendChild(el('span','h',hx+' '+CL[e.cls][1]+(ents.length>1?' +'+(ents.length-1):'')));
    if(ents.length>1)b.classList.add('shared');
    b.setAttribute('aria-label',hx+' '+ents.map(function(x){return x.label}).join(' and ')+', class '+e.cls);
    b.title=ents.map(function(x){return x.label}).join(' / ');
   }else{
    b.classList.add('none');b.appendChild(el('span','l','-'));b.appendChild(el('span','h',hx));
    b.setAttribute('aria-label',hx+' not in instructions.json');
   }
   b.dataset.d=(ents&&!(map.prefix===null&&i>=0xFC))?(ents.some(matches)?'':'1'):(((map.prefix===null&&i>=0xFC)&&(cls||q))?'1':'');
   if(b.dataset.d)b.classList.add('dim');
   grid.appendChild(b);
  }}
 grid.setAttribute('aria-label',map.title);
 document.getElementById('count').textContent=map.count;
}
function show(i){
 var map=MAPS[cur];sel=i;
 [].forEach.call(grid.querySelectorAll('.cell'),function(b){if(+b.dataset.i===i)b.setAttribute('aria-current','true');else b.removeAttribute('aria-current')});
 detail.textContent='';
 var op=opcode(map,i), w=map.prefix===null?2:4, ents=map.cells[i];
 if(map.prefix===null&&i>=0xFC){
  detail.appendChild(el('h2','','0x'+hex(i,2)+'  prefix byte'));
  detail.appendChild(el('p','','The byte after 0x'+hex(i,2)+' selects the instruction. '+MAPS['p'+hex(i,2)].count));
  var go=el('button','tab','Open the 0x'+hex(i,2)+' xx map');go.type='button';go.onclick=function(){tab('p'+hex(i,2))};
  detail.appendChild(go);return}
 if(!ents){
  detail.appendChild(el('h2','','0x'+hex(op,w)+'  not in instructions.json'));
  detail.appendChild(el('p','muted','No instruction in instructions.json has this opcode.'));return}
 ents.forEach(function(e,k){
  if(k)detail.appendChild(el('hr'));
  detail.appendChild(el('h2','','0x'+hex(op,w)+'  '+e.label));
  var dl=el('dl');
  function row(a,b){dl.appendChild(el('dt','',a));dl.appendChild(el('dd','',b))}
  row('opcode','0x'+hex(op,w)+'   '+bin(op,w*4)+'   '+op);
  row('class',(CL[e.cls][2]||e.cls)+' ('+CL[e.cls][1]+')');
  row('function',e.fn);
  row('data type',e.type?e.type:'none in the prefixes field');
  row('register',e.reg?String(e.reg):'not part of this opcode');
  row('operands',String(e.n));
  row('variant',e.v);
  detail.appendChild(dl);
  if(e.url){var p=el('p');var a=el('a','','Instruction page: '+e.mn);a.href=e.url;p.appendChild(a);detail.appendChild(p)}
 });
 if(ents.length>1)detail.appendChild(el('p','muted','instructions.json lists this opcode '+ents.length+' times (different variants of the same instruction).'));
}
function tab(t){
 cur=t;sel=null;build();
 [].forEach.call(document.querySelectorAll('.tab[data-t]'),function(b){b.setAttribute('aria-selected',b.dataset.t===t?'true':'false')});
 detail.textContent='';detail.appendChild(el('p','muted','Select a cell for details. Arrow keys move between cells.'));
 resize()}
function refresh(){tab(cur)}
document.getElementById('tabs').addEventListener('click',function(ev){var b=ev.target.closest('.tab[data-t]');if(b)tab(b.dataset.t)});
grid.addEventListener('click',function(ev){var b=ev.target.closest('.cell');if(!b||b.classList.contains('none'))return;show(+b.dataset.i);resize()});
grid.addEventListener('keydown',function(ev){
 var b=ev.target.closest('.cell');if(!b)return;var i=+b.dataset.i,n=i;
 if(ev.key==='ArrowRight')n=Math.min(255,i+1);else if(ev.key==='ArrowLeft')n=Math.max(0,i-1);
 else if(ev.key==='ArrowDown')n=Math.min(255,i+16);else if(ev.key==='ArrowUp')n=Math.max(0,i-16);
 else if(ev.key==='Home')n=i-(i%16);else if(ev.key==='End')n=i-(i%16)+15;else return;
 ev.preventDefault();var t=grid.querySelector('.cell[data-i="'+n+'"]');
 [].forEach.call(grid.querySelectorAll('.cell'),function(x){x.tabIndex=-1});t.tabIndex=0;t.focus()});
document.getElementById('legend').addEventListener('click',function(ev){
 var b=ev.target.closest('.chip');if(!b)return;var c=b.dataset.c;cls=(cls===c)?null:c;
 [].forEach.call(document.querySelectorAll('.chip'),function(x){x.setAttribute('aria-pressed',x.dataset.c===cls?'true':'false')});
 build()});
document.getElementById('q').addEventListener('input',function(ev){q=ev.target.value.trim().toLowerCase();build()});
function resize(){try{if(window.frameElement){window.frameElement.style.height=(document.documentElement.scrollHeight+4)+'px'}}catch(e){}}
function theme(){try{var p=window.parent;if(p===window)return;
 var s=p.document.body.getAttribute('data-md-color-scheme');
 document.documentElement.setAttribute('data-theme',s==='slate'?'dark':'light')}catch(e){}}
try{if(window.parent!==window){theme();new MutationObserver(theme).observe(window.parent.document.body,{attributes:true,attributeFilter:['data-md-color-scheme']})}}catch(e){}
window.addEventListener('resize',resize);
var h=(location.hash||'').replace('#','');tab(MAPS[h]?h:'one');
})();
"""


def build_page(link_for_fn=None):
    """Return the page as a string. link_for_fn(functionName) -> URL or None."""
    items = load_items()
    cells = derive(items)
    agree, total, bad = cross_check(cells)

    def entry(c):
        e = dict(c)
        url = link_for_fn(c["fn"]) if link_for_fn else None
        if url:
            e["url"] = url
        return e

    maps = {}
    one = {i: [entry(c) for c in cells[i]] for i in range(0x100) if i in cells}
    one_cnt = sum(1 for i in range(0xFC) if i in cells)
    maps["one"] = {"prefix": None, "title": "First opcode byte, 0x00 to 0xFF", "cells": one,
                   "count": f"{one_cnt} of the 252 first-byte values 0x00-0xFB hold an instruction; 0xFC-0xFF are prefixes."}
    for p in (0xFC, 0xFD, 0xFE, 0xFF):
        m = {i: [entry(c) for c in cells[(p << 8) | i]] for i in range(0x100) if ((p << 8) | i) in cells}
        shared = sum(1 for i in m if len(m[i]) > 1)
        txt = f"{len(m)} of 256 second-byte values hold an instruction"
        txt += f" ({shared} listed more than once)." if shared else "."
        maps[f"p{p:02X}"] = {"prefix": p, "title": f"Second byte after prefix 0x{p:02X}", "cells": m, "count": txt}
    # tab labels
    tabs = [("one", "First byte")] + [(f"p{p:02X}", f"0x{p:02X} xx") for p in (0xFC, 0xFD, 0xFE, 0xFF)]

    classes = {c: [CLASSES[c][0], CLASSES[c][1], CLASS_NAME.get(c, c.capitalize())] for c in CLASS_ORDER}
    seen = {i["class"] for i in items}
    assert seen == set(CLASSES), seen ^ set(CLASSES)

    data = json.dumps({"classes": classes, "maps": maps}, separators=(",", ":"))
    data = data.replace("</", "<\\/")

    shared_total = sum(1 for v in cells.values() if len(v) > 1)
    distinct = len(cells)
    check_txt = (f"Cross-check of data type and register against test/nd500-conformance.json: "
                 f"{agree} of {total} opcodes agree")
    if bad:
        check_txt += "; the others: " + ", ".join(f"0x{o:04X}" for o in bad) + " (no opcode of that value in instructions.json)"
    check_txt += "."

    tab_html = "".join(
        f'<button class="tab" role="tab" type="button" data-t="{k}" aria-selected="false">{html.escape(v)}</button>'
        for k, v in tabs)
    chips = "".join(
        f'<button class="chip" type="button" data-c="{c}" aria-pressed="false">'
        f'<span class="sw" data-f="{CLASSES[c][0]}"></span>{html.escape(CLASS_NAME.get(c, c.capitalize()))} '
        f'<span class="muted">{CLASSES[c][1]}</span></button>' for c in CLASS_ORDER)

    return f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ND-500 opcode matrix</title>
<base target="_top">
<style>{CSS}</style></head>
<body>
<h1>ND-500 opcode matrix</h1>
<p class="muted">Each cell is one opcode, coloured by instruction class and labelled with the data type, register and mnemonic.
Bytes 0xFC to 0xFF are prefixes: the instruction is then chosen by the second byte, in its own map.</p>
<div class="bar"><div class="tabs" id="tabs" role="tablist" aria-label="Opcode map">{tab_html}</div>
<label>Find <input id="q" type="search" placeholder="mnemonic or function" aria-label="Find a mnemonic or function"></label></div>
<div class="legend" id="legend" role="group" aria-label="Filter by class">{chips}</div>
<p class="muted" id="count"></p>
<div class="wrap"><div class="grid" id="grid" role="group"></div></div>
<div id="detail" aria-live="polite"></div>
<div class="foot">
<p>Source: docs/instructions/instructions.json, {len(items)} entries, {distinct} distinct opcodes; {shared_total} opcodes are listed by more than one entry (double border).</p>
<p>Data type: the variant-numbered entry of the entry's prefixes field. Register: position 1-4 within the four consecutive opcodes of one variant.</p>
<p>{html.escape(check_txt)}</p>
</div>
<script type="application/json" id="data">{data}</script>
<script>{JS}</script>
</body></html>
"""


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: gen_opcode_matrix.py OUTFILE")
    with open(sys.argv[1], "w", encoding="utf-8") as fh:
        fh.write(build_page())
    print("gen_opcode_matrix: wrote", sys.argv[1], file=sys.stderr)
