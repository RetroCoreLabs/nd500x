# ND-500 Instruction Category: CALL (Subroutine Call / Entry / Return)

FUNCTIONAL behavior reference built by TRACING THE MICROCODE, cross-checked
against the printed manual. Ground-truth mechanism = microcode; documented
intent = manual. Disagreements are called out per-instruction.

Sources traced:
- Microcode: `$ND5000UC/microcode/MICRO-5800-A30.md`
- Field mnemonics: `$ND5000UC/manual/mnemonics.md`
- Manual: `docs/ND-05.009.4 EN ND-500 Reference Manual.md`
- Emulator sources: `src/cpu/instructions/CALL/*.c`

Category members (18 files): CALL, CALLG, CHAIN, ENTB, ENTD, ENTF, ENTFN,
ENTM, ENTS, ENTSN, ENTT, IFKRET (IF K RET), RET, RETB, RETBK, RETD, RETK, RETT.

--------------------------------------------------------------------------------
## How the CALL / ENT* / RET* trio works (read this first)

The ND-500 splits subroutine linkage across TWO instructions:

1. A caller executes CALL or CALLG. This computes the effective addresses of
   the argument list, stores them plus the argument count and the return
   address in pending "call information", then jumps to the first instruction
   of the callee, which MUST be an ENT* instruction.
2. The callee's first instruction (ENTS / ENTF / ENTD / ENTM / ...) consumes
   that pending call information to build the local data area (stack frame),
   copy the argument addresses into the frame (B.ARG), store PREVB / RETA / SP
   / N, and set L = return address. The variant selects WHERE the frame lives
   (stack, fixed area, heap "buddy" element, new module stack, trap vector).
3. RET / RETK / RETD / RETT / RETB / RETBK / IFKRET tear the frame down and
   reload P (program counter) and B (base register) from the frame.

Microcode field decoding used below (from mnemonics.md):
- `ALU,<f>` core ALU op: `A`=pass A, `A+B`, `A-B`, `B-A`, `AND`, `OR`, `XOR`,
  `ANDCB`=A AND (NOT B), `FZRO`=force 0, `A-1`, `A+B,*2`.
- `A,<src>` / `B,<src>` ALU inputs. `D,<dest>` result destination.
  `A,IAC,L` = A input is the L register; `D,IAC,L` = write result to L.
  `A,DAC,B`/`D,DAC,...` = data-cache/register-file access. `A,ALU,REG37`,
  `SC1..SC14` = microcode scratch cells. `MIC,STS` = internal micro status
  (sequence/overflow bookkeeping, NOT the architectural status word).
- Memory: `READ`/`RD` = data read, `WRITE`/`WR` = data write, `WR,PHYS` /
  `RD,PHYS` = physical (MMU-bypass) access, `LOADLA` = load look-ahead / P.
- `ST,SAVA` = SAVE ARITHMETIC STATUS: writes the architectural data-status
  bits Z, C, O (overflow), S (sign) from the current ALU result.
- `ST,SAVC`/`SAVC1` = save carry only (internal). `K,ONE`/`K,ZRO` = force the
  architectural K (flag) bit to 1 / 0. `K,1IFZ` = set K if zero.
  `COND,K` = branch on K.
- `C,SEQ ... INVSEQ` = sequence check; taking the INVSEQ path routes to an
  Instruction-Sequence-Error trap. `T,PUSH`/`T,POP` = micro stack. `T,RETURN`
  = microroutine return. `GET_NEXT`/`DGET_NEXT` (000231 / 000230) = normal
  end-of-instruction: fetch and dispatch the next macro-instruction.

RULE 4040 (manual, "unmentioned data-status bits are cleared") is applied
only to instructions whose microcode actually writes status (`ST,SAVA`). The
ENT* / RET* / CALL* routines contain NO `ST,SAVA` on their spine (verified),
so the arithmetic status bits Z/C/O/S are UNCHANGED by them; only K is touched
where the microcode shows an explicit `K,ONE`/`K,ZRO`.

--------------------------------------------------------------------------------
## Opcode discrepancies found during the trace (READ THIS)

- CHAIN: the emulator source comment lists octal `0175554`. That is WRONG:
  0xFD6C = 64876 decimal = octal `0176554`, which is exactly what the manual
  (section 15.7) prints. Correct octal = **0176554B**.
- ENTT: emulator source uses opcode `0xFD3A` (octal 0176472). The manual's
  ENTT page prints `0BCH / 274B`. These do not agree.
- RETT: emulator source uses opcode `0xFD3B` (octal 0176473). The manual's
  return-summary table (13.11) prints `083H / 203B` as part of the clean run
  RET=200B, RETK=201B, RETD=202B, RETT=203B.
  Both ENTT/RETT conflicts are UNRESOLVED here: the microcode markdown lists
  MICRO addresses (ENTT at micro 000673, RETT at micro 000710), not the
  macro-opcode -> micro-address dispatch table, so which byte the real
  hardware decodes cannot be proven from this trace. Both encodings are
  reported below; do not treat either as confirmed.

================================================================================
## CALL - call subroutine absolute

Opcode: 0xC3 / 0303B. Microcode entry: **CALL** micro 000644.

Format: `CALL <subr.addr/r/W>, <no.of.arg/s/BY>, <arg1/aa/W>,...,<argn/aa/W>`

### Functional pseudocode
```
1. subr_addr = direct 4-byte operand following the opcode      ; micro 000644 A,DAC,B
2. n = <no.of.arg>            ; MUST be a constant byte < 256   ; micro 000646 TYP,BY, D,LC
     if operand is not a constant byte -> Illegal Operand Specifier trap
                                                                ; 000646/000647 -> ILL_OP_SPEC
3. for each of the n argument operands:
       EA[i] = effective address of arg[i]  (arg always read as W integer)
       if arg is register or constant (no memory address) -> IOS trap
     ; C,SEQ / CONOP loop over the operand list (micro 000645-000646, CSAVE)
4. store {subr_addr, n, EA[1..n], return_address = P_after_instr} as pending
   call information for the ENT* at the target
5. P <- subr_addr        (jump; target's first instruction must be ENT*)
     if target is not an entry-point instruction -> Instruction Sequence Error
```

### Operands + datatypes
- `<subr.addr>` : direct W (4 bytes), absolute address of callee entry point.
- `<no.of.arg>` : constant BY (byte, 0..255).
- `<argN>`      : any addressable operand (`aa`), interpreted as W. ALT prefix
  forbidden. Register/constant args are illegal.

### Result / side-effects
Effective addresses of args + count + return address saved for the ENT*.
P set to callee. No memory of the frame is built yet (the ENT* does that).

### Status flags
| Bit | Effect |
|-----|--------|
| K | UNCHANGED (no K op in trace) |
| Z | UNCHANGED (no ST,SAVA) |
| C | UNCHANGED |
| O | UNCHANGED |
| S | UNCHANGED |

Manual "Data status bits: Unaffected" - agrees with microcode.

### Traps
Addressing traps; Call trap (CT); Illegal Operand Specifier (IOS, bad arg or
non-constant count); Instruction Sequence Error (ISE, target not an ENT*).
(The CT trigger point was not located on the traced spine - see UNRESOLVED.)

Citation: micro CALL 000644-000647; manual section 13.8 (Page 228).

================================================================================
## CALLG - call subroutine general

Opcode: 0xB5 / 0265B. Microcode entry: **CALLG** micro 000650.

Format: `CALLG <subr.addr/r/W>, <no.of.arg/s/BY>, <arg1/aa/W>,...,<argn/aa/W>`

### Functional pseudocode
```
1. subr_addr = value of the GENERAL operand <subr.addr> (any addressing mode)
                                                     ; micro 000650 READ, ADACT
2. n = <no.of.arg> constant byte < 256               ; micro 000651 TYP,BY D,LC
     non-constant -> IOS trap                        ; 000653 -> ILL_OP_SPEC
3. compute EA[1..n] of the argument list (each as W); reg/const arg -> IOS
4. save {subr_addr, n, EA[], return_address} as pending call info
5. P <- subr_addr ; target must be an ENT* else ISE  ; 000652 C,SEQ
```
Identical to CALL except operand 0 is a general operand (read through any
addressing mode) instead of a direct 4-byte immediate.

### Operands + datatypes
`<subr.addr>` general W operand (must resolve to an entry point);
`<no.of.arg>` constant BY; `<argN>` addressable, read as W; ALT forbidden.

### Result / side-effects
Same as CALL: arg EAs + count + return address staged for the ENT*; P<-callee.

### Status flags
| Bit | Effect |
|-----|--------|
| K | UNCHANGED |
| Z | UNCHANGED |
| C | UNCHANGED |
| O | UNCHANGED |
| S | UNCHANGED |
Manual "Data status bits: Unaffected" - agrees.

### Traps
Addressing traps; Call trap (CT); IOS; ISE.

Citation: micro CALLG 000650-000653 -> CALLG_1 003714 -> INIT_1 003720;
manual section 13.7 (Page 227).

================================================================================
## ENTM - enter module

Opcode: 0xDF / 0337B. Microcode entry: **ENTM** micro 000656.

Format: `ENTM <bottom.of.stack/r/W>, <stack.demand.of.main/r/W>,
        <total.system.stack.demand/r/W>`

### Functional pseudocode
```
new stack frame for a whole module; the ONLY cross-domain entry point.
1. B_new  = <bottom of stack>                        ; -> B
2. IND(B_new.PREVB) = oldB
3. IND(oldB.SP)     = TOS                             ; save caller TOS
4. TOS = B_new + <total system stack demand>
5. L = B_new.RETA = return_address (from pending call info)
6. B_new.SP = B_new + <stack demand of main program>
7. B_new.N  = number of arguments
8. copy argument EAs into B_new.ARG[1..N]            ; micro 004102.. WRITE loop
9. if <stack demand of main> >= <total system stack demand> -> Stack Overflow
                                                     ; ENTM_0 004111 A-B,CRY MCRY
if entering from a DIFFERENT domain:
   0 -> B.PREVB ; 0 -> B.RETA
   save TOS,LL,HL,THA into OLD domain information table (DIT)
   load TOS,LL,HL,THA from NEW domain information table
                                                     ; DOM_ENTM_* 004114.., WR,PHYS
```

### Operands + datatypes
Three r/W operands: bottom-of-stack (absolute addr), main-program stack demand,
total system stack demand. ALT forbidden.

### Result / side-effects
Builds a fresh module stack; writes PREVB, RETA, SP, N, ARG list to memory;
updates TOS; may swap domain register context (TOS/LL/HL/THA via DIT).

### Status flags
| Bit | Effect |
|-----|--------|
| K | UNCHANGED (no K op) |
| Z | UNCHANGED (no ST,SAVA) |
| C | UNCHANGED |
| O | UNCHANGED |
| S | UNCHANGED |
Manual gives no "Data status bits" line; microcode confirms none written.

### Traps
Addressing traps; ISE (ENTM not reached from a CALL/CALLG); Stack Overflow (STO).

Citation: micro ENTM 000656-000657 -> ENTM1 004102 -> ENTM_0/ENTM_1 004111;
manual "ENTM - enter module" (Page 231).

================================================================================
## ENTD - enter subroutine directly

Opcode: 0x9C / 0234B. Microcode entry: **ENTD** micro 000660.

Format: `ENTD <stack demand/r/W>` (operand present in encoding but no frame
is built; see manual).

### Functional pseudocode
```
1. verify the call carried ZERO arguments:
     C,SEQ + COND,MZRO at micro 000660; if N != 0 -> ENT_SEQ_ERR (000661)
     -> INS_SEQ_ERR -> Instruction Sequence Error trap
2. L = return_address        ; the ONLY initialization ENTD performs
3. (no PREVB/SP/ARG frame is created; a leaf routine)
```

### Operands + datatypes
`<stack demand>` r/W (validated; effectively unused for frame build). The CALL
that reached ENTD must pass 0 arguments.

### Result / side-effects
L <- return address. Nothing written to the stack. If the routine calls
others, software must save/restore L itself.

### Status flags
| Bit | Effect |
|-----|--------|
| K | UNCHANGED |
| Z | UNCHANGED |
| C | UNCHANGED |
| O | UNCHANGED |
| S | UNCHANGED |

### Traps
Address Trap Fetch (ATF); Instruction Sequence Error (ISE, non-zero arg count
OR ENTD not reached from a call).

Citation: micro ENTD 000660 -> ENT_SEQ_ERR 000661 -> INS_SEQ_ERR 003141;
manual "ENTD - enter subroutine directly" (Page 232).

================================================================================
## ENTS - enter stack subroutine

Opcode: 0xB8 / 0270B. Microcode entry: **ENTS** micro 000662.

Format: `ENTS <stack demand/r/W>`

### Functional pseudocode
```
1. read <stack demand> (r, integer/DR datatype)      ; micro 000662 TYP,DR READ
2. newB = oldB.SP                                     ; frame grows on the stack
3. IND(newB.PREVB) = oldB
4. L = newB.RETA = return_address
5. newB.SP = newB + <stack demand>
6. if newB + <stack demand> >= TOS -> Stack Overflow  ; ENTS_STO 004213 -> TRAP
7. newB.N = number of arguments
8. copy argument EAs into newB.ARG[1..N]              ; ENTS_3 004216.. WRITE loop
9. B = newB
```
The ENTS_NEQ0 branch (004171) handles the argument-copy WRITE loop; ENTS_1
(004203) is the zero/short path. `<stack demand>` counts the 20 predefined
bytes (PREVB, RETA, SP, AUX, N).

### Operands + datatypes
`<stack demand>` r/W (number of bytes for the local field, incl. 20 predefined).

### Result / side-effects
New stack frame written (PREVB, RETA, SP, N, ARG list); B and L updated; SP
advanced.

### Status flags
| Bit | Effect |
|-----|--------|
| K | UNCHANGED |
| Z | UNCHANGED |
| C | UNCHANGED |
| O | UNCHANGED |
| S | UNCHANGED |
Microcode manipulates MIC,STS only (sequence/overflow), not architectural bits.

### Traps
Addressing traps; Stack Overflow (STO); Instruction Sequence Error (ISE).

Citation: micro ENTS 000662-000664 -> ENTS_NEQ0 004171 / ENTS_1 004203 /
ENTS_STO 004213; manual "ENTS" (Page 233).

================================================================================
## ENTSN - enter (max number of arguments) stack subroutine

Opcode: 0xBA / 0272B. Microcode entry: **ENTSN** micro 000666.

Format: `ENTSN <stack demand/r/W>, <max no. of arg./r/W>`

### Functional pseudocode
```
Same stack-frame construction as ENTS, but the argument-copy loop is bounded:
1. read <stack demand> and <max no. of arg.>         ; micro 000666-000667 READ
2. newB = oldB.SP ; PREVB, RETA->L, SP, B as in ENTS
3. copy only min(N, <max no. of arg.>) argument EAs into newB.ARG;
   remaining arguments are IGNORED                    ; ENTN_SLOOP 004262 LCDECR loop
4. stack overflow check as ENTS                       ; ENTSN_STO 004253 -> TRAP
```

### Operands + datatypes
`<stack demand>` r/W; `<max no. of arg.>` r/W.

### Result / side-effects
Same as ENTS but at most `<max no. of arg.>` argument addresses are stored.

### Status flags
| Bit | Effect |
|-----|--------|
| K | UNCHANGED |
| Z | UNCHANGED |
| C | UNCHANGED |
| O | UNCHANGED |
| S | UNCHANGED |

### Traps
Addressing traps; Stack Overflow (STO); Instruction Sequence Error (ISE).

Citation: micro ENTSN 000666-000670 -> ENTSN_0 004232 -> ENTSN_1/2/3
004236/004245/004254 -> ENT_PARAM 004257 / ENTN_SLOOP 004262;
manual "ENTSN" (Page 233).

================================================================================
## ENTF - enter subroutine (fixed data area)

Opcode: 0xDD / 0335B. Microcode entry: **ENTF** micro 000665.

Format: `ENTF <address of local data area/r/W>`

### Functional pseudocode
```
1. B = <address of local data area>   ; a FIXED area - variables persist
                                       ; between calls (not on the stack)
2. IND(B.PREVB) = oldB
3. L = B.RETA = return_address         ; ENTF_1 004221 WRITE
4. B.SP = oldB.SP                      ; SP inherited from caller
5. B.N  = number of arguments
6. copy argument EAs into B.ARG[1..N]  ; ENTF_4 004230 -> ENT_PARAM loop
```

### Operands + datatypes
`<address of local data area>` r/W - absolute address of a statically allocated
data area.

### Result / side-effects
Frame overlaid on the fixed area; PREVB/RETA/SP/N/ARG written; B,L updated.
Because the area is fixed, locals keep their values between invocations.

### Status flags
| Bit | Effect |
|-----|--------|
| K | UNCHANGED |
| Z | UNCHANGED |
| C | UNCHANGED |
| O | UNCHANGED |
| S | UNCHANGED |

### Traps
Addressing traps; Instruction Sequence Error (ISE).

Citation: micro ENTF 000665 -> ENTF_1 004221 -> ENTF_2 004224 -> ENTF_4 004230
-> ENT_PARAM 004257; manual "ENTF" (Page 234).

================================================================================
## ENTFN - enter (max number of arguments) fixed-area subroutine

Opcode: 0xDE / 0336B. Microcode entry: **ENTFN** micro 000671.

Format: `ENTFN <address of local data area/r/W>, <max no. of arg./r/W>`

### Functional pseudocode
```
As ENTF (fixed data area, SP inherited from caller) but the argument copy is
bounded by <max no. of arg.>:
1. B = <address of local data area>
2. IND(B.PREVB)=oldB ; L=B.RETA=return_address ; B.SP=oldB.SP ; B.N=N
3. copy only min(N, <max no. of arg.>) argument EAs into B.ARG; rest ignored
                                       ; ENTFN_1 004265 -> ENT_PARAM loop
```

### Operands + datatypes
`<address of local data area>` r/W; `<max no. of arg.>` r/W.

### Result / side-effects
Same as ENTF but at most `<max no. of arg.>` addresses stored.

### Status flags
| Bit | Effect |
|-----|--------|
| K | UNCHANGED |
| Z | UNCHANGED |
| C | UNCHANGED |
| O | UNCHANGED |
| S | UNCHANGED |

### Traps
Addressing traps; Instruction Sequence Error (ISE).

Citation: micro ENTFN 000671-000672 -> ENTFN_1 004265 -> ENT_PARAM 004257;
manual "ENTFN" (Page 234).

================================================================================
## ENTB - enter subroutine with buddy (heap) allocation

Opcode: 0xBD / 0275B. Microcode entry: **ENTB** micro 000676.

Format: `ENTB <log size/r/BY>`

### Functional pseudocode
```
1. read <log size> (byte)                            ; micro 000676 TYP,BY READ
2. allocate a heap element of 2**<log size> words via the buddy allocator
                                       ; FINDBDY 004322, HEAPWR
     if no free element of that size (or larger) -> Stack Overflow trap
3. B = address of the allocated heap element
4. IND(B.PREVB) = oldB
5. B.SP = oldB.SP                       ; SP inherited
6. L = B.RETA = return_address          ; ENTB_1 004305 WRITE
7. B.LOG = <log size>                   ; remembered so RETB can free it
8. B.N = number of arguments
9. copy argument EAs into B.ARG[1..N]   ; ENTB_1 004315 -> ENT_PARAM loop
```

### Operands + datatypes
`<log size>` r/BY - base-2 log of the element size in words.

### Result / side-effects
Heap element allocated and used as the local data area; PREVB/SP/RETA/LOG/N/ARG
written; B,L updated. Must be released by RETB/RETBK.

### Status flags
| Bit | Effect |
|-----|--------|
| K | UNCHANGED |
| Z | UNCHANGED |
| C | UNCHANGED |
| O | UNCHANGED |
| S | UNCHANGED |

### Traps
Addressing traps; Stack Overflow (STO, no free heap element); Instruction
Sequence Error (ISE).

Citation: micro ENTB 000676-000677 -> ENTB_1 004305 -> FINDBDY 004322 /
HEAPWR / ENTB_5 004317 -> ENTS_END 004206; manual "ENTB" (Page 237).

================================================================================
## ENTT - enter trap handler

Opcode (emulator): 0xFD3A / 0176472B.  Opcode (manual page): 0BCH / 0274B.
(DISAGREEMENT - see the opcode-discrepancy note above.)
Microcode entry: **ENTT** micro 000673.

Format: `ENTT <trap handler main-program stack demand/r/W>,
        <total trap handler stack demand/r/W>`

### Functional pseudocode
```
ENTT is the FIRST instruction of every trap handler; it may ONLY be reached by
a trap (not by CALL/CALLG).
1. sequence check: if not reached by a trap -> Instruction Sequence Error
                                       ; ENTT1 013056 C,SEQ -> TRAP_ISE (013057)
2. local data area = area following the trap-handler vector; THA points at it.
   Layout at B = THA + 400B:
     B.PREVB = 0 ; B.RETA = 0 ; L = contents of B.RETA-slot
     B.SP    = B + <trap handler main-program stack demand>
     B.AUX   = protect-violation information
     B.N     = 62B (fixed)
     B.arg1  = 'Trapping P' (address of first byte of the trapping instruction)
     B.arg2..B.arg40 = the saved register block (as numbered in chapter 2)
3. save register block into the frame        ; ENTT_REGS 013062 -> SAVEREG
4. save domain info (OTE/TEMM/CED/CAS etc.)  ; ENTT_DITS 013063 -> SAVEDINFR
5. TOS = B + <total trap handler stack demand>
```

### Operands + datatypes
`<trap handler main-program stack demand>` r/W; `<total trap handler stack
demand>` r/W. ALT forbidden.

### Result / side-effects
Full register block + status + domain context of the trapped program are saved
into the trap-handler local data area; B/L/TOS set up for the handler.
No traps are handled locally within ENTT.

### Status flags
| Bit | Effect |
|-----|--------|
| K | UNCHANGED for the handler (the trapped program's K is SAVED into the frame, arg block) |
| Z | UNCHANGED (saved, not computed) |
| C | UNCHANGED (saved) |
| O | UNCHANGED (saved) |
| S | UNCHANGED (saved) |
ENTT preserves/saves the architectural status; it does not compute new Z/C/O/S.

### Traps
Addressing traps; Instruction Sequence Error (ISE, ENTT executed other than as
a trap entry).

Citation: micro ENTT 000673-000675 -> ENTT1 013056 -> ENTT2/ENTT_REGS 013060/
013062; manual "ENTT - enter trap handler" (Pages 235-236).

================================================================================
## RET - clear-flag return from subroutine

Opcode: 0x80 / 0200B. Microcode entry: **RET** micro 000701.

Format: `RET`  (no operands)

### Functional pseudocode
```
1. 0 -> STATUS.K                       ; micro 000701 K,ZRO
2. L = B.RETA                          ; reload link/return addr from frame
3. P = B.RETA                          ; jump back to caller
4. B = B.PREVB                         ; restore caller base register
   ; RET_1 004406 -> RET_2 004410: reads B.RETA (READ), C,SEQ underflow test
5. if B.PREVB == 0 or B.RETA == 0:     ; RET_2/RET_3 004410/004412 COND,MZRO
     compare CAD (from calling domain DIT) to CED:
        if equal    -> Stack Underflow trap           ; RET_SU 004417
        if unequal  -> cross-domain return: CED <- CAD; reload B,P,CAD and
                       TOS,HL,LL,THA from the new domain information table
6. else normal in-domain return complete              ; RET_4 004414 -> GET_NEXT
```

### Operands + datatypes
None.

### Result / side-effects
P and B reloaded from the current frame; K cleared. Possible domain switch on
a zero PREVB/RETA. Frees the stack frame implicitly (B moves to PREVB).

### Status flags
| Bit | Effect |
|-----|--------|
| K | CLEARED (0 -> K), micro K,ZRO |
| Z | UNCHANGED (no ST,SAVA) |
| C | UNCHANGED |
| O | UNCHANGED |
| S | UNCHANGED |
Manual "Data status bits: Unaffected" refers to Z/C/O/S; K is explicitly set 0
by the Operation clause and by the microcode.

### Traps
Addressing traps; Stack Underflow (STU); Branch Trap (BT).

Citation: micro RET 000701 -> RET_1 004406 -> RET_2..RET_4 004410-004414 /
RET_SU 004417; manual section 13.11, "RET" (Pages 238-239).

================================================================================
## RETK - set-flag return from subroutine

Opcode: 0x81 / 0201B. Microcode entry: **RETK** micro 000702.

Format: `RETK`

### Functional pseudocode
```
Identical to RET except the flag is SET instead of cleared:
1. 1 -> STATUS.K                       ; micro 000702 K,ONE
2. L = B.RETA ; P = B.RETA ; B = B.PREVB
3. same zero-PREVB/RETA underflow / cross-domain handling as RET (shared RET_1)
```

### Operands + datatypes
None.

### Result / side-effects
As RET, but K is set to 1 - lets a callee signal an error/special condition to
the caller through the flag bit.

### Status flags
| Bit | Effect |
|-----|--------|
| K | SET (1 -> K), micro K,ONE |
| Z | UNCHANGED |
| C | UNCHANGED |
| O | UNCHANGED |
| S | UNCHANGED |

### Traps
Addressing traps; Stack Underflow (STU); Branch Trap (BT).

Citation: micro RETK 000702 -> RET_1 004406 (shared with RET);
manual section 13.11, "RETK" (Page 238).

================================================================================
## IFKRET - conditional return (IF K RET)

Opcode: 0x9D / 0235B. Microcode entry: **IFKRET** micro 000703.

Format: `IF K RET`  (assembler notation)

### Functional pseudocode
```
1. if STATUS.K == 1 then                ; micro 000704 COND,K
       ; perform a return WITH the flag left set:
       L = B.RETA ; P = B.RETA ; B = B.PREVB
       ; (K is NOT modified - stays 1)  ; 000705 -> RET_1 004406
   else
       fall through to the next instruction (no return)
                                        ; 000704 INVSEQ -> GET_NEXT
```

### Operands + datatypes
None.

### Result / side-effects
If K set: subroutine return (P,B,L reloaded from frame) with K still set.
If K clear: no effect, execution continues in-line.

### Status flags
| Bit | Effect |
|-----|--------|
| K | UNCHANGED (tested only; stays as-is - remains 1 on the taken path) |
| Z | UNCHANGED |
| C | UNCHANGED |
| O | UNCHANGED |
| S | UNCHANGED |
Note: unlike RET (which forces K=0), IFKRET does NOT clear K on return - the
micro path from 000705 to RET_1 carries no K,ZRO/K,ONE, so K keeps its value.

### Traps
Addressing traps; Stack Underflow (STU); Branch Trap (BT) - only on the taken
(return) path, via the shared RET_1 routine.

Citation: micro IFKRET 000703-000705 -> RET_1 004406; manual section 13.11,
"IF K RET" (Page 238).

================================================================================
## RETD - return from direct subroutine

Opcode: 0x82 / 0202B. Microcode entry: **RETD** micro 000700.

Format: `RETD`

### Functional pseudocode
```
1. P = L                               ; micro 000700 A,IAC,L LOADLA
2. (no frame to unwind - the ENTD partner built none)
   ; RETD_1 004415: reload look-ahead, MIC,RESTU, sequence check, GET_NEXT
```

### Operands + datatypes
None.

### Result / side-effects
Program counter loaded from the L (link) register. B, PREVB, RETA untouched.
Paired with ENTD.

### Status flags
| Bit | Effect |
|-----|--------|
| K | UNCHANGED (no K op) |
| Z | UNCHANGED |
| C | UNCHANGED |
| O | UNCHANGED |
| S | UNCHANGED |
Manual "Data status bits: Unaffected" - agrees; and unlike RET there is no K
change (no K,ZRO/K,ONE in the RETD path).

### Traps
Addressing traps; Stack Underflow (STU); Branch Trap (BT).

Citation: micro RETD 000700 -> RETD_1 004415; manual section 13.11, "RETD"
(Pages 238-239).

================================================================================
## RETB - buddy subroutine return

Opcode: 0xFE1C / 0177034B. Microcode entry: **RETB** micro 000706.

Format: `RETB`

### Functional pseudocode
```
1. 0 -> STATUS.K                       ; micro 000706 K,ZRO
2. release the current local data area (a heap/buddy element) back to the heap
   described by the variables the TOS register points at
                                       ; RETB_1 004363 -> RELBDY 004367 (WRITE)
3. L = B.RETA ; P = B.RETA ; B = B.PREVB   ; (shared with RET via RET_2 004366)
```

### Operands + datatypes
None. (Paired with ENTB.)

### Result / side-effects
Heap element freed to the buddy heap; P,B,L reloaded from frame; K cleared.

### Status flags
| Bit | Effect |
|-----|--------|
| K | CLEARED (0 -> K), micro K,ZRO |
| Z | UNCHANGED |
| C | UNCHANGED |
| O | UNCHANGED |
| S | UNCHANGED |

### Traps
Addressing traps; Stack Underflow (STU); Branch Trap (BT).

Citation: micro RETB 000706 -> RETB_1 004363 -> RELBDY 004367 -> RET_2 004410;
manual section 13.11, "RETB" (Pages 238-239).

================================================================================
## RETBK - set-flag buddy subroutine return

Opcode: 0xFE1D / 0177035B. Microcode entry: **RETBK** micro 000707.

Format: `RETBK`

### Functional pseudocode
```
Identical to RETB but sets the flag:
1. 1 -> STATUS.K                       ; micro 000707 K,ONE
2. release heap/buddy local data area back to the heap   ; RETB_1 004363
3. L = B.RETA ; P = B.RETA ; B = B.PREVB
```

### Operands + datatypes
None. (Paired with ENTB.)

### Result / side-effects
As RETB but K set to 1.

### Status flags
| Bit | Effect |
|-----|--------|
| K | SET (1 -> K), micro K,ONE |
| Z | UNCHANGED |
| C | UNCHANGED |
| O | UNCHANGED |
| S | UNCHANGED |

### Traps
Addressing traps; Stack Underflow (STU); Branch Trap (BT).

Citation: micro RETBK 000707 -> RETB_1 004363 (shared with RETB);
manual section 13.11, "RETBK" (Pages 238-239).

================================================================================
## RETT - trap handler return

Opcode (emulator): 0xFD3B / 0176473B.  Opcode (manual table): 083H / 0203B.
(DISAGREEMENT - see the opcode-discrepancy note above.)
Microcode entry: **RETT** micro 000710.

Format: `RETT`

### Functional pseudocode
```
Undo what ENTT saved; return from a trap handler.
1. reload the register block from B.arg2..B.arg40  ; RETT1 013363.. READ loop
2. reload OTE, TEMM, CED, CAS from the domain information table
                                       ; RETT via CED_TO_DIT / NEW_TO_DIT
3. reload the status register: non-ignorable + fatal status bits from the DIT,
   the rest from B.arg18..B.arg19       ; i.e. the trapped program's full STATUS
   (K,Z,C,O,S) is RESTORED, not computed
4. PREVB and RETA are NOT used/tested.
5. compare trapped-domain number (saved in DIT) with current CED:
     if unequal -> change CED back to the trapped domain    ; domain restore
6. P = trapped P (resume the trapped program)
```

### Operands + datatypes
None.

### Result / side-effects
Complete restoration of the trapped program's register block, domain registers,
and status register; execution resumes at the trapped P (possibly in another
domain).

### Status flags
| Bit | Effect |
|-----|--------|
| K | RESTORED from saved trap context (DIT / B.arg block) - NOT cleared/computed |
| Z | RESTORED from saved trap context |
| C | RESTORED from saved trap context |
| O | RESTORED from saved trap context |
| S | RESTORED from saved trap context |
Manual "Data status bits: Unaffected" is misleading for RETT: the microcode and
the RETT Operation clause show the ENTIRE status register (including K/Z/C/O/S)
is LOADED from the saved trap frame + DIT. Treat RETT as "status = restored".

### Traps
Addressing traps; Stack Underflow (STU); Branch Trap (BT).

Citation: micro RETT 000710 -> RETT1 013363 -> RETT2 013373; manual section
13.11, "RETT" (Pages 238-239).

================================================================================
## CHAIN - load address of multilevel chain (Wn CHAIN)

Opcode: 0xFD6C+(n-1) / 0176554B+(n-1).  (Emulator source octal comment
"0175554" is a typo; correct is 0176554B, matching the manual.)
Microcode entry: **CHAIN** micro 000753.

Format: `Wn CHAIN <address/aa/W>, <offset/r/W>, <no. of levels/r/W>`

### Functional pseudocode
```
1. Wn = <address>                                    ; micro 000753-000754
2. for i in 1..<no. of levels>:                      ; CHAIN_F02 007432.. loop
       link = IND(Wn + <offset>)     ; read the next link (word)
       if link == 0:                 ; CHAIN_F11 007446 COND,MZRO
             1 -> STATUS.K           ; micro 007443/007451 K,ONE
             stop the loop; Wn = last element (points at a zero location)
             -> Illegal Operand Value trap condition
       else:
             Wn = link
3. if <no. of levels> < 0  -> Illegal Operand Value trap
   if <no. of levels> == 0 -> behaves like a LADDR (just <address> -> Wn)
4. CHAIN_RES 007454-007455: ST,SAVA saves arithmetic status from the final
   address value:  S = sign bit of the last address; Z/C/O written from that
   ALU result.
```

### Operands + datatypes
`<address>` aa/W (start pointer; typically current B); `<offset>` r/W (B-relative
offset of the static link); `<no. of levels>` r/W (chain depth).
Result register `Wn` is the word index register selected by the opcode (n=1..4).

### Result / side-effects
`Wn` = base address `<no. of levels>` links up the static chain. Used by
compilers to reach variables of an enclosing procedure.

### Status flags
| Bit | Effect |
|-----|--------|
| K | SET to 1 if a link in the chain is zero (short chain); otherwise (all links non-zero) written by the ST,SAVA at CHAIN_RES - see note |
| Z | WRITTEN by ST,SAVA from the last-address value (micro 007455). Documented value UNKNOWN in the manual (only S is stated). |
| C | WRITTEN by ST,SAVA from the last-address value. Documented value UNKNOWN. |
| O | WRITTEN by ST,SAVA from the last-address value. Documented value UNKNOWN. |
| S | Sign bit of the last address (manual: "Last address.signbit -> S"; micro ST,SAVA). |
Manual "Data Status Bits: Last address.signbit -> S". The microcode's ST,SAVA
at CHAIN_RES 007455 writes the full Z/C/O/S group; the manual only documents S,
so the exact Z/C/O results are inferred-written but their documented meaning is
not stated (UNKNOWN). K is set on a zero link (manual + micro K,ONE).

### Traps
Addressing traps; Illegal Operand Value (IOV: zero link reached, or negative
<no. of levels>).

Citation: micro CHAIN 000753-000756 -> CHAIN_F01 007426 -> CHAIN_F02/F11
007432/007446 -> CHAIN_RES 007454; manual section 15.7 (Page 274).

================================================================================
## Category summary table (status bits)

| Instr | Opcode (oct) | K | Z | C | O | S | Frame action |
|-------|--------------|---|---|---|---|---|--------------|
| CALL   | 0303B   | - | - | - | - | - | stage args, jump to ENT* |
| CALLG  | 0265B   | - | - | - | - | - | as CALL, general subr operand |
| ENTM   | 0337B   | - | - | - | - | - | new module stack (cross-domain ok) |
| ENTD   | 0234B   | - | - | - | - | - | L<-ret only, 0 args |
| ENTS   | 0270B   | - | - | - | - | - | stack frame |
| ENTSN  | 0272B   | - | - | - | - | - | stack frame, bounded arg copy |
| ENTF   | 0335B   | - | - | - | - | - | fixed data area |
| ENTFN  | 0336B   | - | - | - | - | - | fixed area, bounded arg copy |
| ENTB   | 0275B   | - | - | - | - | - | heap/buddy element |
| ENTT   | 0176472B* | save | save | save | save | save | trap-handler entry |
| RET    | 0200B   | 0 | - | - | - | - | pop frame, K:=0 |
| RETK   | 0201B   | 1 | - | - | - | - | pop frame, K:=1 |
| IFKRET | 0235B   | - | - | - | - | - | pop frame IFF K=1 (K kept) |
| RETD   | 0202B   | - | - | - | - | - | P<-L |
| RETB   | 0177034B | 0 | - | - | - | - | free heap elem, K:=0 |
| RETBK  | 0177035B | 1 | - | - | - | - | free heap elem, K:=1 |
| RETT   | 0176473B* | rest | rest | rest | rest | rest | restore trap context |
| CHAIN  | 0176554B | 1 if 0-link | wr | wr | wr | sign | follow static chain |

Legend: `-` = unchanged; `0`/`1` = forced; `save` = saved into trap frame;
`rest` = restored from trap frame/DIT; `wr` = written by ST,SAVA (CHAIN);
`sign` = sign of last address. `*` = emulator opcode; manual prints a different
value (ENTT 0274B, RETT 0203B) - unresolved.

================================================================================
## UNRESOLVED (needs deeper microtrace or authority)

1. ENTT / RETT opcodes: emulator sources use 0xFD3A / 0xFD3B (0176472B /
   0176473B); the manual prints 0BCH/0274B (ENTT) and 083H/0203B (RETT). The
   microcode markdown lists MICRO addresses only, not the macro-opcode ->
   micro-address dispatch, so which byte the hardware actually decodes is not
   provable from this trace. Missing: the opcode-to-microaddress dispatch ROM.
2. CHAIN Z/C/O documented values: the microcode's ST,SAVA at CHAIN_RES 007455
   writes the full Z/C/O/S group, but the manual documents only S. The
   architectural meaning of the Z/C/O it writes for CHAIN is UNKNOWN (would
   need the exact ALU inputs at 007454/007455 decoded to bit level).
3. CALL/CALLG "Call trap (CT)" trigger: the manual lists CT as a trap
   condition, but the CT-raising microstep was not identified on the traced
   CALL/CALLG spine (000644-000653). Missing: where CT is asserted (likely a
   permission/vector check outside the traced cells).
4. RET cross-domain / stack-underflow full path (RET_SU 004417 -> RET_DOM00
   and the DIT reload sequence) was only partially followed; the exact register
   reload order on a cross-domain return is not fully enumerated here.
