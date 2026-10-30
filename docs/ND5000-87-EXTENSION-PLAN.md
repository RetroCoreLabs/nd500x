# Plan: the ND-5000 '87 extension instructions

This lists the work that remains. What is done is not listed. The instructions,
their opcodes, the counts and the manual text are in
[ND-5000 '87 extension instructions](ND5000-87-EXTENSION-INSTRUCTIONS.md); that page is
generated, and its status column shows how far the data is.

## What is wrong today

Measured on 30 October 2026:

- 13 of the 25 '87 instructions are missing from `docs/instructions/instructions.json`:
  AMODB, ENTIER, PLCCN, NCPLC, CLINIT, CLREAD, RHOLE, WHOLE, SEND, RECVE, REXT, WEXT,
  TOSSP. So they have no dispatch entry, no handler, no disassembly and no assembler
  support. (CAD is not missing: the data has it as `cad:=` at 0xFDBA, the manual's opcode.)
- DDIRT is in the data at 0xFE1E. The manual (section 16.11) says 0xFFFA.
- The installed assembler `nd500-as` on the path (dated 14 October 2026) answers
  "unknown instruction" for the 13 missing ones. For DDIRT it emits 0xFE1E.
- The installed disassembler `nd500-dis` on the path prints `???` for the opcodes
  0xFFFD, 0xFFFE, 0xFF1E, 0xFF1F, 0xFE9E, 0xFE9D, 0xB6, 0xB7, 0xFFE8, 0xFFEC, 0xFE9F and
  0xFFFA, and prints `ddirt` for 0xFE1E. It decodes `cad:=` (0xFDBA).

## Step 1 - the tools (found 30 October 2026)

The source of all the `nd500-*` tools is the repository the pcc-nd500 repository
(remote RetroCoreLabs/pcc-nd500, local branch main is 2 commits ahead of origin). One table
feeds the others:

| Tool | Source folder under `src/` of the pcc-nd500 repository | Where its instruction knowledge is |
|---|---|---|
| nd500-as (assembler) | `nd500-as/` | `instrtab.h`, `codetab.h`, `oprndtab.h` - the master tables |
| nd500-opcodes | `nd500-opcodes/` | reads the assembler tables and writes `instructions.json`, C, C# (`Instructionset.Init.cs`) and the disassembler header; the installed program counts 538 entries, not 1078 |
| nd500-dis (disassembler) | `nd500-dis/` | `nd500_instructions.h`, written by `nd500-opcodes -f disasm` (its header says DO NOT EDIT) |
| nd500-dump, nd500-ld, nd500-cc/cc1/cc2/cpp, nd500-mkfs, nd500-mkproto, nd500-splitseg, nd500-symorder | `nd500-*/` | not checked whether any of them has an instruction table; the dump, ld and cc2 programs contain no `svers`/`rphs` text, so probably not |

So the 13 instructions (and the DDIRT correction) go into the assembler tables first, then
`nd500-opcodes` is rebuilt and its outputs regenerated. Still to find out from files, before
any change:

1. Why the installed `nd500-opcodes` says 538 entries while
   `docs/instructions/instructions.json` has 1078 (a newer build, or a different tool).
2. Where `src/cpu/nd500_instructions.c` in this repository comes from (`nd500-opcodes -f c`
   or something else); it has no generator in this repository.
3. How the assembler tables are written (by hand, or generated), and whether the
   `docs` folder of `nd500-as` says how to add an instruction.
4. Opcodes of AMODB and ENTIER are in the manual (sections 10.36 and 10.37): AMODB 0xFFBC to
   0xFFC7 (BY, H, W, four registers each; the manual's hex column has the misprint FFCOH for
   0xFFC0, the octal column 177700B is right) and ENTIER 0xFDC7 (F) and 0xFDC8 (D). The
   earlier "not read" was a fault in `tools/gen_87_extension_doc.py` (it took the manual's
   table of contents entry instead of the section); fixed. Still to be confirmed against the
   microcode.
5. DDIRT: 0xFE1E or 0xFFFA? Check the manual page, the microcode decode and the
   existing handler `src/cpu/instructions/SYSTEM/` DDIRT file and its tests.
6. Whether the other `nd500-*` tools need anything (a linker or dump that prints
   mnemonics, the C compiler back end emitting these instructions).

## What the checks found (30 October 2026)

Sources searched: the ND-500 Reference Manual, every other manual and source file in
NDInsight, and the ND-5000 microcode with RetroCore's opcode table for it. Only the manual
gives opcodes. NDInsight has no other document that does, and RetroCore's table is rebuilt from
the manual and the microcode label names, so it is no second source for the opcode values.
The microcode proves that a routine with the right name exists.

| Group | Instructions | Decision |
|---|---|---|
| Opcode from the manual (octal and hex columns agree) and a microcode routine of that name exists | SEND 0xB6, RECVE 0xB7, REXT 0xFFE8-0xFFEB, WEXT 0xFFEC-0xFFEF, WHOLE 0xFE9D, CLINIT 0xFF1E, CLREAD 0xFF1F, PLCCN 0xFFFD, NCPLC 0xFFFE, TOSSP 0xFE9F, ENTIER 0xFDC7 and 0xFDC8 | Added to the tables |
| No microcode routine | RHOLE 0xFE9E | Not added. The ND-5000 microcode has no RHOLE label and no entry for 0xFE9E. |
| Manual only, the microcode sends it to the illegal-instruction stub | AMODB 0xFFBC-0xFFC7 | Not added until the behaviour on the ND-5000 is known |
| Conflict | DDIRT: manual 0xFFFA; data and assembler 0xFE1E | Both map to the microcode routine DCDUMP. 0xFE1E is tied to chapter 12 of the ND-5000 Microprogram Guide. So 0xFE1E stays and 0xFFFA is added as a second encoding of the same instruction. |

## Not proven - to take up with the original CPU designer

Written down because a table entry that cannot be proven is not guessed. Each item says what
is known and what is only inferred. Where an item is open, the tools carry the best-supported
value and the item stays here until someone who designed the instruction answers.

1. **RHOLE (manual 0xFE9E).** The ND-5000 microcode (MICRO-5800-B30) has no RHOLE routine and no
   entry for that opcode, and NDInsight calls its existence open. Question: does RHOLE exist,
   and is 0xFE9E right? Not in the tools.
2. **AMODB (manual 0xFFBC to 0xFFC7).** The microcode sends every one of these opcodes to its
   illegal-instruction stub. Question: is AMODB executed in hardware or by trap handling, and on
   which microcode version? Not in the tools.
3. **DDIRT.** The assembler tables (from the NDIX-C assembler) have 0xFE1E; the manual has
   0xFFFA; the microcode runs the same routine (DCDUMP) for both. The tools accept both when
   disassembling and assemble 0xFE1E. Question: which is the real encoding, or are both valid?
4. **Operand rules of the 11 added instructions.** The manual gives the format line and the
   access (read or write) of each operand. The assembler needs more: which addressing modes
   are allowed, and the operand size. These were copied from the nearest existing instruction
   with the same format (the instruction named for each in the worksheet), not read from a
   source. Question: do these instructions accept every general addressing mode, including
   constants and pre-indexed, for the read operand? For WHOLE: is the operand form the same
   as SMOVE's string descriptor? For REXT and WEXT: is the device operand a word, any
   mode?
5. **SEND and RECVE register.** The manual shows only W1. The assembler now refuses other
   registers. Question: is W2 to W4 undefined, or is it just not documented?
6. **TOSSP spelling.** The manual's format reads `TOSSP := <operand>`, the code table `TOSSP`.
   The tools use `tossp:=` like `tos:=` and `cad:=`.
7. **Classes** (SYSTEM, IO, FLOAT_MATH, MOVE) are chosen by the name rules in `nd500-opcodes`
   and are this project's grouping, not the manual's.
8. **Privileged.** Which of the 25 are privileged is only what the manual's text says (CLINIT,
   REXT, SCNTXT and others); instructions where the manual is silent are listed as "not stated".

## Step 2 - decide the order

The 14 fall into four groups by what they need. Ronny decides whether all four groups
are done, and in which order.

| Group | Instructions | Needs |
|---|---|---|
| A. plain CPU work | AMODB, ENTIER, PLCCN, NCPLC, TOSSP, CAD | operand decoding, arithmetic, descriptor formats from the manual, domain registers (CAD) |
| B. local clock | CLINIT, CLREAD | the existing clock code `src/cpu/instructions/SYSTEM/Clock.c`; the manual says what the clock does |
| C. nucleus memory | RHOLE, WHOLE | privileged (PIA bit) and the memory management model |
| D. other CPUs and devices | SEND, RECVE, REXT, WEXT | ports between CPUs and external devices; depends on the ND-5000 / octobus model in the emulator |

For any instruction whose behaviour is not yet emulated, the first version raises the
trap the manual names for it. Nothing is silently ignored.

## Step 3 - the data

Order matters, because each file is made from the one before it:

1. **Assembler tables** (`instrtab.h`, `codetab.h`, `oprndtab.h` in `nd500-as` of the
   pcc-nd500 repository). These are the master. They carry no "generated" marker, so they
   are edited by hand in the style of their neighbours. They have to say which instructions
   are '87 extension (a new column or flag per instruction), because the later files cannot
   know it otherwise.
2. **`nd500-opcodes`** reads those tables. It gets a new field `extension` ("87" or empty)
   on every entry in the JSON, C and C# outputs and in the disassembler header.
3. **Regenerate** `instructions.json` (and the copy in this repository),
   `Instructionset.Init.cs` and `nd500-dis/nd500_instructions.h`. Never edit them by hand.

For each of the 13 instructions the table entry has the fields the neighbouring entries have
(opcode, mnemonic, class, data type prefixes, operand count and templates, allowed modes),
with operand types from the manual's "Format" line and opcodes from its code table (the octal
column; the hex column has misprints, for example FF7CC for SCPUNO). The class is one of the
existing 13. Then `tools/gen_87_extension_doc.py` counts from the `extension` field and fails
if it disagrees with the manual's '87 labels.

## Rule for every table entry: proof before it goes in

No entry is added to the assembler tables from memory or by analogy. For each of the 13
instructions (and the DDIRT correction), every field is written down with its source before
the table is touched, in a worksheet kept next to this plan:

| Field | Source that must agree |
|---|---|
| opcode | the manual's octal column, the manual's hex column (a mismatch is a misprint to explain, not to guess), and the ND-5000 microcode decode where it is readable |
| operand count and types | the manual's "Format" line, compared with an existing instruction that has the same format |
| operand access (read, write) | the manual's "Operation" line |
| class | the class of the nearest existing instruction, named in the worksheet |
| privileged | the manual's "Privileged instruction" text |

Then, after the entry is in:

1. Assemble one line per instruction; the bytes must equal the manual's opcode plus the
   operand bytes worked out by hand from the addressing mode rules.
2. Disassemble those bytes; the text must match the line.
3. Run the existing 1078 entries through before and after: the old entries must be
   byte-for-byte unchanged (diff of the generated JSON), only the new ones and DDIRT differ.
4. Anything not proven by at least two independent sources is not added; it stays on the
   list as unproven.

The checking is done by script and by reading the sources, not left to Ronny. The tables are edited only when every row of the worksheet agrees between its sources; a row that disagrees stops that instruction.

## Step 4 - the emulator

For each instruction:

1. Information-table row and dispatch entry in `src/cpu/nd500_instructions.c`
   (via its generator if there is one).
2. Handler `src/cpu/instructions/<CLASS>/<FunctionName>.c`, K&R-free ordinary C11,
   no Unicode in comments. Prototypes are produced by mkptypes at build time.
3. Privileged instructions raise IIC when the PIA status bit is reset, using the
   existing helper in `src/cpu/instruction_helpers.c`.
4. Data status bits and trap conditions exactly as the manual lists them for that
   instruction, not copied from a similar instruction.

## Step 5 - the tools (assembler, disassembler, the rest)

All in `src/` of the pcc-nd500 repository (a separate repository from this one):

- `nd500-as`: add the 13 mnemonics and their operand forms to its tables; correct DDIRT if
  Step 1, question 5 shows 0xFFFA. Mnemonics are lower case.
- `nd500-opcodes`: rebuild, then regenerate `nd500-dis/nd500_instructions.h`,
  `instructions.json` and `Instructionset.Init.cs` from it; never edit the generated files.
- `nd500-dis`: rebuild with the new header; the 13 opcodes must print their mnemonic and
  operands instead of `???`.
- Other `nd500-*` tools: check each one (Step 1, question 7); change only the ones that
  have an instruction table or print mnemonics.
- Install the rebuilt tools where the old ones are (Ronny's call; the old assembler is kept as
  `nd500-as.before-2026-10-14`).
- Round trip for every '87 instruction: assemble, disassemble, compare with the manual's
  opcode. Entries in this repository's `test/disasm_tests.c` and the `src/disasm` of this
  repository (a second disassembler) follow.

## Step 6 - tests

- A C test per instruction in the style of `test/test_rphs_wphs.c`: opcode, operand forms,
  result, status bits, traps.
- Entries in `test/disasm_tests.c`.
- A script check that every opcode the manual labels '87 has a dispatch entry:
  `tools/gen_87_extension_doc.py --check` keeps the page current, and a second check
  compares its opcode list with the dispatch table.
- Meaning of each instruction is checked against the ND-5000 microcode where it is
  readable, since the C# emulator is not a reference.

## Step 7 - documents and counts

- One page per new instruction in `docs/instructions/asm/`, written from the manual under
  `docs/instructions/DOCUMENTATION_STANDARDS.md`; class pages in `docs/instruction-reference/`.
- Regenerate the '87 page (its status column changes) and the all-instructions index.
- Numbers that are written into scripts or pages and change with the data:
  `tools/gen_isa_figures.py` (asserts 241, 1078, 244 one-byte and 834 two-byte entries and
  prints them in the figures), `tools/gen_opcode_matrix.py` (asserts 1078 and a list of
  shared opcodes), `README.md`, `CLAUDE.md`, `docs/INSTRUCTION-SET-OVERVIEW.md`.
- Opcode matrix (`tools/gen_opcode_matrix.py`): "'87 extension" as a choice in the filter, next to the class
  and data type choices, combinable with them; it dims every other cell and shows the hit count on the map tabs
  like the other filters. The popup of a cell says "ND-5000 '87 extension" when it is one. The counts for
  ND-500 base and for '87 are shown on the page. The data comes from the `extension` field.

## Step 8 - records

- A line in `docs/SYNC-BACKLOG.md` for every commit that changes CPU behaviour.
- A numbered item in the shared C# note `$ND500_TESTDATA/retrocore-mon-fixes.md`, if the
  C# emulator is also missing these instructions (not checked).
