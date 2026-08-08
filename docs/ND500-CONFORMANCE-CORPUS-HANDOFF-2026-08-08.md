# ND500 Conformance Corpus — handoff

**Written 2026-08-08.** For whoever picks up ND-500 / ND-5000 emulator correctness.
Everything here was measured or executed, not assumed. Where something is a
guess or an open question it says so.

---

## 1. What the corpus is

One JSON file of single-instruction test cases. **RetroCore generates it,
nd500x runs it.** A case says: put the machine in this state, execute this one
instruction, expect this state and/or this trap condition. Both emulators must
agree with it, so a disagreement between them shows up as a failing case rather
than as nothing at all.

It is **not** produced by running either CPU. The expectations are computed
analytically by RetroCore's generators (`FlagCalculator` and friends). This
matters: a generator bug produces a confidently wrong expectation, and the
emulator then gets "fixed" to match it. That has happened before.

| | path |
|---|---|
| Generator side | `E:\Dev\Repos\Ronny\RetroCore\Emulated.Tests.ND500\Validation\` |
| Corpus file (source) | `~/repos/nd500x/test/nd500-conformance.json` |
| Corpus file (build copy) | `~/repos/nd500x/build/bin/nd500-conformance.json` |
| C runner | `~/repos/nd500x/test/test_conformance.c` |
| ctest name | `conformance` |
| C# runner | `TestComprehensiveExportAndRun.RunConformanceCorpus` |

**Renamed 2026-08-08** from `nd500_tests.json` / `test_instruction_validation.c`
/ ctest `instruction_validation` / `Execute_Tests_From_Master_JSON`. Some
`docs/*.md` analysis notes still use the old names on purpose — they are
write-ups of past investigations and rewriting them would falsify what was run
at the time.

**Current state: 40088 cases, 100% pass, ctest 34/34.**

---

## 2. The loop (exact commands)

Regenerating is awkward because the RetroCore fixture carries `[Explicit]`, and
NUnit **excludes explicit tests from `--filter` runs** — so the obvious command
silently matches nothing. The working procedure comments the attribute out, runs,
and always restores it (`try/finally`):

```powershell
# scratchpad\cycle.ps1 does all of this
$f = 'E:\Dev\Repos\Ronny\RetroCore\Emulated.Tests.ND500\Validation\TestComprehensiveExportAndRun.cs'
# 1. comment out the [Explicit(...)] on the fixture (2 lines)
# 2. dotnet test Emulated.Tests.ND500 --filter "Generate_Master_JSON"
# 3. ALWAYS restore the attribute in a finally block
```

Then install and run:

```powershell
Copy-Item 'E:\Dev\Repos\Ronny\RetroCore\Emulated.Tests.ND500\bin\Debug\net9.0\nd500-conformance.json' `
          '\\wsl$\Ubuntu\home\ronny\repos\nd500x\test\nd500-conformance.json' -Force
```

```bash
cd ~/repos/nd500x/build && cmake .. && make -j8
ctest                       # whole suite
ctest -R '^conformance$'    # the corpus alone
./bin/test_conformance --continue          # full run, does not stop at first failure
./bin/test_conformance --filter mul        # by instruction name
```

**Always check the count, not just the colour.** `Results: 40088 passed, 0 failed`.
A test that stops existing also leaves a green suite — that is exactly how GETB
hid (see §6).

---

## 3. Case schema — what a case can assert

```jsonc
{
  "name": "...",
  "assembly": "W1 GETB $2",
  "bytes": [ ... ],                 // assembled; a case that will not assemble is DROPPED
  "initial": { "regs": { ... }, "ram": [[addr, value], ...] },
  "final":   { "regs": { ... }, "ram": [ ... ] },
  "expectedTrap": "StackOverflow",  // five-token name, LOSSY - see below
  "expectedTrapBits": 134217728,    // EXACT ND-500 condition mask, 64-bit
  "maxInstructions": 1,
  "requiresCallContext": true       // ENT* need a preceding CALL or they raise ISE
}
```

**Registers are nullable with `NullValueHandling.Ignore`.** A register not set in
the scenario is absent from the JSON and is NOT compared. So a case can assert
*only* a trap condition and nothing else — which is the right thing when the
manual documents the condition but not the aftermath.

Supported register keys: `i1-i4, a1-a4, e1-e4, pc, p, l, b, r, st, st2, tos, ll`.
`st2` was added 2026-08-08 — it carries XSE(32), IIC(33), IOS(34), ISE(35),
PV(36) and the fault bits, **none of which any case could express before**, which
is why an audit finding 71 of 86 shared instructions differing in exactly those
guard traps was invisible to the whole suite.

### ⚠ RAM entries: byte vs 32-bit is chosen by VALUE

`test_conformance.c`:

```c
if (val <= 255)  nd500_bus_write8(m, addr, val);      /* single byte */
else             /* 32-bit big-endian across addr..addr+3 */
```

So writing a **small** 32-bit value needs four explicit byte entries. Writing
`MAXL = 4` at `TOS+0` as the number 4 puts it in the most significant byte of a
big-endian word, giving `MAXL = 0x04000000`. That silently makes an
"above MAXL" test meaningless. It bit me; write the bytes.

---

## 4. Trap conditions — the bit map and the two ambiguous tokens

Authoritative bit numbers, `src/cpu/cpu_protos.h`:

```
O    9   Integer Overflow          ATF  21  Address Trap Fetch
IVO 11   Invalid Operation         ATR  22  Address Trap Read
DZ  12   Divide by Zero            ATW  23  Address Trap Write
FU  13   Floating Underflow        AZ   24  Address Zero Access
FO  14   Floating Overflow         DR   25  Descriptor Range
BO  15   BCD Overflow              IX   26  Illegal Index
IOV 16   Illegal Operand Value     STO  27  Stack Overflow
SIT 17   Single Instruction Trap   STU  28  Stack Underflow
BT  18   Branch Trap               PRT  29  Programmed Trap
CT  19   Call Trap                 DT   30  Disable proc. switch Timeout
BPT 20   Breakpoint Trap           DE   31  Disable proc. switch Error

XSE 32  IIC 33  IOS 34  ISE 35  PV 36  THM 37  PGF 38  PWF 39  PRF 40  HF 41
```

`ST1` = bits 0-31, `ST2` = bits 32-41. **ST1 flags and trap-condition bits are the
same bits** (`ND500_FLAG_O == TRAP_O == 1<<9`).

NDIX arms `T_CMTE1 = 0xF413D800` — bits 11,12,14,15,16,17,20,26,28,29,30,31.
**Not 9.** So reporting arithmetic overflow as IVO(11) instead of O(9) delivers a
trap to a guest that expects to ignore it. That was a real bug in ~15 sites.

### The `TrapType` token vocabulary is lossy

| token | bits it could mean |
|---|---|
| `DivisionByZero` | DZ(12) — unambiguous |
| `IllegalInstruction` | IIC(33) — unambiguous |
| `InstructionSequenceError` | ISE(35) — unambiguous |
| `StackOverflow` / `StackUnderflow` | STO(27) / STU(28) — unambiguous |
| `ProtectionViolation` | PV(36) — unambiguous |
| `IntegerOverflow` | O(9) — **added 2026-08-08**, there was no token for bit 9 at all |
| `Overflow` | BO(15) |
| **`FloatException`** | **IVO(11) or FU(13) or FO(14)** — ambiguous |
| **`IllegalOperandValue`** | **IOV(16) or IOS(34)** — ambiguous |

`ComprehensiveTestExporter.DeriveTrapBits` maps the unambiguous ones and
deliberately returns 0 for the two ambiguous ones. **Do not "fix" that by
picking a bit.** Those two tokens are exactly where the two emulators disagree,
so a derived bit bakes in whichever CPU is wrong. A case that wants them pinned
sets `ExpectedTrapBits` itself, justified from the manual — e.g. IFSTGO's section
says "illegal operand value" (IOV 16) while JUMPG's says "illegal operand
specifier" (IOS 34).

### Two channels, and why both count

1. **Trap channel** — `expectedTrap` + `expectedTrapBits`.
2. **Status channel** — the expected `st` value.

An ignorable condition that is **not enabled in OTE never dispatches a trap**; it
only sets its ST1 bit. So `neg` on the greatest negative integer pins O(9)
through the expected ST value with no trap at all. For those conditions the
status channel is not weaker coverage — it is the only coverage available.

The first cut of the coverage gate counted only the trap channel and reported
20 covered / 174 holes. Counting both gives **47 / 147**. Acting on the first
report would have generated duplicates of tests that already existed.

The C runner's ignorable-trap scan (`test_conformance.c`) is table-driven over
O, IVO, DZ, FU, FO, BO, IOV, STO, STU. It previously had **no branch for O(9)**,
so the corpus was structurally blind to its own worst bug class.

---

## 5. The coverage gate

`TrapConditionSpec.cs` is the ND-500 Reference Manual's own **"Trap conditions:"**
line for all **144** instructions that carry one, generated from the manual text
embedded in `docs/instructions/yaml/*.yaml` (`manual_reference.content`). Each
entry keeps ND's sentence verbatim so the mapping can be checked against the
source words. Regenerate it with `scratchpad\genspec.py` if the YAMLs change.

`TestND500_TrapCoverage.cs` scores the corpus against it and holds a **ratchet**
(`CoveredPairsBaseline`). It prints every hole by name.

```
documented (instruction, condition) pairs : 194
  covered                                 :  47   (30 via trap, 17 via ST only)
  HOLES                                   : 147
ADDRESSING family (reported, not gated)   : 119
```

`ADDRESSING` is a family (ATF/ATR/ATW + PRT + PGF), not one bit, and provoking it
needs MMU state rather than operand values — so it is recorded with bit `-1` and
not gated.

### Remaining holes

| condition | holes | why it is blocked |
|---|---|---|
| IOV(16) | 44 | mostly the `if*go` family — see below |
| IVO(11) | 23 | ambiguous token; needs a ruling |
| BT(18) | 16 | armed by trap-enable state, not operand values |
| FO(14) | 13 | ambiguous token |
| O(9) | 11 | byconr/fconr/hconr/wconr/cind/ixi/invc/mulad/psum/pwconv — doable |
| BO(15) | 11 | packed-BCD group; needs valid/overflowing BCD operands |
| FU(13) | 9 | ambiguous token |
| IOS(34) | 7 | call/callg/jumpg/poly/rdus/test/tset |
| singles | 13 | BPT, CT, ISE, IX, DZ(rem), IIC(riom), DT, DE |

**The other 15 `if*go` mnemonics cannot express their IOV at all.** The manual's
rule is about the *second format* — the status-bit form — and in this assembler
that form **is** the separate `ifstgo` / `if-stgo` mnemonic. `if=go` and friends
take only a displacement, so there is no bit number to make illegal.

---

## 6. Open defects found and NOT fixed

These are real, measured, and deliberately left. Ronny has already ruled on the
first two (see §7).

### (a) 45 instructions are generated with the wrong data type

`VariantDrivenGenerator.GenerateForMnemonic` maps `variantNumber` → data type
(0=BY, 1=H, 2=W, 3=F, 4=D). That is only meaningful when an instruction's
variants really *are* the data types. Audit of `instructions.json` (1078 entries)
found **45 mnemonics** whose generated types contradict their declared prefixes:

```
mnemonic     n  variantNumbers  declares   generates  MISSING  BOGUS
getb         4  0               W          BY         W        BY
addc/subc    4  0               W          BY         W        BY
invc         4  0               W          BY         W        BY
chain        4  0               W          BY         W        BY
pwconv       4  0               W          BY         W        BY
phyladr      4  0               W          BY         W        BY
udiv/umul    4  0               W          BY         W        BY
riom         1  0               H          BY         H        BY
sin cos tan sqrt exp asin acos atan atan2 alog alog2 alog10 int intr poly rem axi
             8  0,1             F/D        BY/H       F/D      BY/H
byconr hconr wconr fconr
             2  0,1             F/D (W/D)  BY/H       F/D      BY/H
and inv or xor rdus
            16  0,1,2,3         BY/H/W     BY/F/H/W   -        F
byconv dconv fconv hconv wconv
             5  0..4            (varies)   BY/D/F/H/W -        one bogus each
```

Every transcendental is generated with a BY/H prefix it does not have. This is
most of the **1708 scenarios per run that fail to assemble and are dropped**
(FLOAT_MATH alone: 290). The exporter *does* report them by class — that is how
GETB surfaced — but nobody was reading it.

**GETB is the sharpest case: the corpus has NEVER contained a single GETB test.**
Its four opcodes `0xFE4C..0xFE4F` are the registers W1..W4, all carrying
`variantNumber 0`, so every variant became `BY1 GETB`, which the assembler
correctly refuses. A grep for GETB in the corpus finds only GETBF and GETBI. The
new documented-STO cases are gated on the **opcode** for exactly this reason.

> ⚠ **`variantNumber` is not a spare field.** `OpcodeTable.cs:2623` uses it for
> LOOP displacement sizing: `directSize = (jsonInstr.VariantNumber < 5) ? 1 : 2`
> — the `:B` forms are variants 0-4 and the `:H` forms 5-9. Renumbering variants
> to mean "data type" **would break the assembler**. The safe data fix is to add
> an explicit per-variant `dataType` field and have the framework read that,
> leaving `variantNumber` meaning "index within the mnemonic's variant list".

### (b) DIV2 / DIV3 expectations use unsigned division

`CreateDiv2Scenario` / `CreateDiv3Scenario` compute the expected quotient as
`val1 / val2` on `uint`, while the one-operand `/` path sign-extends first and
divides signed. Latent today — the only inputs are 100/10, 100/3, 0/5, all
positive — but any negative operand yields a confidently wrong expectation. This
is why the documented overflow cases bypass those paths entirely: feeding the
greatest negative integer and -1 through them would produce `0`, not a trap case.
(Also note `int.MinValue / -1` **throws** `OverflowException` in C#, so the signed
path must special-case it or generation dies.)

### (c) The `i < 5` caps

`GenerateAdd2Scenarios` and friends cap their edge-case loops at `i < 5`, which
truncates the list before the extremes — so no pair they emit can overflow. That
is why ADD2/MUL2/DIV2/DIV3 had no O(9) coverage while the uncapped one-operand
`+`, `-`, `*` did. Removing the caps multiplies case counts; measure before
committing to it.

### (d) Existing GETB expectations rest on undefined behaviour

The pre-existing `Getb_LogSize*` scenarios never set TOS, so GETB reads its heap
variables from address 0 and they expect "returns 0, Z set". If the data fix in
(a) makes them assemble, they will start executing — and that expectation is not
from the manual. Adjudicate before trusting a failure there.

### (e) 12 behavioural divergences between the emulators

`add, byconr, dconv, div4, entb, fconr, fconv, hconr, psum, pwconv, retb, wconr`.
Plus 71 guard-only divergences (nd500x raises IOS where RetroCore raises
ISE/IIC) — direction unknown, untouched.

---

## 7. Rulings already given (do not re-ask)

- **Fix `instructions.json`, the data** — not the framework, not per-generator
  overrides. Correct the source so the mapping is fed true data, and fix every
  instruction of the same shape at once. Mind the `variantNumber` coupling above.
- **Fix the DIV2/DIV3 signed-division bug properly**, and validate all scenarios
  and edge cases. *"If there are bugs we root cause and fix the bugs. We never
  reduce quality. We always fix the known bugs."*
- **Settle the ambiguous IVO/IOV cases from the oracle** — find the oracle
  result, **validate that it is not misunderstood or itself wrong**, then use it
  going forward.
- Naming: **ND500 Conformance Corpus**. Exact bits plus systematic coverage.

---

## 8. Gotchas that cost real time

- **`[Explicit]` + `--filter` matches nothing.** See §2.
- **Line endings**: RetroCore C# sources are **LF**. Writing CRLF into them once
  produced a 5,824-line phantom diff. Detect and preserve.
- **Shared trees.** Both repos have other sessions working in them concurrently.
  Stage only exact paths (`git add -- <path>`); never `git add -A`. If the index
  already holds someone else's staged files, commit with a pathspec
  (`git commit -- <paths>`) — but then **the old paths of a rename are not
  included**, which left `nd500_tests.json` undeleted and needed a follow-up
  commit.
- **`wsl bash -c '...'` from PowerShell eats `$variables`.** Put the script in a
  file and run `wsl bash /mnt/c/...`.
- **The 1988 guest tty has a small canonical input buffer** — a ~250-char command
  line wedges it until Ctrl-C. And its `ping` has no `-c`: `ping -c 3 host`
  pings the host named "3".
- **Read the code, not the file header.** `Abs.c`'s header quotes the manual
  saying "S=0 always"; twenty lines below, a microcode adjudication proves the
  prose wrong.

---

## 9. Commits

**nd500x** (branch `fix/deabf-i1-success-and-load-investigation` at time of
writing):

```
712ba00  A CPU reset left the cycle counter running, so a stale SOLO region trapped
11baec3  Correct the /etc/halt rationale, and verify the guest changes by booting it
8d9c511  Conformance corpus: teach the runner ST2 and exact trap-condition bits
75f4ed3  Conformance corpus: exact trap bits enforced, and a scan that can see O(9)
fd17b39  Conformance corpus: the first cases generated from ND documented trap conditions
921770e  Conformance corpus: the documented stack-overflow cases, and GETB at last
4c29fd9  Conformance corpus: the documented illegal-bit-number cases for IFSTGO
424c552  Rename the fixture and its runner to the ND500 Conformance Corpus
61f1a55  Drop the old fixture and runner paths left behind by the rename
```

**RetroCore** (branch `ethernet-ii-controller-fixes`):

```
5fe5eaa6e  ND500 Conformance Corpus: exact trap bits, a manual-derived coverage
           gate, and the trap fixes behind them        (41 files)
```

That RetroCore commit also carries the emulator fixes: `CpuND500.TrapIntegerOverflow`
plus the ~20 sites that were reporting arithmetic overflow as `TrapInvalidOperation`,
and the assembler's LOOP displacement rule corrected from variant *parity* to
variant *group*.

---

## 10. Where the trap cases came from

Every documented-trap case is justified by a sentence in the manual, and the
sentence is quoted in the code next to it. Three worked examples of the standard:

- **`/` integer overflow** — 11.4: *"Integer overflow occurs if and only if the
  largest possible negative integer is divided by -1."* One input, exactly.
- **`INIT` stack overflow** — 13.9: *"A value of &lt;stack demand of main program&gt;
  greater than or equal to &lt;total system stack demand&gt; will cause a stack
  overflow trap condition."* Both halves generated — equal and greater — because
  a rule coded as `>` instead of `>=` passes the greater case and fails only the
  equal one.
- **`IFSTGO` illegal bit number** — *"&lt;bit No.&gt; has the range 0 to 29
  inclusive. Other values ... will cause an illegal operand value trap
  condition."* 30 and 31 generated; 29 being the last legal value is what
  separates a correct check from an off-by-one.

Where the manual states the condition but not the aftermath, the case asserts
**the condition and nothing else** — no expected result register, no expected ST.
The manual says *which* conditions an instruction raises; it does not say what a
register holds when the result does not fit, and asserting an invented value only
tests that the code agrees with the invention.

`ENTM` is the one place reasoning was used instead of a quote: its section lists
STO without a rule, so it borrows INIT's on the grounds that the two take the
same three operands under the same names. That is marked as such in the code — if
it ever disagrees with an emulator, adjudicate before "fixing" either side.
