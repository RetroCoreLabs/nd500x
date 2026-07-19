# ND-500 CPU Instruction Behavior Reference: Category CALL

Authoritative behavior reference for the ND-500 CALL instruction category, for
validating the nd500x emulator. Every statement below is read directly from one
of the two primary sources; anything not confirmed by either source is marked
`UNKNOWN (needs verification)`.

## Sources

- PRIMARY spec (the TRUTH per project rules):
  `/home/ronny/repos/nd500x/docs/ND-05.009.4 EN ND-500 Reference Manual.md`
- GROUND-TRUTH flag micro-behavior (ND-5000 microcode):
  `/mnt/e/Dev/Ronny/ND5000UC/microcode/MICRO-5800-A30.md`
- Micro-op field decode:
  `/mnt/e/Dev/Ronny/ND5000UC/manual/mnemonics.md`
- Instruction set enumerated from:
  `/home/ronny/repos/nd500x/src/cpu/instructions/CALL/*.c`

## Micro-op field decode (from mnemonics.md, "K" field bits and STATUS field)

- `K,ONE`  = SET K to 1                              (mnemonics.md line 653)
- `K,ZRO`  = CLEAR K to 0                            (mnemonics.md line 654)
- `K,1IFZ` = SET K to 1 IF ALU operation result is 0 (mnemonics.md line 655)
- `ST,SAVA` = SAVE STATUS FROM ALU OPERATION (updates the arithmetic data
  status bits from the ALU result)                  (mnemonics.md line 656)
- `COND,K` = branch condition tests K from S1 (a test, NOT a flag write)
                                                     (mnemonics.md line 778)
- `COND,MSGN` = condition = S (sign) from ALU op     (mnemonics.md line 773)
- `COND,MSORZ` = condition = OR of S and Z from ALU op (mnemonics.md line 767)

## Flag / status-bit vocabulary used in this document

- K  = the "flag" bit of the STATUS register (a.k.a. Key / Invalid flag). Set/
  cleared explicitly by the return instructions and by CHAIN.
- Z, C, O, S = the ARITHMETIC DATA STATUS bits (Zero, Carry, Overflow, Sign).
  The manual's "Data status bits" line refers to THESE. They are written only by
  a micro-op that saves ALU status (`ST,SAVA`).
- STO / STU = trap-condition status bits (Stack Overflow / Stack Underflow).
  These are distinct from the arithmetic data status bits. Per manual page 275:
  "The STO status bit is set/reset for each ENTS, ENTSN, ENTB, INIT, ENTM and
  GETB instruction." and "STU ... set/reset at each return from a stack
  subroutine."

## Instruction set (from `/home/ronny/repos/nd500x/src/cpu/instructions/CALL/`)

CALL, CALLG, CHAIN, ENTB, ENTD, ENTF, ENTFN, ENTM, ENTS, ENTSN, ENTT,
IFKRET (manual name: IF K RET), RET, RETB, RETBK, RETD, RETK, RETT.

### OPCODE DISCREPANCIES (source-file vs manual) - flagged, not resolved here

- ENTT: manual says `0BCH / 274B` (manual page 235). Source file
  `/home/ronny/repos/nd500x/src/cpu/instructions/CALL/Entt.c` claims
  `0xFD3A / 0176472`. These disagree; the manual value is used below as PRIMARY.
- RETT: manual says `083H / 203B` (manual page 238). Source file
  `/home/ronny/repos/nd500x/src/cpu/instructions/CALL/Rett.c` claims
  `0xFD3B / 0176473`. These disagree; the manual value is used below as PRIMARY.
- CHAIN: manual says `0FD6C+(n-1) / 176554B+(n-1)` (manual line 9539). Source
  file `Chain.c` octal `0175554` is arithmetically WRONG (0x175554 octal =
  64364 decimal, but 0xFD6C = 64876 decimal = 176554 octal). Manual value used.

Note: the octal numbers cited as "microcode entry" below are ND-5000 microcode
ROM addresses (the label's address in MICRO-5800-A30.md), NOT instruction
opcodes.

---

## CALL - call subroutine absolute

- Opcode: 303B (0C3H)   [manual page 228, line ~7717]
- Microcode entry: label **CALL** at octal 000644 (MICRO-5800-A30.md line 434)
- Operation: Calculate the effective addresses of the arguments, prepare for the
  entry point at <subr. addr.> (a direct 4-byte operand following the opcode),
  and jump to that subroutine entry point.
- Operands: `<subr. addr.>, <no of arg/s/BY>, <arg1/aa/W>,...,<argn/aa/W>`.
  `<no of arg>` must be a constant byte integer < 256. Args are always
  interpreted as word integers; register/constant args are illegal (they have no
  data-memory address). Args may not be prefixed with ALT.

STATUS FLAGS:

| Flag | Effect | Evidence |
|------|--------|----------|
| K | UNCHANGED | Manual "Data status bits: Unaffected" (page 228); no K micro-op at CALL entry (line 434-437) |
| Z | UNCHANGED | Manual "Data status bits: Unaffected"; no ST,SAVA in CALL flow |
| C | UNCHANGED | Manual "Data status bits: Unaffected"; no ST,SAVA in CALL flow |
| O | UNCHANGED | Manual "Data status bits: Unaffected"; no ST,SAVA in CALL flow |
| S | UNCHANGED | Manual "Data status bits: Unaffected"; no ST,SAVA in CALL flow |

TRAP conditions (manual page 228): Addressing traps, Call trap (CT), Illegal
operand specifier (IOS), Instruction sequence error (ISE). ISE occurs if the
target is not an entry-point instruction.

---

## CALLG - call subroutine general

- Opcode: 265B (0B5H)   [manual page 227, line 7677]
- Microcode entry: label **CALLG** at octal 000650 (MICRO-5800-A30.md line 438)
- Operation: Same as CALL, except <subr. addr.> is a GENERAL operand (any
  addressing mode), accessed via a general operand rather than a direct operand.
  Calculate arg effective addresses, prepare for the entry point, jump to it.
- Operands: `<subr. addr/r/W>, <no of arg/s/BY>, <arg1/aa/W>,...,<argn/aa/W>`.
  Same operand rules as CALL.

STATUS FLAGS:

| Flag | Effect | Evidence |
|------|--------|----------|
| K | UNCHANGED | Manual "Data status bits: Unaffected" (page 227); no K micro-op at CALLG entry (line 438-440) |
| Z | UNCHANGED | Manual "Data status bits: Unaffected"; no ST,SAVA in CALLG flow |
| C | UNCHANGED | Manual "Data status bits: Unaffected"; no ST,SAVA in CALLG flow |
| O | UNCHANGED | Manual "Data status bits: Unaffected"; no ST,SAVA in CALLG flow |
| S | UNCHANGED | Manual "Data status bits: Unaffected"; no ST,SAVA in CALLG flow |

TRAP conditions (manual page 227): Addressing traps, Call trap (CT), Illegal
operand specifier (IOS), Instruction sequence error (ISE).

---

## CHAIN - load address of multilevel chain (Wn CHAIN)

- Opcode: 176554B+(n-1) (0FD6CH+(n-1)), where Wn selects the destination
  register 1..4   [manual section 15.7, line 9539]
- Microcode entry: label **CHAIN** at octal 000753 (MICRO-5800-A30.md line 505);
  resolution tail **CHAIN_RES** at octal 007454-007455 (lines 3900-3901).
- Operation (manual line 9539+):
  `<address> -> Wn ; for i in (1..<no. of levels>) do while ((Wn)+<offset>) != 0 :
  ((Wn)+<offset>) -> Wn`. Follows a static-link chain `<no. of levels>` steps and
  loads Wn with the base address of the target scope. `<no. of levels>` == 0
  behaves like LADDR.
- Operands: `<address/aa/W>, <offset/r/W>, <no. of levels/r/W>`.

STATUS FLAGS:

| Flag | Effect | Evidence |
|------|--------|----------|
| K | CONDITIONAL: SET (=1) if a next link in the chain is zero (premature termination); CLEARED otherwise | Manual line 9552: "If the next link in the chain is zero ... the K flag is set." Microcode: `K,ONE` at octal 007443 and 007451 (the zero-link/terminate paths); `K,ZRO` at octal 007436 (non-terminating path). |
| S | SET to the sign bit of the last computed address ("Last address.signbit -> S") | Manual "Data Status Bits: Last address.signbit -> S" (line ~9560). Microcode: `ST,SAVA` at **CHAIN_RES** octal 007455 (and 000762 in the levels==0 / LADDR path), saving ALU status; S = COND,MSGN sense. |
| Z | Written by ST,SAVA (saves full ALU status), but the manual documents only S | Microcode `ST,SAVA` at 007455; manual documents only S. Exact Z value = UNKNOWN (needs verification of the final ALU operand at 007455). |
| C | Written by ST,SAVA, manual documents only S | Same as Z above; exact C value UNKNOWN (needs verification). |
| O | Written by ST,SAVA, manual documents only S | Same as Z above; exact O value UNKNOWN (needs verification). |

TRAP conditions (manual line 9556): Addressing traps, Illegal operand value
(IOV). IOV occurs when a next link is zero (chain terminated early) AND when
`<no. of levels>` is negative.

---

## ENTS - enter stack subroutine

- Opcode: 270B (0B8H)   [manual page 233, line 7900+]
- Microcode entry: label **ENTS** at octal 000662 (MICRO-5800-A30.md line 448)
- Operation: Allocate a new local data block on the stack and enter the
  subroutine. `<stack demand>` is the number of bytes for the local data field
  (including the 20-byte predefined PREVB/RETA/SP/AUX/N area).
- Operands: `<stack demand/r/W>`.
- Initializations (manual page 233): B.SP -> B ; oldB -> B.PREVB ;
  return address -> B.RETA -> L ; newB + <stackdemand> -> B.SP ;
  number of arguments -> B.N ; addresses of arguments -> B.ARG.
- Sequence rule: executing an entry-point instruction not resulting from a
  subroutine call causes an ISE trap (manual page 230).

STATUS FLAGS:

| Flag | Effect | Evidence |
|------|--------|----------|
| K | UNCHANGED | No K micro-op in ENTS flow (entry line 448-450; extended flow 2183-2187 verified no K,ONE/K,ZRO) |
| Z | UNCHANGED | No ST,SAVA in ENTS flow |
| C | UNCHANGED | No ST,SAVA in ENTS flow |
| O | UNCHANGED | No ST,SAVA in ENTS flow |
| S | UNCHANGED | No ST,SAVA in ENTS flow |
| STO (trap bit) | SET/RESET by this instruction (set if newB.SP >= TOS) | Manual page 275: "The STO status bit is set/reset for each ENTS, ENTSN, ENTB, INIT, ENTM and GETB instruction." |

TRAP conditions (manual page 233): Addressing traps, Stack overflow (STO),
Instruction sequence error (ISE). STO occurs if B + <stack demand> >= TOS.

---

## ENTSN - enter maximum-number-of-arguments stack subroutine

- Opcode: 272B (0BAH)   [manual page 233, line 7923]
- Microcode entry: label **ENTSN** at octal 000666 (MICRO-5800-A30.md line 452)
- Operation: Like ENTS, but only the first `<max no. of arg.>` argument addresses
  are transferred to the stack; remaining ones are ignored, and B.N contains the
  <max no. of arg.> value (manual page 233 and line 1120).
- Operands: `<stack demand/r/W>, <max no. of arg./r/W>`.
- Initializations: same layout as ENTS.

STATUS FLAGS:

| Flag | Effect | Evidence |
|------|--------|----------|
| K | UNCHANGED | No K micro-op in ENTSN flow (entry 452-454; extended 2216-2220 verified no K write) |
| Z | UNCHANGED | No ST,SAVA in ENTSN flow |
| C | UNCHANGED | No ST,SAVA in ENTSN flow |
| O | UNCHANGED | No ST,SAVA in ENTSN flow |
| S | UNCHANGED | No ST,SAVA in ENTSN flow |
| STO (trap bit) | SET/RESET by this instruction | Manual page 275 (ENTSN listed explicitly) |

TRAP conditions (manual page 233): Addressing traps, Stack overflow (STO),
Instruction sequence error (ISE).

---

## ENTD - enter subroutine directly

- Opcode: 234B (09CH)   [manual page 232]
- Microcode entry: label **ENTD** at octal 000660 (MICRO-5800-A30.md line 446)
- Operation: No local-data-area initialization and no argument transfer. The
  call to ENTD MUST have zero arguments.
- Operands: `<stack demand/r/W>` (per manual section 13.10 format table; the
  ENTD description states no stack frame is initialized).
- Initializations (manual page 232): return address -> L.

STATUS FLAGS:

| Flag | Effect | Evidence |
|------|--------|----------|
| K | UNCHANGED | No K micro-op at ENTD (line 446) |
| Z | UNCHANGED | No ST,SAVA in ENTD flow |
| C | UNCHANGED | No ST,SAVA in ENTD flow |
| O | UNCHANGED | No ST,SAVA in ENTD flow |
| S | UNCHANGED | No ST,SAVA in ENTD flow |

TRAP conditions (manual page 232): Address trap fetch (ATF), Instruction
sequence error (ISE). ISE also occurs if the argument count is non-zero.

---

## ENTF - enter subroutine (fixed/static data area)

- Opcode: 335B (0DDH)   [manual page 234]
- Microcode entry: label **ENTF** at octal 000665 (MICRO-5800-A30.md line 451)
- Operation: Enter a subroutine using a pre-allocated fixed (static) data area.
  Variables keep their values between calls (non-reentrant).
- Operands: `<address of local data area/r/W>`.
- Initializations (manual page 234): <address of local data area> -> B ;
  oldB -> B.PREVB ; return address -> B.RETA -> L ; oldB.SP -> B.SP ;
  number of arguments -> B.N ; addresses of arguments -> B.ARG.

STATUS FLAGS:

| Flag | Effect | Evidence |
|------|--------|----------|
| K | UNCHANGED | No K micro-op in ENTF flow (entry 451; extended 2207-2214 verified no K write) |
| Z | UNCHANGED | No ST,SAVA in ENTF flow |
| C | UNCHANGED | No ST,SAVA in ENTF flow |
| O | UNCHANGED | No ST,SAVA in ENTF flow |
| S | UNCHANGED | No ST,SAVA in ENTF flow |

TRAP conditions (manual page 234): Addressing traps, Instruction sequence error
(ISE). (STO is NOT listed for ENTF/ENTFN in the manual page-275 STO list.)

---

## ENTFN - enter maximum-number-of-arguments subroutine (fixed data area)

- Opcode: 336B (0DEH)   [manual page 234, line 7964]
- Microcode entry: label **ENTFN** at octal 000671 (MICRO-5800-A30.md line 455)
- Operation: Like ENTF (fixed data area), but only the first `<max no. of arg.>`
  argument addresses are transferred to the stack; remaining ones ignored and
  B.N = <max no. of arg.> (manual page 234, line 7968; line 1120).
- Operands: `<address of local data area/r/W>, <max no. of arg./r/W>`.
- Initializations: same layout as ENTF.

STATUS FLAGS:

| Flag | Effect | Evidence |
|------|--------|----------|
| K | UNCHANGED | No K micro-op in ENTFN flow (entry 455-456; extended 2243-2250 verified no K write) |
| Z | UNCHANGED | No ST,SAVA in ENTFN flow |
| C | UNCHANGED | No ST,SAVA in ENTFN flow |
| O | UNCHANGED | No ST,SAVA in ENTFN flow |
| S | UNCHANGED | No ST,SAVA in ENTFN flow |

TRAP conditions (manual page 234): Addressing traps, Instruction sequence error
(ISE).

---

## ENTM - enter module (initialize new stack)

- Opcode: 337B (0DFH)   [manual page 231]
- Microcode entry: label **ENTM** at octal 000656 (MICRO-5800-A30.md line 444)
- Operation: Initialize a NEW stack. The only entry point that may be called
  from another domain. If entered cross-domain, TOS/THA/LL/HL are saved to the
  old domain information table and reloaded from the new one.
- Operands: `<bottom of stack/r/W>, <stack demand of main program/r/W>,
  <total system stack demand/r/W>`.
- Initializations (manual page 231): <bottom of stack> -> B ; oldB -> B.PREVB ;
  TOS -> IND(oldB.SP) ; <bottom of stack> + <total system stack demand> -> TOS ;
  return address -> B.RETA -> L ;
  <bottom of stack> + <stack demand of main program> -> B.SP ;
  number of arguments -> B.N ; addresses of arguments -> B.arg.
  If change of domain: 0 -> B.PREVB ; 0 -> B.RETA ; TOS,LL,HL,THA saved/loaded
  via domain information tables.

STATUS FLAGS:

| Flag | Effect | Evidence |
|------|--------|----------|
| K | UNCHANGED | No K micro-op in ENTM flow (entry 444; extended 2112-2136 verified no K write) |
| Z | UNCHANGED | No ST,SAVA in ENTM flow |
| C | UNCHANGED | No ST,SAVA in ENTM flow |
| O | UNCHANGED | No ST,SAVA in ENTM flow |
| S | UNCHANGED | No ST,SAVA in ENTM flow |
| STO (trap bit) | SET/RESET by this instruction (set if main-program stack demand >= total system stack demand) | Manual page 275 (ENTM listed explicitly); manual page 231 |

TRAP conditions (manual page 231): Addressing traps, Instruction sequence error
(ISE), Stack overflow (STO).

---

## ENTB - enter subroutine with buddy allocation

- Opcode: 275B (0BDH)   [manual page 237]
- Microcode entry: label **ENTB** at octal 000676 (MICRO-5800-A30.md line 460)
- Operation: Allocate a local data area of size 2**<log size> words from the
  heap (buddy allocator) and enter the subroutine.
- Operands: `<log size/r/BY>`.
- Initializations (manual page 237): address of heap element -> B ;
  oldB -> B.PREVB ; oldB.SP -> B.SP ; return address -> B.RETA -> L ;
  log size -> B.LOG ; number of arguments -> B.N ; addresses of arguments -> B.ARG.

STATUS FLAGS:

| Flag | Effect | Evidence |
|------|--------|----------|
| K | UNCHANGED | No K micro-op in ENTB flow (entry 460-461; extended 2259-2269 verified no K write) |
| Z | UNCHANGED | No ST,SAVA in ENTB flow |
| C | UNCHANGED | No ST,SAVA in ENTB flow |
| O | UNCHANGED | No ST,SAVA in ENTB flow |
| S | UNCHANGED | No ST,SAVA in ENTB flow |
| STO (trap bit) | SET/RESET by this instruction (set if no free heap block of the requested size or larger) | Manual page 275 (ENTB listed explicitly); manual page 237 |

TRAP conditions (manual page 237): Addressing traps, Stack overflow (STO),
Instruction sequence error (ISE).

---

## ENTT - enter trap handler

- Opcode: 274B (0BCH)   [manual page 235]. (Source file Entt.c disagrees: claims
  0176472/0xFD3A. See "OPCODE DISCREPANCIES" above.)
- Microcode entry: label **ENTT** at octal 000673 (MICRO-5800-A30.md line 457);
  further flow at 013053+ (lines 5689+).
- Operation: Trap-handler entry point (first instruction of a trap handler).
  Stacks the register block into a special local data area at THA+400B; arg1 =
  "Trapping P" (address of the first byte of the trapping instruction). ENTT may
  ONLY be executed as the result of a trap, never as a normal CALL/CALLG target
  (manual page 230).
- Operands: `<trap handler main program stack demand/r/W>,
  <total trap handler stack demand/r/W>`.

STATUS FLAGS:

| Flag | Effect | Evidence |
|------|--------|----------|
| K | UNCHANGED (the entering register block, including STATUS, is SAVED to the local data area, not modified) | No K,ONE/K,ZRO micro-op in ENTT flow (entry 457; flow 5689-5697); manual page 235 describes only saving the register block |
| Z | UNCHANGED | No ST,SAVA in ENTT flow |
| C | UNCHANGED | No ST,SAVA in ENTT flow |
| O | UNCHANGED | No ST,SAVA in ENTT flow |
| S | UNCHANGED | No ST,SAVA in ENTT flow |

Note: ENTT clearing of the OTE / trap-enable state (asserted by the nd500x
source) is NOT confirmed as a Z/C/O/S/K data-status effect by the manual page
235 or the microcode entry flow. Any OTE side effect is a separate register, not
one of the K/Z/C/O/S flags = out of scope for this flag table.

TRAP conditions (manual page 235): Addressing traps, Instruction sequence error
(ISE). "(No traps are handled locally.)"

---

## RET - clear-flag return from subroutine

- Opcode: 200B (080H)   [manual page 238]
- Microcode entry: label **RET** at octal 000701 (MICRO-5800-A30.md line 463)
- Operation (manual page 238): `0 -> STATUS.K ; B.RETA -> P -> L ; B.PREVB -> B`.
  Return from a subroutine with local data area; new base register and return
  address taken from the current local data area.
- Operands: none.

STATUS FLAGS:

| Flag | Effect | Evidence |
|------|--------|----------|
| K | CLEARED (to 0) | Manual page 238 "0 -> STATUS.K". Microcode `K,ZRO` at octal 000701 (line 463). |
| Z | UNCHANGED | Manual page 239 "Data status bits: Unaffected"; no ST,SAVA in RET / RET_1 / RET_2 tail (verified 004406-004414) |
| C | UNCHANGED | Manual "Data status bits: Unaffected"; no ST,SAVA in RET tail |
| O | UNCHANGED | Manual "Data status bits: Unaffected"; no ST,SAVA in RET tail |
| S | UNCHANGED | Manual "Data status bits: Unaffected"; no ST,SAVA in RET tail |
| STU (trap bit) | SET/RESET at this return | Manual page 275: STU "set/reset at each return from a stack subroutine" |

TRAP conditions (manual page 239): Addressing traps, Stack underflow (STU),
Branch trap (BT). If B.PREVB or B.RETA is zero, a cross-domain return is
attempted; STU occurs only if there is no alternative domain (CAD zero or CAD ==
CED).

---

## RETK - set-flag return from subroutine

- Opcode: 201B (081H)   [manual page 238]
- Microcode entry: label **RETK** at octal 000702 (MICRO-5800-A30.md line 464)
- Operation (manual page 238): `1 -> STATUS.K ; B.RETA -> P -> L ; B.PREVB -> B`.
  Identical to RET except it SETS K.
- Operands: none.

STATUS FLAGS:

| Flag | Effect | Evidence |
|------|--------|----------|
| K | SET (to 1) | Manual page 238 "1 -> STATUS.K". Microcode `K,ONE` at octal 000702 (line 464). |
| Z | UNCHANGED | Manual page 239 "Data status bits: Unaffected"; no ST,SAVA in RET tail |
| C | UNCHANGED | Manual "Data status bits: Unaffected"; no ST,SAVA in RET tail |
| O | UNCHANGED | Manual "Data status bits: Unaffected"; no ST,SAVA in RET tail |
| S | UNCHANGED | Manual "Data status bits: Unaffected"; no ST,SAVA in RET tail |
| STU (trap bit) | SET/RESET at this return | Manual page 275 |

TRAP conditions (manual page 239): Addressing traps, Stack underflow (STU),
Branch trap (BT). Same cross-domain rule as RET.

---

## RETD - return from direct subroutine

- Opcode: 202B (082H)   [manual page 238]
- Microcode entry: label **RETD** at octal 000700 (MICRO-5800-A30.md line 462)
- Operation (manual page 238/239): `L -> P`. Load the new program counter from
  the link register. (Pairs with ENTD; B register is not changed.)
- Operands: none.

STATUS FLAGS:

| Flag | Effect | Evidence |
|------|--------|----------|
| K | UNCHANGED | Manual page 239 "Data status bits: Unaffected"; no K micro-op at RETD (line 462) or RETD_1 (004415) |
| Z | UNCHANGED | Manual "Data status bits: Unaffected"; no ST,SAVA in RETD flow |
| C | UNCHANGED | Manual "Data status bits: Unaffected"; no ST,SAVA in RETD flow |
| O | UNCHANGED | Manual "Data status bits: Unaffected"; no ST,SAVA in RETD flow |
| S | UNCHANGED | Manual "Data status bits: Unaffected"; no ST,SAVA in RETD flow |

TRAP conditions (manual page 239, shared RET-group line): Addressing traps,
Stack underflow (STU), Branch trap (BT). (RETD does not unwind a stack frame, so
STU applicability is per the shared group line; the RETD operation itself only
loads P from L.)

---

## RETT - trap handler return

- Opcode: 203B (083H)   [manual page 238]. (Source file Rett.c disagrees: claims
  0176473/0xFD3B. See "OPCODE DISCREPANCIES" above.)
- Microcode entry: label **RETT** at octal 000710 (MICRO-5800-A30.md line 470);
  status-restore flow includes **RETT_NSTS** at octal 013440 (line 5934).
- Operation (manual page 238/239): The register block is loaded from
  B.arg2..B.arg40. OTE, TEMM, CED and CAS are loaded from the domain information
  table. The status register is loaded partly from B.arg18..B.arg19 and partly
  from the domain information table. PREVB and RETA are not used or tested.
- Operands: none.

STATUS FLAGS:

| Flag | Effect | Evidence |
|------|--------|----------|
| K | RESTORED from saved trap context (part of the status register loaded from B.arg18..B.arg19 / DIT), NOT computed | Manual page 238/239: "The status register is loaded partly from B.arg18..B.arg19 and partly from the domain information table." Microcode has no K,ONE/K,ZRO write in the RETT flow; status is restored (RETT_NSTS 013440). |
| Z | RESTORED from saved trap context | Same manual/microcode evidence as K above |
| C | RESTORED from saved trap context | Same manual/microcode evidence as K above |
| O | RESTORED from saved trap context | Same manual/microcode evidence as K above |
| S | RESTORED from saved trap context | Same manual/microcode evidence as K above |

Note: RETT does NOT clear or set flags to fixed values; it reloads the entire
status register (all of K/Z/C/O/S) from the saved trap context. Exact bit-by-bit
provenance (which come from B.arg18..B.arg19 vs the domain information table) =
UNKNOWN (needs verification against the DIT/status layout).

TRAP conditions (manual page 239, shared RET-group line): Addressing traps,
Stack underflow (STU), Branch trap (BT). Manual page 235 (ENTT section): "No
traps are handled locally" for the trap-handler pair.

---

## IF K RET (source: IFKRET) - conditional return if flag set

- Opcode: 235B (09DH)   [manual page 238]
- Microcode entry: label **IFKRET** at octal 000703 (MICRO-5800-A30.md line 465);
  test at octal 000704 (`COND,K`), fall-through-to-return tail at octal 000705.
- Operation (manual page 238):
  `if STATUS.K = 1 then B.RETA -> P -> L ; B.PREVB -> B endif`.
  If K is set, perform a subroutine return WITH THE FLAG BIT REMAINING SET;
  otherwise control goes to the next instruction.
- Operands: none.

STATUS FLAGS:

| Flag | Effect | Evidence |
|------|--------|----------|
| K | UNCHANGED. If K==1 a return is taken and K remains SET; if K==0 no return and K remains CLEAR. | Manual page 238: "a subroutine return is performed with the flag bit remaining set." Microcode: octal 000704 uses `COND,K` (a TEST, not a write); the taken-return tail at octal 000705 -> RET_1 contains NO `K,ZRO`/`K,ONE`. So K is never rewritten. |
| Z | UNCHANGED | Manual page 239 "Data status bits: Unaffected"; no ST,SAVA in IFKRET flow (000703-000705 + RET tail) |
| C | UNCHANGED | Manual "Data status bits: Unaffected"; no ST,SAVA in flow |
| O | UNCHANGED | Manual "Data status bits: Unaffected"; no ST,SAVA in flow |
| S | UNCHANGED | Manual "Data status bits: Unaffected"; no ST,SAVA in flow |
| STU (trap bit) | SET/RESET at this return (only when a return is actually performed) | Manual page 275 STU note; manual page 239 lists IF K RET among the returns subject to the PREVB/RETA==0 cross-domain / STU rule |

CONTRADICTION TO FLAG (for emulator validation): the nd500x source file
`/home/ronny/repos/nd500x/src/cpu/instructions/CALL/Ifkret.c` comment claims
"K: Cleared if return occurs". Both the manual (page 238, "flag bit remaining
set") AND the microcode (no K,ZRO in the 000704/000705/RET_1 taken path)
CONTRADICT this: K must remain SET when the return is taken. Emulator must NOT
clear K on an IF K RET return.

TRAP conditions (manual page 239, shared RET-group line): Addressing traps,
Stack underflow (STU), Branch trap (BT). Source-file note also mentions Address
trap fetch (ATF) if the return address is invalid; the manual groups this under
Addressing traps.

---

## RETB - buddy subroutine return (clear flag)

- Opcode: 177034B (0FE1CH)   [manual page 238, line 8111+]
- Microcode entry: label **RETB** at octal 000706 (MICRO-5800-A30.md line 468);
  heap-release tail **RETB_1** at octal 004363 (line 2305).
- Operation (manual page 238): Local data area released to heap;
  `0 -> STATUS.K ; B.RETA -> P -> L ; B.PREVB -> B`. The heap element used as the
  local data area is released to the heap described by the variables pointed at
  by TOS. Must be used to return from an ENTB-entered routine.
- Operands: none.

STATUS FLAGS:

| Flag | Effect | Evidence |
|------|--------|----------|
| K | CLEARED (to 0) | Manual page 238 "0 -> STATUS.K". Microcode `K,ZRO` at octal 000706 (line 468). |
| Z | UNCHANGED | Manual page 239 "Data status bits: Unaffected"; no ST,SAVA in RETB flow |
| C | UNCHANGED | Manual "Data status bits: Unaffected"; no ST,SAVA in RETB flow |
| O | UNCHANGED | Manual "Data status bits: Unaffected"; no ST,SAVA in RETB flow |
| S | UNCHANGED | Manual "Data status bits: Unaffected"; no ST,SAVA in RETB flow |
| STU (trap bit) | SET/RESET at this return | Manual page 275 |

TRAP conditions (manual page 239): Addressing traps, Stack underflow (STU),
Branch trap (BT).

---

## RETBK - buddy subroutine return (set flag)

- Opcode: 177035B (0FE1DH)   [manual page 238, line 8117]
- Microcode entry: label **RETBK** at octal 000707 (MICRO-5800-A30.md line 469);
  shares the RETB_1 heap-release tail (octal 004363).
- Operation (manual page 238): Local data area released to heap;
  `1 -> STATUS.K ; B.RETA -> P -> L ; B.PREVB -> B`. Identical to RETB except it
  SETS K.
- Operands: none.

STATUS FLAGS:

| Flag | Effect | Evidence |
|------|--------|----------|
| K | SET (to 1) | Manual page 238 "1 -> STATUS.K". Microcode `K,ONE` at octal 000707 (line 469). |
| Z | UNCHANGED | Manual page 239 "Data status bits: Unaffected"; no ST,SAVA in RETBK flow |
| C | UNCHANGED | Manual "Data status bits: Unaffected"; no ST,SAVA in RETBK flow |
| O | UNCHANGED | Manual "Data status bits: Unaffected"; no ST,SAVA in RETBK flow |
| S | UNCHANGED | Manual "Data status bits: Unaffected"; no ST,SAVA in RETBK flow |
| STU (trap bit) | SET/RESET at this return | Manual page 275 |

TRAP conditions (manual page 239): Addressing traps, Stack underflow (STU),
Branch trap (BT).

---

## Summary table (K flag and arithmetic data status bits)

| Instr | Opcode (oct/hex, manual) | K | Z | C | O | S | Other |
|-------|--------------------------|---|---|---|---|---|-------|
| CALL   | 303B / 0C3H | unch | unch | unch | unch | unch | CT trap on execute |
| CALLG  | 265B / 0B5H | unch | unch | unch | unch | unch | CT trap on execute |
| CHAIN  | 176554B+(n-1) / 0FD6CH+(n-1) | SET if zero link else CLEAR | ST,SAVA (only S documented) | ST,SAVA (only S documented) | ST,SAVA (only S documented) | = last addr sign bit | IOV trap on zero link / negative levels |
| ENTS   | 270B / 0B8H | unch | unch | unch | unch | unch | STO set/reset |
| ENTSN  | 272B / 0BAH | unch | unch | unch | unch | unch | STO set/reset |
| ENTD   | 234B / 09CH | unch | unch | unch | unch | unch | - |
| ENTF   | 335B / 0DDH | unch | unch | unch | unch | unch | - |
| ENTFN  | 336B / 0DEH | unch | unch | unch | unch | unch | - |
| ENTM   | 337B / 0DFH | unch | unch | unch | unch | unch | STO set/reset |
| ENTB   | 275B / 0BDH | unch | unch | unch | unch | unch | STO set/reset |
| ENTT   | 274B / 0BCH (src conflict) | unch (block saved) | unch | unch | unch | unch | saves reg block |
| RET    | 200B / 080H | CLEAR | unch | unch | unch | unch | STU set/reset |
| RETK   | 201B / 081H | SET | unch | unch | unch | unch | STU set/reset |
| RETD   | 202B / 082H | unch | unch | unch | unch | unch | - |
| RETT   | 203B / 083H (src conflict) | RESTORED | RESTORED | RESTORED | RESTORED | RESTORED | reloads status reg from context |
| IF K RET | 235B / 09DH | unch (stays set when return taken) | unch | unch | unch | unch | STU set/reset when return taken |
| RETB   | 177034B / 0FE1CH | CLEAR | unch | unch | unch | unch | STU set/reset; frees heap block |
| RETBK  | 177035B / 0FE1DH | SET | unch | unch | unch | unch | STU set/reset; frees heap block |

"unch" = UNCHANGED. All effects above confirmed by BOTH the ND-500 Reference
Manual and the ND-5000 microcode except where noted UNKNOWN or documented in one
source only (CHAIN Z/C/O; RETT per-bit provenance).
