# Trap conformance suite — design

## Why

On 2026-08-07 an audit of all 176 trap call sites under `src/cpu/` found **19
wrong trap numbers** (fixed in `ee3ec62` and `694bac2`) and 4 places that raise
no trap where the manual requires one.

Every one of them survived because **nothing asserts which trap fires**.
`instruction_validation` runs 40,064 generated cases, but it validates *results*
only. The consequences were not theoretical:

- `ADD2`/`MUL2`/… raised `TRAP_IVO` (bit 11) on integer overflow instead of
  `TRAP_O` (bit 9). NDIX arms bit 11 and not bit 9, so a condition the guest
  deliberately ignores was delivered as an Invalid Operation trap every timer
  tick.
- `HCONV`/`BYCONR`/`HCONR`/`WCONR` raised `TRAP_IOV` (bit 16, armed) for
  conversion overflow. That fires on every `char`/`short` narrowing PCC emits.
- `BYCONV` had been "fixed" by **deleting its trap entirely**, with a comment
  correctly observing that the kernel expects silent wrapping — treating the
  symptom of a wrong trap number in its siblings.

A test that asserted the exact bit would have caught all three.

## Shape

Table-driven. One row per documented trap condition. Each row carries its
manual citation, so the table doubles as the conformance record.

```c
typedef struct {
    const char *name;          /* "BYCONV: value outside byte range"        */
    void      (*setup)(TrapCase *);  /* build cpu state + fetched instr     */
    uint64_t    expect_trap;   /* TRAP_O — exact bit, or 0 for "no trap"    */
    uint32_t    expect_flags;  /* ND500_FLAG_O | …  status bits required    */
    uint32_t    expect_flags_mask; /* which flags the manual actually pins  */
    uint64_t    expect_result; /* value the manual says lands in the dest   */
    int         check_result;  /* 0 where the manual does not specify one   */
    const char *authority;     /* "ND-05.009.4 15.2" — never blank          */
} TrapCase;
```

**Assert equality, never "a trap happened".** `expect_trap` must match the
raised bit exactly; that is the whole point. A case expecting no trap must
prove none was raised.

**Arm every trap bit** during the run so dispatch cannot mask a mistake. Then
add a second pass with NDIX's real mask, `T_CMTE1 = 0xf413d800`, asserting
which cases actually reach a handler — that is what separates a cosmetic
mismatch from a guest-visible bug, and it is the number that mattered in all
three cases above.

## Coverage

Follow `docs/ND-500-TRAPS.md` and the per-instruction "Trap conditions:" line
in ND-05.009.4. Minimum:

| Family | Conditions |
|---|---|
| Integer arithmetic | overflow → `O`; divide by zero → `DZ` |
| Conversions | narrowing / float→int overflow → `O` (BYCONV, HCONV, WCONV, BYCONR, HCONR, WCONR, PWCONV, WPCONV) |
| Bit field | bit no. ≥ width, field size 0, bit+size > width → `IOV`; destination unchanged |
| Shifts | count ≥ width → `IOV` |
| Transcendental | `SQRT` neg, `SIN`/`COS` \|arg\|>65536, `ASIN`/`ACOS` \|arg\|>1, `ALOG` ≤0, `ATAN2` 0,0 → `IVO`; `EXP` >255·ln2 → `IVO` **and result = largest float** |
| Stack | `ENTS`/`ENTM`/`INIT` → `STO`; `RET`/`RETB`/`RETK` → `STU` |
| Call/entry | interlock → `ISE` |
| Decimal | non-digit → `IVO`; BCD overflow → `BO` |
| `IXI`/`AXI` | zero base + negative exponent → `IOV`, result = largest float |

## Known gaps — write these as failing tests

Deliberately not fixed on 2026-08-07: adding a trap NDIX has **armed** changes
behaviour on a working guest and needs its own testing rather than being
smuggled into a correctness sweep. Record each as a red test.

| Site | Missing | NDIX arms it? | Covered |
|---|---|---|---|
| `Wpconv.c` BCD overflow | `TRAP_BO` (15) | **yes** | not yet |
| `Axi.c` zero base, negative exponent | `TRAP_IVO` (11) — **corrected** | **yes** | yes |
| `Pwconv.c:95` integer overflow | `TRAP_O` (9) | no | not yet |
| `Wconv.c` double→word — never even sets `O` | `TRAP_O` (9) | no | yes |

**Correction to the AXI row.** This originally said AXI was missing `TRAP_IOV`
(bit 16). That is wrong. `Axi.c`'s own header, quoting ND-500 Reference Manual
ch.12.1, lists the trap conditions as "Floating overflow (FO), Floating
underflow (FU), Invalid operation (IVO)" and the data status bits as "invalid
operation -> IVO". So the right bit is **IVO (11)**, not IOV (16).

AXI is also missing *three* traps, not one: `Axi.c` detects overflow,
underflow and invalid-operation correctly and then only sets flags — and for
invalid operation it sets `ND500_FLAG_K` (bit 8, "destination full"), which is
not an error flag at all. IVO is armed by NDIX, so fixing this changes guest
behaviour and still needs its own testing.

**Why `Wpconv`/`Pwconv` are not covered yet.** Both take a packed-BCD
descriptor from memory rather than registers, so they need a descriptor
builder (`Nd500BcdDescriptor`) that the current two-register operand helper
does not provide. Nothing about them is unclear — it is only unwritten.

## Unresolved — do not guess

The audit could not settle these from the manual. Leave them alone until there
is evidence (ND-5000 microcode, or a real-machine test); do not encode a guess
as an assertion.

- BCD descriptor invalid / `FW = 0`: `IOV` (armed) or `DR` (not armed)?
  `Wpconv.c`'s own header comment says `DR`, the code raises `IOV`, and the
  manual lists neither. The choice changes guest behaviour.
- `Smvtu`/`Scotr`/`Smvtr` translate-table address wrap → currently `IVO`
  (armed). Pure emulator guard; the manual specifies no trap.
- `Schpar.c:80` mode out of 0..3 → `IOS` or `IOV`?

## Wiring

New file `test/test_trap_conformance.c`, registered in `test/CMakeLists.txt`,
run by `ctest`. Follow `test/test_solo_traps.c` for style — plain C, a `CHECK`
macro, a header comment that quotes the manual and states what each case pins.

One implementation detail to confirm first: how a raised trap is observed after
`raise_trap()` — the pending-trap register on `Nd500Cpu`, versus intercepting
dispatch. `test_solo_traps.c` drives `check_solo_timeout()` directly and is the
nearest precedent.

## Note on the existing suite

`ote_instructions`, `mon_calls` and `instruction_validation` were already
failing before this work (confirmed by stashing against a clean tree).
`instruction_validation`'s failures are `DoubleSub` **result** mismatches —
unrelated to traps, but worth triaging separately.
