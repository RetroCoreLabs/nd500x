#!/usr/bin/env python3
"""Generate the MkDocs page tree and navigation for the nd500x documentation site.

Writes build-site/ (gitignored) from the Markdown files git tracks:

  build-site/docs/<repo path>.md     every page, copied, with relative links
                                     rewritten (see "Links" below)
  build-site/docs/docs/instructions/ALL-INSTRUCTIONS.md
                                     generated index of the 241 instructions
  build-site/mkdocs.yml              INHERIT: ../docs-site/mkdocs.yml + nav

Usage, from anywhere:
  python3 tools/gen_docs_site.py                 generate
  python3 tools/gen_docs_site.py --check         verify only, exit 1 on a gap
  python3 tools/gen_docs_site.py --include-untracked
                                                 also take untracked .md files
                                                 (local preview of new files;
                                                 CI never needs it)

POLICY - what is on the site and where
--------------------------------------
Sections and their files are listed in SECTIONS below, in the order they appear
in the left-hand tree. A pattern is a git path or a glob; glob matches are
sorted. The 241 per-instruction pages (docs/instructions/asm/*.md) are placed
by instructions.json: each class gets its reference page as section index and
its instructions below it, sorted by mnemonic.

OFF the site (OFF_SITE below), linked to github.com where a page points at them:
  CLAUDE.md                        tool instructions, not documentation
  docs/instructions/yaml/          same content as the asm pages, machine form
  docs/instructions/instructions.md  1,086-row table; the generated index replaces it
  docs/instructions/*.json, *.py   data and tooling, not pages
  docker/DOCKERHUB.md              the Docker Hub listing text
  external/, .claude/              not this repository's documentation

A tracked .md that no pattern covers makes --check fail, so a new document is
placed on purpose, never by accident.

Links
-----
A relative link whose target is on the site stays relative (the site keeps the
repository tree, so most links work unchanged; README.md becomes index.md and a
file name with spaces is renamed with dashes, both handled through a path map).
A relative link to anything else that exists in the repository (source, yaml,
json, workflows, LICENSE) becomes a github.com blob/tree URL. Anything that
resolves to nothing is left alone, so `mkdocs build --strict` reports it.
Images referenced from an included page are copied along with it.

External images
---------------
An image loaded from another site blocks the page's load event until that
host answers. The reference manual embeds one scanned page from
standardmemory.com, which does not answer at all, and the page took over two
minutes to render in Chrome because of it (measured 29 October 2026). So an
image whose host is not in IMAGE_HOSTS_OK is published as a plain link with
its alt text; the reader can still open the scan, the page never waits for it.

Lists after a label
-------------------
github.com lets a list start on the line right after a paragraph; Python-Markdown
needs a blank line first and otherwise runs the items into the paragraph. Many
instruction pages are written the GitHub way ("**Key Characteristics:**" then
"- ..." on the next line). The copy inserts that blank line, outside code
fences only, so the site shows the list the author saw on github.com.
"""
import glob
import json
import os
import re
import subprocess
import sys
import urllib.parse

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
OUT = os.path.join(ROOT, "build-site")
GITHUB = "https://github.com/RetroCoreLabs/nd500x"
BRANCH = "main"

# ---------------------------------------------------------------------------
# Policy
# ---------------------------------------------------------------------------
# (title, entries). An entry is a pattern string, a (title, pattern) pair, or a
# nested (title, entries) section. "@instructions" expands to the per-class
# instruction tree; "@all-instructions" is the generated index page.
SECTIONS = [
    ("Home", ["README.md"]),
    ("Documents overview", ["docs/README.md"]),
    ("Using the emulator", [
        "docker/README.md",
        "docs/INIT_SCRIPT_FORMAT.md",
        "docs/SINTRAN-SHELL-SPEC.md",
        "docs/NC_INTERACTIVE_COMMANDS.md",
        "docs/RUN_500_SH_CONFIG_RECONSTRUCTION.md",
        "docs/PATH_CONVENTIONS.md",
        "docs/EXTERNAL-ARTIFACTS.md",
        "src/frontend/nd500wasm/web/demo/README.md",
        ("Examples", [
            "examples/README.md",
            "examples/04-c-math/README.md",
            ("C kernel", [
                "examples/05-c-kernel/README.md",
                "examples/05-c-kernel/*.md",
            ]),
        ]),
    ]),
    ("Debugger and DAP", [
        "docs/DEBUGGER_COMMAND_REFERENCE.md",
        "docs/DAP_INTEGRATION.md",
        "docs/DAP_COMMAND_TEST_MATRIX.md",
        "docs/DAP_BUG_REPORTS.md",
    ]),
    ("ND-500 reference", [
        "docs/INSTRUCTION-SET-OVERVIEW.md",
        ("All instructions", "@all-instructions"),
        ("Instructions by class", ["@instructions"]),
        "docs/ND-500-TRAPS.md",
        "docs/ND500_PACKED_BCD.md",
        "docs/cpu_implementation_changes.md",
        "docs/INSTRUCTION-VALIDATION-REPORT.md",
        "docs/SPEC-CORPUS-SOLO-TUTTI-DT-DE.md",
        "test/TRAP_TEST_PLAN.md",
        "docs/instructions/README.md",
        "docs/instructions/DOCUMENTATION_STANDARDS.md",
        ("Norsk Data manuals", [
            ("ND-500 Reference Manual (ND-05.009.4)", "docs/ND-05.009.4 EN ND-500 Reference Manual.md"),
            ("ND-500 Assembler Reference Manual (ND-60.113.02)", "docs/ND-60.113.02 EN Assembler Reference Manual.md"),
            ("ND Linker User Guide and Reference Manual (ND-860289-2)", "docs/ND-860289-2-EN ND Linker User Guide and Reference Manual.md"),
        ]),
    ]),
    ("Toolchain and SINTRAN", [
        "docs/SINTRAN-CONVENTIONS.md",
        "docs/CONVERT_DOMAIN_FORMAT_AND_USAGE.md",
        "docs/VTM-FILE-FORMAT.md",
        "docs/mon_tests_SCHEMA.md",
        "docs/NDIX-NETWORKING.md",
        "docs/HOWTO-TEST-NDIX-NETWORKING.md",
        "docs/NDIX_FECALL_MON600_SPEC.md",
        "docs/ND5000-OCTOBUS-SHARED-MEMORY-ARCHITECTURE.md",
        "tools/README.md",
        "tools/gen-asm-map.md",
        "tools/git-hooks/README.md",
        "tools/ndix/README.md",
        "tools/ndix/userland/README.md",
        "tools/ndix/userland/bsd43/README.md",
        "docs-site/README.md",
    ]),
    ("Working notes", [
        "docs/CURRENT-PLAN.md",
        "docs/SYNC-BACKLOG.md",
        "docs/INVESTIGATION-TRAPS.md",
        "docs/PLAN-THREE-EMULATOR-ALIGNMENT.md",
        "docs/HANDOFF-*.md",
        "docs/SYNC-*.md",
        "docs/CARVE*.md",
        "docs/CONVERT_DOMAIN_MON_ANALYSIS.md",
        "docs/HELP-CRASH-*.md",
        "docs/LINKER-LOAD-ERROR52-INVESTIGATION.md",
        "docs/NC_CRASH_*.md",
        "docs/NC_TOOLCHAIN_MON_PLAN.md",
        "docs/MON_TO_BINARY_PLAN.md",
        "docs/TEST-PLAN-NDIX-NETWORKING.md",
        "docs/NDIX_SEG30_MICROCODE_ANSWER.md",
        "docs/ND500-CONFORMANCE-CORPUS-HANDOFF-*.md",
    ]),
]

OFF_SITE = [
    "CLAUDE.md",
    "docs/instructions/yaml/*.md",
    "docs/instructions/instructions.md",
    "docker/DOCKERHUB.md",
]

# Per-instruction page file for a mnemonic when neither the lower-cased
# function name nor the mnemonic with ':' -> '_' is the file name.
ASM_SPECIAL = {
    "+": "add.md", "-": "sub.md", "*": "mul.md", "/": "div.md",
    ":=": "assignto.md", "=:": "assignfrom.md",
    "b:=": "b_assignto.md", "b=:": "b_assignfrom.md",
    "r:=": "r_assignto.md", "r=:": "r_assignfrom.md",
}

IMAGE_EXT = (".svg", ".png", ".jpg", ".jpeg", ".gif")

# Hosts whose images may stay inline (they answer quickly and reliably).
IMAGE_HOSTS_OK = ("img.shields.io", "upload.wikimedia.org")

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def git_files(include_untracked):
    cmd = ["git", "ls-files"]
    if include_untracked:
        cmd += ["--cached", "--others", "--exclude-standard"]
    out = subprocess.run(cmd, cwd=ROOT, check=True, capture_output=True, text=True).stdout
    files = [l for l in out.splitlines() if l]
    return files

def md_universe(files):
    return sorted(f for f in files if f.endswith(".md")
                  and not f.startswith(("external/", ".claude/", "build")))

def match(pattern, universe):
    if any(c in pattern for c in "*?["):
        return sorted(f for f in universe if glob.fnmatch.fnmatch(f, pattern))
    return [pattern] if pattern in universe else []

def page_title(repo_path):
    with open(os.path.join(ROOT, repo_path), encoding="utf-8", errors="replace") as fh:
        for line in fh:
            if line.startswith("# "):
                t = line[2:].strip()
                t = re.sub(r"[`*_]", "", t)
                return t
    return os.path.splitext(os.path.basename(repo_path))[0]

def dest_path(repo_path):
    """Where a repository file lands under build-site/docs/."""
    if repo_path == "README.md":
        return "index.md"
    d, b = os.path.split(repo_path)
    b = b.replace(" ", "-")
    return os.path.join(d, b) if d else b

def rel(from_dir, to_path):
    r = os.path.relpath(to_path, from_dir or ".")
    return r.replace(os.sep, "/")

# ---------------------------------------------------------------------------
# Instruction pages from instructions.json
# ---------------------------------------------------------------------------
def load_instructions():
    with open(os.path.join(ROOT, "docs/instructions/instructions.json"), encoding="utf-8") as fh:
        items = json.load(fh)["instructions"]
    by_fn = {}
    for it in items:
        e = by_fn.setdefault(it["functionName"], {
            "mnemonic": it["mnemonic"], "class": it["class"], "fn": it["functionName"],
            "operands": it["operandCount"], "types": set(), "opcodes": []})
        e["opcodes"].append(int(it["opcode"], 16))
        for tok in it["prefixes"].replace("InstructionPrefixes.", "").split("|"):
            tok = tok.strip()
            if tok in ("BI", "BY", "H", "W", "F", "D"):
                e["types"].add(tok)
    return by_fn, len(items)

def asm_file(e, universe):
    m = e["mnemonic"]
    for cand in (ASM_SPECIAL.get(m), e["fn"].lower() + ".md", m.replace(":", "_") + ".md"):
        if cand and "docs/instructions/asm/" + cand in universe:
            return "docs/instructions/asm/" + cand
    return None

TYPE_ORDER = ["BI", "BY", "H", "W", "F", "D"]

def instruction_tree(by_fn, universe, problems):
    """[(class, ref_page, [(title, asm_path), ...])] and the index rows."""
    classes = {}
    for fn, e in by_fn.items():
        p = asm_file(e, universe)
        if p is None:
            problems.append(f"no asm page for instruction {e['mnemonic']!r} ({fn})")
            continue
        e["page"] = p
        classes.setdefault(e["class"], []).append(e)
    tree = []
    for cls in sorted(classes):
        ref = f"docs/instruction-reference/{cls}.md"
        if ref not in universe:
            problems.append(f"no class reference page {ref}")
        entries = sorted(classes[cls], key=lambda e: (e["mnemonic"].lower(), e["fn"]))
        tree.append((cls, ref, [(f"{e['mnemonic']}  ({e['fn']})", e["page"]) for e in entries]))
    return tree

def write_all_instructions(by_fn, total, pathmap, used):
    """Generated index page, placed at docs/instructions/ALL-INSTRUCTIONS.md."""
    page = "docs/instructions/ALL-INSTRUCTIONS.md"
    here = os.path.dirname(page)
    rows = []
    for e in sorted(by_fn.values(), key=lambda e: (e["mnemonic"].lower(), e["fn"])):
        if "page" not in e:
            continue
        ops = sorted(e["opcodes"])
        rng = f"{ops[0]:04X}" if len(ops) == 1 else f"{ops[0]:04X}-{ops[-1]:04X}"
        types = " ".join(t for t in TYPE_ORDER if t in e["types"]) or "-"
        mn = e["mnemonic"].replace("|", "\\|")
        link = rel(here, pathmap[e["page"]])
        cls_link = rel(here, pathmap[f"docs/instruction-reference/{e['class']}.md"])
        rows.append(f"| [`{mn}`]({link}) | {e['fn']} | [{e['class']}]({cls_link}) | {types} | {e['operands']} | {len(ops)} | `{rng}` |")
    text = [
        "<!-- generated by tools/gen_docs_site.py from docs/instructions/instructions.json; edit the source, not this page -->",
        "# All instructions",
        "",
        f"{len(rows)} instructions, {total} opcodes. One row per instruction; the opcode count is the number of",
        "data-type and register variants, and the range is the lowest and highest opcode of those variants.",
        "Mnemonics link to the instruction's own page; classes link to the class reference.",
        "",
        "| Mnemonic | Function | Class | Data types | Operands | Opcodes | Opcode range |",
        "|---|---|---|---|---|---|---|",
    ] + rows + [""]
    dest = os.path.join(OUT, "docs", page)
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    with open(dest, "w", encoding="utf-8") as fh:
        fh.write("\n".join(text))
    used.add(page)
    return page

# ---------------------------------------------------------------------------
# Navigation from the policy
# ---------------------------------------------------------------------------
def expand(entries, universe, used, problems, instr_tree, all_page, pathmap):
    nav = []
    for entry in entries:
        if isinstance(entry, tuple) and isinstance(entry[1], list):
            title, sub = entry
            if sub == ["@instructions"]:
                sub_nav = []
                for cls, ref, pages in instr_tree:
                    items = []
                    if ref in universe:
                        items.append(pathmap[ref]); used.add(ref)
                    for t, p in pages:
                        items.append({t: pathmap[p]}); used.add(p)
                    sub_nav.append({cls: items})
                nav.append({title: sub_nav})
            else:
                nav.append({title: expand(sub, universe, used, problems, instr_tree, all_page, pathmap)})
            continue
        if isinstance(entry, tuple):
            title, pattern = entry
            if pattern == "@all-instructions":
                nav.append({title: all_page})
                continue
            files = match(pattern, universe)
        else:
            title, pattern, files = None, entry, match(entry, universe)
        if not files:
            problems.append(f"policy pattern matches nothing: {pattern}")
        for f in files:
            if f in used:
                continue  # a glob repeating an explicit entry: first placement wins
            used.add(f)
            nav.append({title or page_title(f): pathmap[f]})
    return nav

# ---------------------------------------------------------------------------
# Copy with link rewriting
# ---------------------------------------------------------------------------
LINK_RE = re.compile(r'(\]\()([^)\s]+)((?:\s+"[^"]*")?\))')
HTML_RE = re.compile(r'((?:href|src)=")([^"]+)(")')

def rewrite(text, src_repo, pathmap, extra_assets):
    src_dir = os.path.dirname(src_repo)
    dst_dir = os.path.dirname(pathmap[src_repo])

    def fix(target):
        if re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", target) or target.startswith(("#", "//")):
            return target
        path, _, anchor = target.partition("#")
        if not path:
            return target
        path = urllib.parse.unquote(path)
        resolved = os.path.normpath(os.path.join(src_dir, path)) if not path.startswith("/") else path.lstrip("/")
        resolved = resolved.replace(os.sep, "/")
        if resolved in pathmap:
            new = rel(dst_dir, pathmap[resolved])
        else:
            abs_path = os.path.join(ROOT, resolved)
            if os.path.isfile(abs_path) and resolved.lower().endswith(IMAGE_EXT):
                pathmap[resolved] = resolved
                extra_assets.add(resolved)
                new = rel(dst_dir, resolved)
            elif os.path.isdir(abs_path):
                new = f"{GITHUB}/tree/{BRANCH}/{resolved}"
            elif os.path.isfile(abs_path):
                new = f"{GITHUB}/blob/{BRANCH}/{resolved}"
            else:
                return target  # unresolved: leave it, mkdocs --strict will say so
        if " " in new and not new.startswith("http"):
            new = urllib.parse.quote(new)
        return new + ("#" + anchor if anchor else "")

    text = LINK_RE.sub(lambda m: m.group(1) + fix(m.group(2)) + m.group(3), text)
    text = HTML_RE.sub(lambda m: m.group(1) + fix(m.group(2)) + m.group(3), text)
    return text

EXT_IMAGE = re.compile(r"!\[([^\]]*)\]\((https?://([^/)\s]+)[^)\s]*)\)")

def unhost_external_images(text, repo_path):
    def fix(m):
        alt, url, host = m.group(1), m.group(2), m.group(3)
        if host in IMAGE_HOSTS_OK:
            return m.group(0)
        print(f"gen_docs_site: {repo_path}: external image from {host} published as a link", file=sys.stderr)
        return f"[{alt or url}]({url})"
    return EXT_IMAGE.sub(fix, text)

LIST_ITEM = re.compile(r"^\s*([-*+]|\d+[.)])\s+")
FENCE = re.compile(r"^\s*(```|~~~)")

def blank_line_before_lists(text):
    out, prev, in_fence = [], "", False
    for line in text.split("\n"):
        if FENCE.match(line):
            in_fence = not in_fence
        elif (not in_fence and LIST_ITEM.match(line) and prev.strip()
              and not LIST_ITEM.match(prev) and not prev.startswith((" ", "\t", "|", "#"))):
            out.append("")
        out.append(line)
        prev = line
    return "\n".join(out)

def copy_pages(used, pathmap):
    extra = set()
    for repo_path in sorted(used):
        src = os.path.join(ROOT, repo_path)
        if not os.path.isfile(src):
            continue  # generated page, already written
        with open(src, encoding="utf-8", errors="replace") as fh:
            text = fh.read()
        text = rewrite(text, repo_path, pathmap, extra)
        text = unhost_external_images(text, repo_path)
        text = blank_line_before_lists(text)
        dest = os.path.join(OUT, "docs", pathmap[repo_path])
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(dest, "w", encoding="utf-8") as fh:
            fh.write(text)
    for asset in sorted(extra):
        src = os.path.join(ROOT, asset)
        dest = os.path.join(OUT, "docs", asset)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(src, "rb") as a, open(dest, "wb") as b:
            b.write(a.read())
    return extra

def copy_theme_assets():
    for name in ("nd500x.svg", "nd500x.ico"):
        src = os.path.join(ROOT, "assets", name)
        dest = os.path.join(OUT, "docs", "assets", name)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(src, "rb") as a, open(dest, "wb") as b:
            b.write(a.read())

# ---------------------------------------------------------------------------
def main():
    check = "--check" in sys.argv
    include_untracked = "--include-untracked" in sys.argv
    files = git_files(include_untracked)
    universe = md_universe(files)
    problems = []

    by_fn, total = load_instructions()
    instr_tree = instruction_tree(by_fn, universe, problems)

    # path map for every candidate page (universe) plus the generated index
    pathmap = {f: dest_path(f) for f in universe}
    all_page = "docs/instructions/ALL-INSTRUCTIONS.md"
    pathmap[all_page] = all_page

    used = set()
    nav = expand(SECTIONS, universe, used, problems, instr_tree, all_page, pathmap)

    off = set()
    for pat in OFF_SITE:
        off.update(match(pat, universe))
    unplaced = [f for f in universe if f not in used and f not in off]
    for f in unplaced:
        problems.append(f"tracked document not covered by the policy (add to SECTIONS or OFF_SITE): {f}")
    both = sorted(used & off)
    for f in both:
        problems.append(f"listed both in a section and in OFF_SITE: {f}")

    if problems:
        for p in problems:
            print("gen_docs_site:", p, file=sys.stderr)
    print(f"gen_docs_site: {len(used)} pages in nav, {len(off)} off the site, "
          f"{len(instr_tree)} instruction classes, {sum(len(p) for _, _, p in instr_tree)} instruction pages",
          file=sys.stderr)
    if check:
        sys.exit(1 if problems else 0)
    if problems:
        sys.exit(1)

    # ---- write build-site/
    import shutil
    if os.path.isdir(OUT):
        shutil.rmtree(OUT)
    os.makedirs(os.path.join(OUT, "docs"))
    write_all_instructions(by_fn, total, pathmap, used)
    extra = copy_pages(used, pathmap)
    copy_theme_assets()

    import yaml
    child = {"INHERIT": "../docs-site/mkdocs.yml", "docs_dir": "docs", "nav": nav}
    with open(os.path.join(OUT, "mkdocs.yml"), "w", encoding="utf-8") as fh:
        fh.write("# generated by tools/gen_docs_site.py - do not edit, regenerate\n")
        yaml.safe_dump(child, fh, sort_keys=False, allow_unicode=True, width=200)
    print(f"gen_docs_site: wrote {OUT} ({len(extra)} images copied)", file=sys.stderr)

if __name__ == "__main__":
    main()
