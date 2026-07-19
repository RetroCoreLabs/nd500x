# ND-500 Instruction Category: SYSTEM - Functional Behavior Reference

Ground-truth source: microcode disassembly
`/mnt/e/Dev/Ronny/ND5000UC/microcode/MICRO-5800-A30.md`
Field decode: `/mnt/e/Dev/Ronny/ND5000UC/manual/mnemonics.md`
Documented intent: `/home/ronny/repos/nd500x/docs/ND-05.009.4 EN ND-500 Reference Manual.md`

This file was produced by TRACING each instruction's microcode routine (following
each microcell's `ADDR=`/`T,RETURN` chain, decoding ALU/A/B/D/K/ST/READ/WRITE
fields) and cross-checking against the ND-500 Reference Manual. Statements are
what the microcode/manual actually show; items that could not be resolved from
the traced routine are marked `UNKNOWN (needs deeper microtrace)`.

Instruction set = the 35 `.c` files in
`/home/ronny/repos/nd500x/src/cpu/instructions/SYSTEM/`.

---

## Decoding legend (how the microcells were read)

- `ALU,<f>`  = ALU function. `A` = pass A; `A-B`/`B-A` = subtract (with `CRY,ONE`
  = true two's-complement subtract); `A+B` = add; `AND`/`OR`/`XOR`; `ANDCA` =
  (NOT A) AND B (used to CLEAR selected bits); `ANDCB` = A AND (NOT B); `FZRO` =
  force 0; `A-1` = decrement.
- `A,<src>` / `B,<src>` = ALU inputs. `A,ALU,REG37` = the just-fetched operand
  value/address supplied by the operand-specifier logic; `A,DATA` = memory read
  latch; `A,MIC,STS` = micro status word; `A,SPEC,MOD` = MODUS register;
  `A,BMnn` = bit-mask constant (bit nn octal; `BM00` = 0); `A,SARG/LARG/MARG` =
  immediate constants; `A,DAC,EAO` = computed effective address.
- `D,<dest>` = where the ALU/F-bus result is written (`SCn` = scratch,
  `Xn` = index register In, `IAC,L`/`IAC,P` = link/program counter,
  `DAC,B`/`DAC,R` = base/record register, `DAC,REG04` = base register write path,
  `DAC,LDRES` = record-register load path, `SPEC,MOD` = MODUS,
  `MIC,STS` = status word, `DMM/IMM ...` = data/instruction MMU registers).
- `READ`/`RD,DOM`/`RD,PHYS`/`RD,POF` = data memory read; `WRITE`/`WR,PHYS`/
  `WR,POF` = data memory write; `LADDR` = perform address (ladder) request only
  (no data fetch); `EA1SAVE`/`ADACT` = compute/latch effective address.
- Status controls: `ST,SAVA` writes the four arithmetic data-status bits
  (Z,C,O,S) from the ALU result; `ST,ACCA`/`ST,SAVM` accumulate / save
  multiply status (feeds O); `K,ONE`=set K, `K,ZRO`=clear K,
  `K,1IFZ`=set K if ALU result==0. `G,OOPS`/`G,OPS` fetch the next
  instruction/operand specifier.
- Flag model used below: **K** (status bit 8, the "flag"), **Z** (zero),
  **C** (carry), **O** (overflow), **S** (sign). Manual rule: an instruction
  whose microcode executes `ST,SAVA` writes ALL FOUR of Z/C/O/S from that ALU
  operation even though the prose usually lists only the "interesting" one; the
  remaining bits take their literal value from the ALU op (typically C=0, O=0 for
  a pass-through `ALU,A`).

Privilege check pattern (shared by all privileged ops): read `MIC,STS` -> `AND`
with `BM01` (the PIA "privileged instructions allowed" bit, status bit 1) ->
conditional jump to `ILLEG` (Illegal Instruction Code trap) when the bit is 0.

---

## 1. LADDR - Load address

- Opcodes (octal): BIn 177040+(n-1), BYn 177044+(n-1), Hn 177050+(n-1),
  Wn/Fn 176474+(n-1), Dn 177054+(n-1). n = 1..4 selects target register In.
- Microcode entry: `LADDRN` @ octal 000761 (also `LADDRD4` @ 000763 for the
  double-scaled form).

FUNCTIONAL PSEUDOCODE:
```
1. LADDR request only (no data fetch): compute effective address of <operand>
   using the instruction's data-type scaling (LADDR + ADACT, no READ).
2. addr = DAC.EAO                     ; the computed effective address
3. In = addr        (D,ALU,REG37 via ORCON=20 selects target register)
4. ST,SAVA          ; Z/C/O/S written from the address value
5. -> GET_NEXT
```
- OPERANDS: one address-valued operand `<operand/aa/t>`; the type letter (BI/BY/
  H/W/F/D) only sets index scaling. Registers/constants are illegal (no address).
- RESULT: In := address of operand. No memory is read or written.
- STATUS FLAGS:

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | CONDITIONAL: address==0 -> 1 | CLEARED (pass-through, C=0) | CLEARED | CONDITIONAL: address bit31 |

  (`ST,SAVA` at 000762 writes all four; manual documents only "address=0 -> Z".)
- TRAPS: Addressing traps (protect violation / page fault during address calc).
- CITATION: microcode `LADDRN` 000761-000762; manual sec 15.4.

---

## 2. RLADDR - Load address into record register

- Opcodes (octal): BI 176125, BY 176132, H 176261, W/F 276, D 176262.
- Microcode entry: `RLADDR` @ octal 000766.

FUNCTIONAL PSEUDOCODE:
```
1. LADDR request only (LADDR + ADACT, AB=11): compute addr(<operand>).
2. R = addr   (result routed through D,DAC,LDRES = record-register load path)
3. ST,SAVA    ; Z/C/O/S from the address
4. G,OOPS (fetch next).  (000770 -> ILLEG guards register/constant operands)
```
- OPERANDS: one address-valued operand. Register/constant operands -> illegal.
- RESULT: R (record register) := address of operand.
- STATUS FLAGS: identical to LADDR (Z <- addr==0, S <- addr sign, C/O cleared;
  K unchanged). `ST,SAVA` at 000767.
- TRAPS: Addressing traps.
- CITATION: microcode `RLADDR` 000766-000770; manual sec 15.5.

---

## 3. BLADDR - Load address into base register

- Opcodes (octal): BI 176263, BY 176274, H 176467, W/F 176543, D 176470.
- Microcode entry: `BLADDR` @ octal 000771.

FUNCTIONAL PSEUDOCODE:
```
1. LADDR request only: compute addr(<operand>).
2. B = addr   (result routed through D,DAC,REG04 = base-register write path)
3. ST,SAVA    ; Z/C/O/S from the address
4. -> LOAD_L  (finish, fetch next)
```
- OPERANDS: one address-valued operand. Register/constant -> illegal.
- RESULT: B (local base register) := address of operand.
- STATUS FLAGS: as LADDR (Z <- addr==0, S <- addr sign, C/O cleared, K unchanged).
  `ST,SAVA` at 000772.
- TRAPS: Addressing traps.
- CITATION: microcode `BLADDR` 000771-000772; manual sec 15.6.

---

## 4. PHYLADR - Get physical address ('87 extension)

- Opcodes (octal): tn 177760+(n-1). n selects target index register In.
- Microcode entry: `PHYLADR` @ octal 001026 -> `PHLADR_1` @ 004561.

FUNCTIONAL PSEUDOCODE:
```
1. Compute effective (logical) address of <operand> (EA1SAVE + ADACT, DATOP).
2. K,ZRO                       ; clear the K flag (001027)
3. Translate logical addr -> physical ND-500 address via the MODUS/MMU path
   (PHLADR_1 reads SPEC,MOD and drives the address-translation machinery).
4. In := translated physical address.
```
- OPERANDS: one address-valued operand `<operand/aa/W>`.
- RESULT: In := tr(addr(operand)) = physical address of the operand.
- STATUS FLAGS:

| K | Z | C | O | S |
|---|---|---|---|---|
| CLEARED (K,ZRO @001027) | UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED |

  Manual leaves "Data status bits" blank; the traced entry cell clears K.
  The exact Z/S effect of the translated store past `PHLADR_1` was not followed
  to completion: UNKNOWN whether the physical-address store re-writes Z/S
  (needs deeper microtrace of PHLADR_1 tail).
- TRAPS: Addressing traps (translation may page-fault / protect-violate).
- CITATION: microcode `PHYLADR` 001026-001027, `PHLADR_1` 004561; manual sec 16.37.

---

## 5. LIND - Load index (with bounds check)

- Opcodes (octal): BYn 176441+(n-1), Hn 176420+(n-1), Wn 254+(n-1),
  Fn 177710+(n-1), Dn 177714+(n-1).
- Microcode entry: `LIND` @ octal 001576 -> `LIND_1` 003630 -> `LIND_2` 003632.

FUNCTIONAL PSEUDOCODE:
```
1. SC1 = <index>   (READ) with ST,SAVA   ; Z/C/O/S set from the index value
2. SC2 = <lower>   (READ) with K,ZRO     ; K pre-cleared
3. tmp = <upper> - <index>               ; (001600, A-B CRY,ONE)
4. tmp2 = <index> - <lower>              ; (001601)  conditional test
5. if (index < lower) or (index > upper):
        In = index ; K = 1 (K,ONE) ; -> ILL_INDEX (Illegal index trap)
   else:
        In = index ; K = 0 ; clear IX group bit in MIC,STS (LIND_2: ANDCA BM32)
```
- OPERANDS: `<index/r/t>, <lower/r/t>, <upper/r/t>`; In = target register.
- RESULT: In := index. IX status bit (MIC,STS bit selected by BM32) updated.
- STATUS FLAGS:

| K | Z | C | O | S |
|---|---|---|---|---|
| CONDITIONAL: out-of-range -> 1, in-range -> 0 | CONDITIONAL: index==0 -> 1 | from index ALU op | from index ALU op | CONDITIONAL: index sign bit |

  (`ST,SAVA` at 001576 saves Z/C/O/S from the loaded index; K,ZRO then K,ONE
  give the documented "0->K in range / 1->K out of range".)
- TRAPS: Addressing traps; Illegal index (IX) when out of range.
- CITATION: microcode `LIND` 001576-001602, `LIND_1/2` 003630-003633;
  manual sec 15.8.

---

## 6. CIND - Calculate index (multi-dimensional, with bounds check)

- Opcodes (octal): BYn 176424+(n-1), Hn 176430+(n-1), Wn 260+(n-1),
  Fn 177720+(n-1), Dn 177724+(n-1).
- Microcode entry (integer/word path): `CIND_W` @ octal 025476 ->
  `MUL4_WRITE` 025503; bounds check `CIND_CHK` @ 003144 / `CIND_1` 003151.

FUNCTIONAL PSEUDOCODE:
```
1. range = <upper> - <lower> + 1          ; 025476 A-B CRY,ONE ; 025477 +1, ST,ACCA
2. prod  = In * range                     ; 025500 AAP2,IMUL (integer multiply),
                                            ST,ACCM  ; sets O on multiply overflow
3. In = prod + <index>                    ; 025501 A+B, ST,ACCA
4. write result (MUL4_WRITE, ST,SAVM saves the multiply/overflow status)
5. CIND_CHK: if (index < lower) or (index > upper) [or IX bit already set]:
        set IX bit (OR BM32 into MIC,STS); K = 1 ; -> Illegal index trap
   else:
        clear IX bit ; K = 0
```
- OPERANDS: `<index/r/t>, <lower/r/t>, <upper/r/t>`; In accumulates the address.
- RESULT: In := In*(upper-lower+1) + index. IX bit updated.
- STATUS FLAGS:

| K | Z | C | O | S |
|---|---|---|---|---|
| CONDITIONAL: out-of-range -> 1, else 0 | CONDITIONAL: result==0 -> 1 | from ALU/accum | CONDITIONAL: integer overflow -> 1 | CONDITIONAL: result sign bit |

  (`ST,SAVA`/`ST,ACCA`/`ST,SAVM` accumulate Z/S and the overflow bit across the
  multiply-add; matches manual "result=0->Z, result.signbit->S, overflow->O".)
- TRAPS: Addressing traps; Integer overflow (O); Illegal index (IX).
- CITATION: microcode `CIND_W` 025476-025504, `CIND_CHK/CIND_1` 003144-003152;
  manual sec 15.9.

---

## 7. DCC - Data cache clear

- Opcode (octal): 177425.
- Microcode entry: `DCC` @ octal 000720 -> `DCC_IC` 011311 -> `CLR_DUDC`
  (dump-dirty) 014144 -> `CLR_DC` (invalidate) 014152.

FUNCTIONAL PSEUDOCODE:
```
1. push sequencer, -> CLR_DUDC : iterate all cache lines issuing CCD
   (Clear Cache and Dump Dirty): write every 'dirty' line back to memory.
2. -> CLR_DC : mark data-cache entries invalid (D,SPEC,CLDCA).
3. -> GET_NEXT.   (If no cache present, the loop is a no-op.)
```
- OPERANDS: none.
- RESULT: data cache invalidated, dirty lines flushed to memory. No register
  or status change.
- STATUS FLAGS: all UNCHANGED (no ST,SAVA / K on the path). Matches manual.

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED |

- TRAPS: None.
- CITATION: microcode `DCC` 000720, `DCC_IC` 011311-011313, `CLR_DUDC` 014144,
  `CLR_DC` 014152; manual sec 16.10.

---

## 8. PCC - Program cache clear

- Opcode (octal): 177424.
- Microcode entry: `PCC` @ octal 000717 -> `PCC_IC` 011307 -> `CLR_IC`.

FUNCTIONAL PSEUDOCODE:
```
1. push sequencer, -> CLR_IC : mark all program(instruction)-cache lines invalid.
2. -> DGET_NEXT.   (No dirty writeback: program cache is read-only. No-op if
   no cache present.)
```
- OPERANDS: none.
- RESULT: program cache invalidated. No register/status change.
- STATUS FLAGS: all UNCHANGED. Matches manual.

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED |

- TRAPS: None.
- CITATION: microcode `PCC` 000717, `PCC_IC` 011307-011310; manual sec 16.12.

---

## 9. DDIRT - Dump 'Dirty' ('87 extension)

- Opcode: manual gives Hex FFFA / octal 177772. (NOTE: the emulator source
  `Ddirt.c` header lists 0xFE1E - this disagrees with the manual; the manual is
  authoritative. DISCREPANCY flagged for the C# handoff.)
- Microcode entry: no distinct `DDIRT` label exists in this microcode image.
  The dirty-writeback primitive it must use is `CLR_DUDC` @ octal 014144
  (the CCD "clear cache and dump dirty" loop that `DCC`/`DCTSB` also call).

FUNCTIONAL PSEUDOCODE (from the manual + the shared primitive):
```
1. For each data-cache line marked 'dirty': write it back to main memory
   (CCD memory op, as in CLR_DUDC), WITHOUT invalidating the line.
2. No-op if no cache present.
```
- OPERANDS: none.
- RESULT: dirty data-cache lines flushed to memory. No register/status change.
- STATUS FLAGS: all UNCHANGED (manual: "Data status bits: Unaffected").

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED |

- TRAPS: None documented (manual). (`Ddirt.c` additionally enforces a privilege
  check -> IIC; the manual's DDIRT section lists no trap. UNRESOLVED: whether the
  real microcode privilege-checks DDIRT - its exact entry cell was not located.)
- CITATION: manual sec 16.11; shared primitive microcode `CLR_DUDC` 014144.
  Exact DDIRT dispatch cell: UNKNOWN (needs deeper microtrace of the 0xFFFx
  extension dispatch table).

---

## 10. DMON - Data memory management on (privileged)

- Opcode (octal): 177426.
- Microcode entry: `DMON` @ octal 000721 -> `DMON_1` 011314.

FUNCTIONAL PSEUDOCODE:
```
1. SC13 = MIC,STS ; SC13 &= PIA(BM01)
2. if PIA == 0: -> ILLEG (Illegal Instruction Code trap)          [privilege]
3. MODUS |= data-MMS-enable bit (OR BM33 into SC13 -> ... SPEC,MOD)
4. reprogram PS/MODUS (NEW_PS_2 path) and continue (-> LOAD_L).
   If MMS already on, the OR leaves it unchanged (effective no-op).
```
- OPERANDS: none.
- RESULT: subsequent DATA accesses are MMU-mapped. No status change.
- STATUS FLAGS: all UNCHANGED (no ST,SAVA/K). Matches manual.

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED |

- TRAPS: Illegal instruction code (IIC) if PIA=0.
- CITATION: microcode `DMON` 000721, `DMON_1` 011314-011321; manual sec 16.13.

---

## 11. PMON - Program memory management on (privileged)

- Opcode (octal): 177427.
- Microcode entry: `PMON` @ octal 000722 -> `PMON_1` 011322 -> `PMONOF` 011344.

FUNCTIONAL PSEUDOCODE:
```
1. SC13 = MIC,STS ; SC13 &= PIA(BM01)
2. if PIA == 0: -> ILLEG (IIC trap)
3. MODUS |= program-MMS-enable bit (OR BM23 into SC13 -> SPEC,MOD)
4. PMONOF: L -> P (LOADLA from IAC,L), i.e. continue at the virtual address
   held in L.  If MMS already on, control simply transfers to L (no further
   effect).
```
- OPERANDS: none.
- RESULT: subsequent INSTRUCTION fetches are MMU-mapped; execution resumes at
  the virtual address in L (L -> P).
- STATUS FLAGS: all UNCHANGED. Matches manual.

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED |

- TRAPS: Illegal instruction code (IIC) if PIA=0.
- CITATION: microcode `PMON` 000722, `PMON_1` 011322-011327, `PMONOF` 011344-011345;
  manual sec 16.14.

---

## 12. DMOF - Data memory management off (privileged)

- Opcode (octal): 177430.
- Microcode entry: `DMOF` @ octal 000723 -> `DMOF_1` 011330.

FUNCTIONAL PSEUDOCODE:
```
1. SC13 = MIC,STS ; SC13 &= PIA(BM01)
2. if PIA == 0: -> ILLEG (IIC trap)
3. MODUS &= ~data-MMS-enable bit (ANDCA BM33 -> SPEC,MOD) -> data accesses now
   interpreted as physical addresses.
4. continue (-> LOAD_L).  If already off, effective no-op.
```
- OPERANDS: none.  RESULT: data MMU disabled. No status change.
- STATUS FLAGS: all UNCHANGED. Matches manual.

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED |

- TRAPS: Illegal instruction code (IIC) if PIA=0.
- CITATION: microcode `DMOF` 000723, `DMOF_1` 011330-011335; manual sec 16.15.

---

## 13. PMOF - Program memory management off (privileged)

- Opcode (octal): 177431.
- Microcode entry: `PMOF` @ octal 000724 -> `PMOF_1` 011336 -> `PMONOF` 011344.

FUNCTIONAL PSEUDOCODE:
```
1. SC13 = MIC,STS ; SC13 &= PIA(BM01)
2. if PIA == 0: -> ILLEG (IIC trap)
3. MODUS &= ~program-MMS-enable bit (ANDCA BM23 -> SPEC,MOD)
4. PMONOF: L -> P ; resume at the (now physical) address in L.
   If already off, control transfers to physical L (no further effect).
```
- OPERANDS: none.  RESULT: program MMU disabled; L -> P.
- STATUS FLAGS: all UNCHANGED. Matches manual.

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED |

- TRAPS: Illegal instruction code (IIC) if PIA=0.
- CITATION: microcode `PMOF` 000724, `PMOF_1` 011336-011343, `PMONOF` 011344-011345;
  manual sec 16.16.

---

## 14. PCTSB - Clear program translation speedup buffer (privileged)

- Opcode (octal): 177434.
- Microcode entry: `PCTSB` @ octal 000725 -> `PCTSB_1` 011346.

FUNCTIONAL PSEUDOCODE:
```
1. SC13 = MIC,STS ; check PIA(BM01); if 0 -> ILLEG (IIC trap)
2. push, -> CLR_IC   : invalidate program cache.
3. push, -> CLR_ITSB : clear the program translation speedup buffer (TSB),
   forcing re-init from capability/segment/page tables.
4. -> LOAD_L.
```
- OPERANDS: none.  RESULT: program TSB + program cache cleared. No status change.
- STATUS FLAGS: all UNCHANGED. Matches manual.

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED |

- TRAPS: Illegal instruction code (IIC) if PIA=0.
- CITATION: microcode `PCTSB` 000725, `PCTSB_1` 011346-011353; manual sec 16.24.

---

## 15. DCTSB - Clear data translation speedup buffer (privileged)

- Opcode (octal): 177435.
- Microcode entry: `DCTSB` @ octal 000726 -> `DCTSB_1` 011354.

FUNCTIONAL PSEUDOCODE:
```
1. SC13 = MIC,STS ; check PIA(BM01); if 0 -> ILLEG (IIC trap)
2. push, -> CLR_DUDC : dump 'dirty' data-cache lines to memory.
3. push, -> CLR_DC   : invalidate data cache.
4. push, -> CLR_DTSB : clear data translation speedup buffer.
5. -> GET_NEXT.
```
- OPERANDS: none.  RESULT: data TSB + data cache cleared, dirty data flushed.
- STATUS FLAGS: all UNCHANGED. Matches manual.

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED |

- TRAPS: Illegal instruction code (IIC) if PIA=0.
- CITATION: microcode `DCTSB` 000726, `DCTSB_1` 011354-011362; manual sec 16.24.

---

## 16. RPGU - Read Page Used table (privileged)

- Opcodes (octal): BIn 177210+(n-1) (single bit), Hn 177214+(n-1) (16-bit group).
- Microcode entry: `RPGUBI` @ octal 000727 -> `RPGUBI_1` 011364 (bit form);
  `RPGUH` @ 000731 -> `RPGUH_1` 011375 (group form).

FUNCTIONAL PSEUDOCODE (bit form):
```
1. SC3 = <bit no.> (operand, READ).
2. RPGUBI_1: check PIA(BM01); if 0 -> ILLEG (IIC trap).
3. push loop count 8 (SARG=10 octal) -> WIP_PGU: build the physical byte address
   of the PGU-table word for this page number.
4. Read the PGU byte(s), mask out the requested bit / 16-bit group.
5. In = extracted bit or group ; ST,SAVA (RWIP_PGUH tail @011475/011502).
   Reading non-existent memory yields 0.
```
- OPERANDS: `<bit or group no./r/W>`; In = target register.
- RESULT: In := selected PGU bit (0/1) or 16-bit group. Returns logical OR of the
  separate program+data PGU tables.
- STATUS FLAGS:

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | CONDITIONAL: read value==0 -> 1 | from ALU op | from ALU op | from ALU op |

  (`ST,SAVA` on the extract writes Z; manual documents "bit or bit group = 0 -> Z".)
- TRAPS: Illegal instruction code (IIC); Illegal operand value (IOV).
- CITATION: microcode `RPGUBI` 000727-000730, `RPGUBI_1..3` 011364-011374,
  `RWIP_PGUBI/H` 011466-011502; manual sec 16.20.

---

## 17. ZPGU - Clear Page Used bit (privileged)

- Opcode (octal): BI 177220.
- Microcode entry: `ZPGUBI` @ octal 000733 -> `ZPGUBI_1` 011405 ->
  `ZWIP_PGUBI` 011510.

FUNCTIONAL PSEUDOCODE:
```
1. SC3 = <bit no.> (operand, READ).
2. ZPGUBI_1: check PIA(BM01); if 0 -> ILLEG (IIC trap).
3. build physical PGU byte address for the page; select the bit mask (from LC).
4. read byte (RD,POF), K,1IFZ tests whether the bit was already 0
   (011521 K,1IFZ), then clear it: byte &= ~mask ; write back (WR,POF).
5. clears the bit in BOTH program and data PGU tables (MODUS BM14 toggles table
   select across the two write passes).
```
- OPERANDS: `<bit no./r/W>`.
- RESULT: specified PGU bit := 0 in both tables.
- STATUS FLAGS:

| K | Z | C | O | S |
|---|---|---|---|---|
| INFERRED SET: K,1IFZ sets K if the cleared bit was already 0 | UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED |

  DISCREPANCY: manual says "Data status bits: Unaffected", but the traced
  microcode executes `K,1IFZ` at 011521 (the ZWIP/ZPGU shared clear path), so K
  is written = 1 when the selected bit was already zero. Flagged for handoff.
- TRAPS: Illegal instruction code (IIC); Illegal operand value (IOV).
- CITATION: microcode `ZPGUBI` 000733-000734, `ZPGUBI_1..3` 011405-011415,
  `ZWIP_PGUBI` 011510-011522; manual sec 16.21.

---

## 18. CPGU - Clear Page Used table (privileged)

- Opcode (octal): 177432.
- Microcode entry: `CPGU` @ octal 000735 -> `CPGU_1` 011450 -> `CWIPPGU` 011523.

FUNCTIONAL PSEUDOCODE:
```
1. SC13 = MIC,STS ; check PIA(BM01); if 0 -> ILLEG (IIC trap).
2. push loop count 8 -> WIP_PGU: get table base.
3. CPGU_3 loop: walk the whole PGU table, writing a fill pattern
   (LARG=12525252525 -> CWIPPGU) that zeroes the used-bits across all entries.
4. -> continue.  Whole PGU table cleared.
```
- OPERANDS: none.  RESULT: entire PGU table := 0.
- STATUS FLAGS: all UNCHANGED (no ST,SAVA/K on the clear-table path). Matches manual.

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED |

- TRAPS: Illegal instruction code (IIC).
- CITATION: microcode `CPGU` 000735, `CPGU_1..3` 011450-011456, `CWIPPGU` 011523;
  manual sec 16.22.

---

## 19. RWIP - Read Written In Page table (privileged)

- Opcodes (octal): BIn 177224+(n-1) (bit), Hn 177230+(n-1) (16-bit group).
- Microcode entry: `RWIPBI` @ octal 000736 -> `RWIPBI_1` 011416 ->
  `RWIP_PGUBI` 011466; group form `RWIPH` 000740 -> `RWIPH_1` 011427.

FUNCTIONAL PSEUDOCODE (bit form):
```
1. SC3 = <bit no.> (operand, READ).
2. RWIPBI_1: check PIA(BM01); if 0 -> ILLEG (IIC trap).
3. RWIP_PGUBI: compute physical WIP-table byte address for the page number;
   read the byte (RD,POF); AND with the bit mask from LC.
4. In = extracted bit (or 16-bit group in the H form) ; ST,SAVA (011475/011502).
   Returns logical OR of program+data WIP tables; non-existent memory -> 0.
```
- OPERANDS: `<bit or group no./r/w>`; In = target register.
- RESULT: In := selected WIP bit or group.
- STATUS FLAGS:

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | CONDITIONAL: value==0 -> 1 | from ALU op | from ALU op | from ALU op |

  (`ST,SAVA`; manual: "bit or bit group = 0 -> Z".)
- TRAPS: Addressing traps; Illegal instruction code (IIC).
- CITATION: microcode `RWIPBI` 000736-000737, `RWIPBI_1..3` 011416-011426,
  `RWIP_PGUBI/H` 011466-011507; manual sec 16.17.

---

## 20. ZWIP - Clear Written In Page bit (privileged)

- Opcode (octal): BI 177234.
- Microcode entry: `ZWIPBI` @ octal 000742 -> `ZWIPBI_1` 011437 ->
  `ZWIP_PGUBI` 011510.

FUNCTIONAL PSEUDOCODE:
```
1. SC3 = <bit no.> (operand, READ).
2. ZWIPBI_1: check PIA(BM01); if 0 -> ILLEG (IIC trap).
3. ZWIP_PGUBI: build physical WIP byte address; select bit mask;
   read byte (RD,POF); K,1IFZ (011521) tests old bit; clear it
   (byte &= ~mask) and write back (WR,POF).  Cleared in BOTH tables.
```
- OPERANDS: `<bit no./r/W>`.
- RESULT: specified WIP bit := 0 in both program and data WIP tables.
- STATUS FLAGS:

| K | Z | C | O | S |
|---|---|---|---|---|
| INFERRED SET: K,1IFZ -> K=1 if bit already 0 | UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED |

  Same DISCREPANCY as ZPGU: manual says "Unaffected"; microcode 011521 does
  `K,1IFZ` on the shared clear path. Flagged.
- TRAPS: Illegal instruction code (IIC); Illegal operand value (IOV).
- CITATION: microcode `ZWIPBI` 000742-000743, `ZWIPBI_1..3` 011437-011447,
  `ZWIP_PGUBI` 011510-011522; manual sec 16.18.

---

## 21. CWIP - Clear Written In Page table (privileged)

- Opcode (octal): 177433.
- Microcode entry: `CWIP` @ octal 000744 -> `CWIP_1` 011457 -> `CWIPPGU` 011523.

FUNCTIONAL PSEUDOCODE:
```
1. SC13 = MIC,STS ; check PIA(BM01); if 0 -> ILLEG (IIC trap).
2. push loop count 8 -> WIP_PGU: get table base.
3. CWIP_3 loop: walk whole WIP table writing the zero-fill pattern
   (LARG=25252525252 -> CWIPPGU) that clears all written-in-page bits.
```
- OPERANDS: none.  RESULT: entire WIP table := 0.
- STATUS FLAGS: all UNCHANGED. Matches manual.

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED |

- TRAPS: Illegal instruction code (IIC).
- CITATION: microcode `CWIP` 000744, `CWIP_1..3` 011457-011465, `CWIPPGU` 011523;
  manual sec 16.19.

---

## 22. RDUS - Load bypassing cache

- Opcodes (octal): BIn 177240+(n-1), BYn 177244+(n-1), Hn 177250+(n-1),
  Wn 177254+(n-1).
- Microcode entry: `RDUSW` @ octal 000751 -> `RDUS_1` 004515 (word/integer);
  `RDUSBI` @ 000747 -> `RDUSBI_1` 004505 (bit).

FUNCTIONAL PSEUDOCODE (word form):
```
1. LADDR + EA1SAVE: compute effective address of {source}.
2. RDUS_1: temporarily set the cache-bypass bit in MODUS
   (read SPEC,MOD -> SC4 ; SC13 = SC4 | BM05 ; MODUS = SC13 & ~BM06).
3. READ {source} from main memory (bypassing cache) -> value.
4. In = value ; ST,SAVA (004522)  ; Z/C/O/S from the loaded value.
5. restore MODUS (SC4 -> SPEC,MOD) ; -> LOAD_L.
   (Value is also brought into cache for later refs, per manual.)
```
- OPERANDS: `{source/r/t}`; In = target register. Register/constant operands
  are illegal (IOS trap).
- RESULT: In := {source} loaded directly from memory (cache bypassed).
- STATUS FLAGS:

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | CONDITIONAL: source==0 -> 1 | from ALU op | from ALU op | CONDITIONAL: source sign bit |

  (`ST,SAVA` 004522; manual: "{source}=0 -> Z, {source}.signbit -> S".)
- TRAPS: Addressing traps; Illegal operand specifier (IOS).
- CITATION: microcode `RDUSW` 000751-000752, `RDUS_1` 004515-004523;
  `RDUSBI` 000747, `RDUSBI_1` 004505-004514; manual sec 16.25.

---

## 23. WDUS - Store bypassing cache

- Opcodes (octal): BIn 177260+(n-1)? (source header lists BIn/BYn/Hn/Wn:
  BYn 177264, Hn 177270, Wn 177274; per emulator `Wdus.c`). Store analog of RDUS.
- Microcode entry: `WDUSW` @ octal 001671 -> `WDUS_1` 004476 (word);
  `WDUSBY` 001651, `WDUSH` 001661, `WDUSBI` -> `WDUSBI_1` 004466 (bit).

FUNCTIONAL PSEUDOCODE (word form):
```
1. compute effective address (LADDR/EA1SAVE, D,SC12 holds value; ORB,IN merges
   the destination operand).
2. WDUS_1: set cache-bypass bit in MODUS (as RDUS: SC13 = MODUS | BM05,
   MODUS &= ~BM06).
3. value = In (register operand) ; WRITE value to memory bypassing cache
   (004503 WRITE, C,MEMOT) ; ST,SAVA on the stored value.
4. restore MODUS ; -> LOAD_L.
```
- OPERANDS: `<value>` register and a destination; the store goes straight to
  memory. Register/constant destination illegal (store rule).
- RESULT: memory[dest] := value, written past the cache (kept consistent).
- STATUS FLAGS:

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | CONDITIONAL: stored value==0 -> 1 | from ALU op | from ALU op | CONDITIONAL: value sign bit |

  (`ST,SAVA` at 004503. The manual's WDUS entry was not printed in the extracted
  range; the traced routine saves arithmetic status from the stored value.
  UNKNOWN: manual's exact documented status wording for WDUS - not located in
  the manual excerpt; flag effect stated here is from the microcode.)
- TRAPS: Addressing traps; Illegal operand specifier (store to register/const).
- CITATION: microcode `WDUSW` 001671, `WDUS_1` 004476-004504; `WDUSBI_1` 004466;
  manual sec ~16.25 (store-bypass companion).

---

## 24. RPHS - Read from physical segment ('87 extension, privileged)

- Opcode (octal): 177765.
- Microcode entry: `RPHS` @ octal 001057 -> `RPHS_1` 007702
  (byte-copy engine `RPHS_SX/SW/SB`).

FUNCTIONAL PSEUDOCODE:
```
1. SC3 = <domain number> (operand, READ).
2. RPHS_1: check PIA(BM01); if 0 -> ILLEG (IIC trap).
3. set up: I4 = physical segment number, I3 = addr on physical segment,
   I2 = logical addr on the domain, I1 = byte count; select target domain
   via DMM,PHS/DMM,DOM.
4. while I1 > 0 and not crossing a physical-segment page boundary:
       byte = S(I4.I3)                 ; RD,PHYS from the physical segment
       D(<domain>.I2) = byte           ; WRITE to the domain
       I3++ ; I2++ ; I1--              ; (007742/007756/007772 update indices,
                                         ST,SAVA on the I1 count each pass)
5. terminate when I1==0 or page boundary reached.
```
- OPERANDS: `<domain number/r/W>`; implicit I1..I4 as above.
- RESULT: block of bytes copied from a physical segment to a domain; I1/I2/I3
  updated (I1 counts down toward 0).
- STATUS FLAGS:

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | CONDITIONAL: no bytes left (I1==0) -> 1 ; page-boundary-and-none-left -> 0 | from count ALU op | from count ALU op | from count ALU op |

  (`ST,SAVA` on the decremented I1; manual: "no bytes left = 0 : 1 -> Z ;
  page boundary and no bytes left < 0 : 0 -> Z".)
- TRAPS: (privileged) Addressing traps during the copy; IIC if PIA=0.
- CITATION: microcode `RPHS` 001057-001060, `RPHS_1` 007702-007712,
  `RPHS_SX/SW/SB` 007713-007774; manual sec 16.31.

---

## 25. WPHS - Write to physical segment ('87 extension, privileged)

- Opcode (octal): 177764.
- Microcode entry: `WPHS` @ octal 001061 -> `WPHS_1` 007607
  (byte-copy engine `WPHS_SX/SW/SB`).

FUNCTIONAL PSEUDOCODE:
```
1. SC3 = <domain number>; SC6/MIC,VECT setup.
2. WPHS_1: check PIA(BM01); if 0 -> ILLEG (IIC trap).
3. same register roles as RPHS (I4=phys seg, I3=phys addr, I2=domain addr,
   I1=count) but direction reversed:
4. while I1 > 0 and not crossing a physical-segment page boundary:
       byte = S(<domain>.I2)           ; READ from the domain
       D(I4.I3) = byte                 ; WR,PHYS to the physical segment
       I3++ ; I2++ ; I1--              ; ST,SAVA on I1
5. terminate on I1==0 or page boundary.
```
- OPERANDS: `<domain number/r/W>`; implicit I1..I4.
- RESULT: block of bytes copied from a domain to a physical segment; indices
  updated.
- STATUS FLAGS: identical rule to RPHS (Z <- I1 exhausted; K/C/O/S per count op).

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | CONDITIONAL: I1==0 -> 1 ; page-boundary-and-none-left -> 0 | from count op | from count op | from count op |

- TRAPS: (privileged) Addressing traps; IIC if PIA=0.
- CITATION: microcode `WPHS` 001061-001062, `WPHS_1` 007607-007617,
  `WPHS_SX/SW/SB` 007620-007701; manual sec 16.32.

---

## 26. LREGBL - Load register block ('87 extension)

- Opcode (octal): 177766.
- Microcode entry: `LREGBL` @ octal 001030 -> `LOADRG_1`.

FUNCTIONAL PSEUDOCODE:
```
1. SC3 = <mask> (operand, READ) ; SC4 = <address> (operand, READ).
2. SC6 = 1 (bit walker).
3. LOADRG_1 loop over register-number bits 0..37 set in <mask>:
       In/reg = memory[ <address> + regnum*4 ]     (logical load)
   Registers residing in the Domain Information Table (DIT) are updated in the
   DIT; in non-privileged mode the mask is reduced so ST2/PS/CED/CAD/CTE/MTE/
   TEMM cannot be modified.
```
- OPERANDS: `<mask/r/W>, <address/r/W>`.
- RESULT: the masked registers loaded from the logical save area at
  address + regnum*4 (regnum per the chapter-16.27 register-number table:
  P=0,L=1,B=2,R=3,I1..I4=4..7,A1..A4=10..13,E1..E4=14..17,STS=20,PS=21,TOS=22,
  LL=23,HL=24,THA=25,CED=26,CAD=27,MIC=30,OTE=31,...).
- STATUS FLAGS: all UNCHANGED (no ST,SAVA/K on the dispatch cells).

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED |

  (Loading STS/PS from the block can of course replace the whole status word;
  that is a load, not a computed data-status update.)
- TRAPS: Addressing traps (block access); reduced-mask enforcement in
  non-privileged mode.
- CITATION: microcode `LREGBL` 001030-001032 (`LOADRG_1` loop body only
  partially traced); manual sec 16.27 / 16.27.2. Full per-bit loop body:
  described from the manual register table (microtrace of LOADRG_1 tail not
  exhaustively followed).

---

## 27. SREGBL - Save register block ('87 extension)

- Opcode (octal): 177767.
- Microcode entry: `SREGBL` @ octal 001033 -> `STORERG_1`.

FUNCTIONAL PSEUDOCODE:
```
1. SC3 = <mask> (READ) ; SC4 = <address> (READ) ; SC6 = 1 (bit walker).
2. STORERG_1 loop over register-number bits set in <mask>:
       memory[ <address> + regnum*4 ] = reg          (logical store)
   DIT-resident registers are read from the DIT and stored if masked.
```
- OPERANDS: `<mask/r/W>, <address/r/W>`.
- RESULT: masked registers stored to logical memory at address + regnum*4.
- STATUS FLAGS: all UNCHANGED.

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED |

- TRAPS: Addressing traps.
- CITATION: microcode `SREGBL` 001033-001035 (`STORERG_1` loop body only
  partially traced); manual sec 16.27 / 16.27.1.

---

## 28. LCNTXT - Load context block ('87 extension, privileged)

- Opcode (octal): 177770.
- Microcode entry: `LCNTXT` @ octal 001036 -> `LOADCT_1` 010002.

FUNCTIONAL PSEUDOCODE:
```
1. SC3 = <mask> (READ) ; SC4 = <address> (READ) ; SC5 = <process number> (READ,
   001040 tests <address>==0 via CSAVE/COND,MZRO).
2. LOADCT_1 loop over masked register numbers:
       reg = PHYSICAL_memory[ base + regnum*4 ]
   where base = <address> if nonzero, else the context save area of process
   <process number>, addressed by (procno+1)*400B + OS-defined offset.
   If process number < 0, the current process number is kept.
3. DIT-resident registers loaded into the DIT; PIA updated from the new DIT.
```
- OPERANDS: `<mask/r/W>, <address/r/W>, <process number/r/W>`.
- RESULT: context of the target process loaded from PHYSICAL addresses per mask.
- STATUS FLAGS: all UNCHANGED (no ST,SAVA/K on dispatch cells).

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED |

- TRAPS: Privileged (uses physical addressing) -> IIC if PIA=0; addressing traps.
- CITATION: microcode `LCNTXT` 001036-001041, `LOADCT_1` 010002 (loop body only
  partially traced); manual sec 16.27 / 16.27.4.

---

## 29. SCNTXT - Save context block ('87 extension, privileged)

- Opcode (octal): 177771.
- Microcode entry: `SCNTXT` @ octal 001042 -> `STORECT_1` 010226.

FUNCTIONAL PSEUDOCODE:
```
1. SC3 = <mask> (READ) ; SC4 = <address> (READ) ; SC6=1 ;
   001044 tests <address>==0 (CSAVE/COND,MZRO).
2. STORECT_1 loop over masked register numbers:
       PHYSICAL_memory[ base + regnum*4 ] = reg
   base = <address> if nonzero, else context save area of the CURRENT process
   ((procno+1)*400B + OS-defined offset).
```
- OPERANDS: `<mask/r/W>, <address/r/W>`.
- RESULT: current process context stored to PHYSICAL addresses per mask.
- STATUS FLAGS: all UNCHANGED.

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED |

- TRAPS: Privileged -> IIC if PIA=0; addressing traps.
- CITATION: microcode `SCNTXT` 001042-001044, `STORECT_1` 010226 (loop body only
  partially traced); manual sec 16.27 / 16.27.3.

---

## 30. INT - Integer part (truncated)

- Opcodes (octal): Fn 177140+(n-1) (single float), Dn 177144+(n-1) (double).
  (Emulator `Int.c` uses hex 0xFE60-0xFE63 / 0xFE64-0xFE67, matching the manual.)
- Microcode entry: `INTF` @ octal 002607 -> `INTF_0` 017345 (single);
  `INTD` @ 002611 -> `INTD_0` 017365 (double).

FUNCTIONAL PSEUDOCODE (single-float):
```
1. SC5 = <x> (READ, float).
2. INTF_0: SC1 = AAP( AAP_CTRL=377 , SC5 )   ; AAP integer-part operation on the
   floating value (truncate toward zero, no rounding).
3. exponent/normalization handling (INTF_1/INTF_2): if |x| is already integral or
   too large, pass value through unchanged; otherwise mask off the fractional
   mantissa bits (ANDCA of the fraction mask) to truncate.
4. In (Fn) = truncated integer value, still in FLOAT format ; ST,SAVA sets status.
```
- OPERANDS: `<x/r/t>` (F or D); In = target floating register (result in float
  format).
- RESULT: In := trunc(x) expressed as a float.
- STATUS FLAGS:

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | CONDITIONAL: result==0 -> 1 | from ALU op | from ALU op | CONDITIONAL: result sign bit |

  (`ST,SAVA` at 017346/017350; manual: "result=0 -> Z, result.signbit -> S".)
- TRAPS: Addressing traps.
- CITATION: microcode `INTF/INTD` 002607-002612, `INTF_0..2` 017345-017354,
  `INTD_0..2` 017365-017374; manual sec 10.34.

---

## 31. INTR - Integer part with rounding

- Opcodes (octal): Fn 177150+(n-1), Dn 177154+(n-1).
  (Emulator `Intr.c`: 0xFE68+(n-1) / 0xFE6C+(n-1), matching the manual.)
- Microcode entry: `INTRF` @ octal 002613 -> `INTRF_0` 017355 (single);
  `INTRD` @ 002615 -> `INTRD_0` 017375 (double).

FUNCTIONAL PSEUDOCODE (single-float):
```
1. SC5 = <x> (READ, float).
2. INTRF_0: SC1 = AAP( AAP_CTRL=377 , SC5 ) integer-part operation.
3. INTRF_1: like INT but adds the round bit before truncating: A-1 on BM26
   (round-position mask), so the fraction is rounded to nearest integer rather
   than truncated toward zero (017362/017364 add-and-mask, ST,SAVA).
4. In (Fn) = rounded integer value in FLOAT format.
```
- OPERANDS: `<x/r/t>`; In = target floating register.
- RESULT: In := round(x) as a float.
- STATUS FLAGS: same as INT (Z <- result==0, S <- result sign, C/O per op, K
  unchanged). `ST,SAVA`.

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | CONDITIONAL: result==0 -> 1 | from ALU op | from ALU op | CONDITIONAL: result sign bit |

- TRAPS: Addressing traps.
- CITATION: microcode `INTRF/INTRD` 002613-002616, `INTRF_0/U/#/1` 017355-017364,
  `INTRD_0/U/1` 017375-017377; manual sec 10.35.

---

## 32. TUTTI - Enable process switch

- Opcode (octal): 177001.
- Microcode entry: `TUTTI` @ octal 000712 -> `TUTTI_0` 004534.

FUNCTIONAL PSEUDOCODE:
```
1. SC13 = MODUS (SPEC,MOD).
2. MIC,STS &= ~PSD-bit  (TUTTI_0 004535 ANDCB clears the process-switch-disable
   status bit).
3. MODUS &= ~BM25 (004536) -> re-enable process switching in the address/modus
   machinery ; RF2 updated (004537) ; -> LOAD_L.
```
- OPERANDS: none.
- RESULT: process switching re-enabled (complement of SOLO). Clears PSD.
- STATUS FLAGS: all UNCHANGED (no ST,SAVA/K). Matches manual.

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED |

- TRAPS: None.
- CITATION: microcode `TUTTI` 000712, `TUTTI_0` 004534-004537; manual sec 16.2.

---

## 33. FREEB - Free buddy element

- Opcode (octal): 176666.
- Microcode entry: `FREEB` @ octal 001000 -> `FREEB_1` 004403 -> `RELBDY`.

FUNCTIONAL PSEUDOCODE:
```
1. SC7 = <log size> (operand, READ, byte).
2. size = 2 * 2^<log size> (001001 A+B,*2 doubling to word count) ; fetch the
   <element> operand specifier (G,OPS).
3. FREEB_1: LADDR/EA1SAVE compute the element's address ; push -> RELBDY:
   append the element (size 2**<log size> words) to the appropriate freelist of
   the heap described by the variables pointed to by the TOS register.
   Elements are NOT coalesced.  If <element> uses a DESC prefix, the index
   register is not updated.
```
- OPERANDS: `<log size/r/BY>, <element/s/W>`. Requires write access to element;
  TOS must point to the heap descriptor block.
- RESULT: element linked onto the heap freelist for its size class. No register
  result.
- STATUS FLAGS: all UNCHANGED (no ST,SAVA/K on the free path). Matches manual
  ("Data status bits: Unaffected").

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED |

- TRAPS: Addressing traps.
- CITATION: microcode `FREEB` 001000-001001, `FREEB_1` 004403-004405 (-> RELBDY);
  manual sec 15.14.

---

## 34. SLOCA - String locate element

- Opcodes (octal): BI 176657 (bit string), BY 176660 (byte string).
- Microcode entry: `SLOCABI_F01` @ octal 007142 (bit); `SLOCABY_F01` @ 007164
  (byte). Dispatch cells `SLOCABI` 001330 / `SLOCABY` 001333.

FUNCTIONAL PSEUDOCODE (byte form):
```
1. Read <test> operand ; set up source descriptor: I1 = current index into
   <source>, remaining-count from the descriptor.
2. loop (SLOCABY_F10):
       if source exhausted (count reached 0):
           K=0 ; Z=0 (source empty) ; I1 -> next element ; terminate
       elem = S(I1)                        ; READ current element
       if elem == <test>:
           K=0 ; Z=1 (found) ; I1 -> found element ; terminate
       I1 = I1 + 1                         ; advance
   (compare uses ST,SAVA / K,ZRO / K,1IFZ across 007144-007163.)
3. If the scan runs outside the source range -> Descriptor Range (DR) trap
   with K=0, Z=1, I1 unmodified (SOUR_RANGE path, 007150 K,ZRO).
```
- OPERANDS: `(<source/r/t/I1=>, <test/r/BI,BY>)`; I1 is the running index.
- RESULT: I1 positioned at the matching element (or next element if empty);
  termination condition encoded in K/Z.
- STATUS FLAGS:

| K | Z | C | O | S |
|---|---|---|---|---|
| CLEARED (K=0 on all documented terminations) | CONDITIONAL: found or outside-source -> 1 ; source-empty -> 0 | from compare op | from compare op | from compare op |

  Manual terminating conditions: outside source -> K=0,Z=1,DR trap ;
  element==test -> K=0,Z=1 ; source empty -> K=0,Z=0. `ST,SAVA` at 007147/007163/
  007167 saves the compare status.
- TRAPS: Descriptor Range (DR) trap when scanning outside the source.
- CITATION: microcode `SLOCABI_F01..F12` 007142-007163, `SLOCABY_F01..F12`
  007164-007175; manual sec 14.15.

---

## 35. SVERS - Store microprogram version ('87 extension)

- Opcode (octal): 177773.
- Microcode entry: `SVERS` @ octal 001051 -> `SVERS_1` 011023 -> `VERSION` 000001.

FUNCTIONAL PSEUDOCODE:
```
1. compute destination address (001051 ADACT).
2. SVERS_1 (011023): SC2 = destination path ; -> VERSION.
3. VERSION (000001): SC13 = LARG = octal 026246   ; the microprogram version
   constant (this specific microcode image = octal 026246 = hex 0x2CA6).
4. store SC13 (the version word) to <destination>.
```
- OPERANDS: `<destination/w/W>` (write address).
- RESULT: memory[destination] := microprogram version number (this image:
  octal 026246). NOTE: emulator `Svers.c` writes 0x00010000 instead - DISCREPANCY
  vs the traced microcode constant; flagged for handoff.
- STATUS FLAGS: manual: "Status bit set according to version." For this image the
  version constant is nonzero and positive, so:

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | CONDITIONAL: version==0 -> 1 (here 0) | UNKNOWN | UNKNOWN | CONDITIONAL: version bit31 (here 0) |

  The store-side status write was not observed to use `ST,SAVA` in the two traced
  cells (`VERSION` has no ST,SAVA). UNKNOWN whether the shared store tail sets
  Z/S: needs deeper microtrace of the SVERS store path past `VERSION`'s RETURN.
- TRAPS: None documented.
- CITATION: microcode `SVERS` 001051-001052, `SVERS_1` 011023, `VERSION` 000001;
  manual sec 16.35.

---

## Cross-check summary (microcode vs manual)

- LADDR/RLADDR/BLADDR/PHYLADR: microcode confirms manual (address load, Z on
  zero address). Microcode additionally writes S/C/O via ST,SAVA (address sign);
  manual lists only Z. PHYLADR clears K (K,ZRO) - manual silent.
- LIND/CIND: microcode confirms bounds check, K set on out-of-range + illegal
  index trap, IX bit maintained in MIC,STS (BM32); CIND overflow via multiply
  status (ST,SAVM). Full agreement with manual.
- Cache/MMU/TSB (DCC/PCC/DDIRT/DMON/PMON/DMOF/PMOF/PCTSB/DCTSB): microcode
  confirms privilege check (PIA/BM01 -> ILLEG/IIC) for the MMU/TSB ops and the
  no-op-if-already-in-state behaviour; PMON/PMOF confirm L->P. Cache-only ops
  (DCC/PCC/DDIRT) show no privilege gate in the traced primitive.
- PGU/WIP reads (RPGU/RWIP): confirmed OR of prog+data tables, Z on zero.
  PGU/WIP clears (ZPGU/ZWIP): microcode executes K,1IFZ - CONTRADICTS the manual
  "Data status bits: Unaffected". Table clears (CPGU/CWIP): unaffected, agrees.
- RDUS/WDUS: confirmed cache-bypass via MODUS bit toggle, Z/S from value.
- RPHS/WPHS: confirmed privileged byte-copy engine with page-boundary stop and
  Z from the I1 count.
- REGBL/CNTXT: confirmed mask-driven register block load/store; CNTXT uses
  physical addressing (privileged). Loop bodies described from the manual
  register-number table (not exhaustively microtraced).
- INT/INTR: confirmed AAP integer-part (truncate vs round), result in float,
  Z/S from result.
- TUTTI: confirmed clears the process-switch-disable (PSD) status bit.
- FREEB/SLOCA/SVERS: FREEB confirmed heap freelist append (unaffected status);
  SLOCA confirmed element scan with K/Z terminating semantics; SVERS version
  constant in THIS microcode image = octal 026246 (differs from the emulator).

## Still-UNKNOWN items (need deeper microtrace)

1. DDIRT - exact microcode dispatch/entry cell not located in this image (0xFFFx
   extension dispatch); behaviour described from the shared CLR_DUDC primitive +
   manual. Also: whether real microcode privilege-checks DDIRT.
2. SVERS - whether the store tail past `VERSION` writes Z/S (C/O left UNKNOWN).
3. PHYLADR - whether the physical-address store tail (`PHLADR_1`) re-writes Z/S
   after the K,ZRO in the entry cell.
4. WDUS - manual's exact documented status wording was not in the extracted
   manual range (flag effect stated is from the microcode ST,SAVA).
5. REGBL/CNTXT per-bit loop bodies (LOADRG_1/STORERG_1/LOADCT_1/STORECT_1) were
   only partially followed; the register-by-register transfer order is taken from
   the manual register-number table rather than an exhaustive microtrace.
