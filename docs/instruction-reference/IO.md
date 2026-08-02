# ND-500 Instruction Category: IO

Functional behavior reference built by TRACING THE 5800 MICROCODE
(`$ND5000UC/microcode/MICRO-5800-A30.md`), decoding fields via
`$ND5000UC/manual/mnemonics.md`, and cross-checking documented
intent against the ND-500 Reference Manual
(`docs/ND-05.009.4 EN ND-500 Reference Manual.md`).

Rule of evidence: statements below are taken from the actual microcells and the
manual. Anything not directly derivable from those is marked UNKNOWN or inferred.

Category source directory: `src/cpu/instructions/IO/`
Instructions in this category (one .c file each):

- `Riom.c` -> RIOM

---

## RIOM - Read I/O Processor Memory

- Mnemonic / assembly form: `H RIOM <ND-100 addr/r/W>,<buffer/w/H>,<no of halfwords>`
- Opcode: octal `0177166` (hex `0xFE76`, decimal 65142)
- Class: IO, privileged
- Operand count: 3
- Manual section: 16.23 (Reference Manual page 297/311)

### Microcode routine (ground truth)

Entry dispatch (operand-fetch stage, ORCON=04 = OR A/D from operand specifier):

```
000745 RIOM   : ALU,A A,ALU,REG37 B,X1 D,SC1  READ ADACT ORCON=04  -> 000746
000746        : ALU,A A,BM01     B,X1 D,SC2                        -> RIOM_0 (011271)
```

Privilege check:

```
011271 RIOM_0 : ALU,A   A,BM01    B,X1 D,SC2                       -> 011272
011272        : ALU,AND A,MIC,STS B,SC2                            -> 011273
011273        : ALU,FZRO A,BM00   B,X1  C,SEQ COND,MZRO  -> ILLEG (000200) on zero
```

Operand address setup:

```
011274 RIOM_1 : ALU,XOR A,BM00 B,X1  G,OPS                        -> 011275
011275        : ALU,A A,ALU,REG37 B,X1 D,SC2  G,OPS LADDR EA2SAVE ADACT ORCON=04 -> 011276
011276        : ALU,A A,ALU,REG37 B,X1 D,LC   READ ADACT ORCON=04 -> 011277
011277        : ALU,A EXUC A,SC1 B,X1 D,DAC,DPA COND,MSEXO         -> DUMMY (pipeline break)
011300        : ALU,FZRO EXUC A,BM00 B,X1  AD_ARTI=1 EA2SAVE ADACT AA=6 AB=1 IX*8 ORCON=0x3E -> DUMMY
011301        : ALU,FZRO A,BM00 B,X1  T,JMP AD_ARTI=1 EA1SAVE ADACT AA=2 AB=1 IX*8 ORCON=0x3E -> RIOM_2
```

Transfer loop:

```
011302 RIOM_2 : ALU,FZRO A,BM00 B,X1 LCDECR C,SEQ T,JMP INVSEQ COND,LCZ
                AD_ARTI=1 EA1SAVE ADACT AA=5 AB=1 ORCON=0x02  -> RIOM_3 while LC!=0
011303        : ALU,FZRO A,BM00 B,X1 T,JMP -> GET_NEXT (003231)   (loop exit, LC==0)
011304 RIOM_3 : ALU,A  TYP,HW A,DATA B,X1 D,SC1  RD,POF           -> 011305   (read halfword src)
011305        : ALU,XOR TYP,HW A,BM00 B,X1  AD_ARTI=1 EA2SAVE ADACT AA=6 AB=1 ORCON=0x02 -> 011306
011306        : ALU,A  TYP,HW A,SC1  B,X1  WRITE                  -> RIOM_2   (write halfword dst)
```

Field decode used above (from mnemonics.md):
- `A,MIC,STS` = MIC status bits; `ALU,AND ... COND,MZRO -> ILLEG` = privilege/mode
  bit test; branch to ILLEG (the illegal-instruction handler at octal 000200) when
  the masked bit is zero. This is the IIC (Illegal Instruction Code) trap.
- `LADDR` = ladder (address-translation) request; `EA1SAVE`/`EA2SAVE` = save computed
  address into EA1/EA2 (and EAO); `ADACT` = address-arithmetic activate; `AD_ARTI=1`
  with AA/AB = pointer increment (AA=5 EA1, AA=6 EA2, AB=1 MARG stride).
- `D,LC` = load Loop Counter; `LCDECR` = decrement LC; `COND,LCZ` + `INVSEQ` = loop
  while LC != 0.
- `RD,POF` = physical read with MMS (read source halfword); `WRITE` = write data
  memory (store halfword to ND-500 buffer). `TYP,HW` = 16-bit halfword element.
- No `ST,SAVA` / `ST,SAV*` / `K,*` appears anywhere in 000745-011306: the routine
  writes NO arithmetic status and NO K flag.

### Functional pseudocode (traced data path)

```
RIOM src_addr(op0, W, read), buffer(op1, H, write addr), count(op2, halfwords):

  1. Fetch operand 0 (ND-100 physical address, Word):  SC1 <- value(op0)
  2. Privilege check:
        if (MIC.STS AND privilege_mask) == 0:
            trap ILLEG            ; Illegal Instruction Code (IIC) - privileged instr
  3. Fetch operand 1 (ND-500 buffer, logical address):  EA2 <- address(op1)   ; LADDR translated
  4. Fetch operand 2 (halfword count):                  LC  <- value(op2)
  5. DAC.DPA <- SC1                                      ; source physical data address
     EA1 <- source pointer,  EA2 <- destination pointer  ; initialized via AD_ARTI
  6. loop (RIOM_2):
        if LC == 0: goto GET_NEXT          ; done, fetch next instruction
        LC <- LC - 1
        EA1 <- EA1 + stride                ; advance source (halfword)
        h  <- read_halfword(source)        ; RD,POF, TYP,HW   -> SC1
        EA2 <- EA2 + stride                ; advance destination (halfword)
        write_halfword(destination, SC1)   ; WRITE, TYP,HW
        goto loop
  7. (fall out of loop) -> GET_NEXT
```

Note: the ND-100 source is read via DMA/physical-read path (`RD,POF`, DAC.DPA set
from the ND-100 address). Per manual, this does not interrupt ND-100 execution.

### Operands and datatypes

| # | Role | Direction | Datatype | Microcode evidence |
|---|------|-----------|----------|--------------------|
| 0 | ND-100 physical source address | read | Word (W, 32-bit) | 000745 READ -> SC1; manual `<ND-100 addr/r/W>` |
| 1 | ND-500 destination buffer (logical addr) | write | Halfword (H, 16-bit) | 011275 LADDR EA2SAVE (addr only); loop WRITE TYP,HW |
| 2 | Count = number of halfwords | read | Word/count | 011276 READ -> D,LC (loop counter) |

The `H` instruction prefix selects halfword (16-bit) transfer units (loop cells use
`TYP,HW`).

### Result / side-effects

- `count` halfwords are copied from ND-100 (I/O processor) memory into the ND-500
  buffer, one halfword per loop pass (source and destination pointers both advance
  by one halfword each pass; LC counts down to 0).
- Registers used internally: SC1 (source addr / transfer temp), SC2 (privilege mask
  scratch), EA1 (source pointer), EA2 (destination pointer), LC (count), DAC.DPA
  (source physical data address). These are microarchitectural scratch, not
  programmer-visible ND-500 registers.
- No programmer-visible general register is modified by the transfer itself.

### Status flags

Microcode ground truth: NO `ST,SAVA`, `ST,SAVC`, `ST,SAVF`, `ST,LOAD`, `K,ONE`,
`K,ZRO`, or `K,1IFZ` field appears in any RIOM microcell (000745-011306). No status
is written. Manual 16.23 states verbatim: "Data status bits: Unaffected." The flag
summary table (manual line ~15046) row RIOM shows no data-status writes.

| Flag | Effect | Basis |
|------|--------|-------|
| K | UNCHANGED | no K,* field in routine; manual "unaffected" |
| Z | UNCHANGED | no ST,SAV* in routine; manual "unaffected" |
| C | UNCHANGED | no ST,SAV* in routine; manual "unaffected" |
| O | UNCHANGED | no ST,SAV* in routine; manual "unaffected" |
| S | UNCHANGED | no ST,SAV* in routine; manual "unaffected" |

Manual rule 4040 (unmentioned data-status bits are CLEARED) does NOT apply here:
the manual explicitly declares all data status bits Unaffected, and the microcode
confirms by never issuing a status-save, so every bit is preserved, not cleared.

### Trap conditions

- IIC (Illegal Instruction Code): raised by the microcode privilege test at
  011272-011273 (`MIC.STS AND mask == 0 -> ILLEG` at octal 000200). RIOM is a
  privileged instruction; executing it without the required privilege bit traps.
  (Manual: "Privileged instruction." / trap "Illegal instruction code (IIC)".)
- Addressing traps: raised by the address-translation / memory path
  (`LADDR` at 011275, `RD,POF` at 011304, `WRITE` at 011306) on page fault,
  protection violation, or invalid address of source or destination.
  (Manual: "Addressing traps".)
- IOV (Illegal Operand Value): documented by manual as a trap condition. The exact
  microcell that raises IOV is NOT isolated in this trace; it is most plausibly
  raised during operand fetch / address arithmetic for an invalid count or address,
  but that specific check is UNKNOWN from the traced cells (see Unresolved).

### Microcode vs manual vs C implementation

- Microcode and manual AGREE: privileged (IIC on privilege failure), DMA halfword
  copy from I/O processor memory to an ND-500 buffer, all data status bits
  Unaffected.
- DISAGREEMENT with the C emulator source
  `src/cpu/instructions/IO/Riom.c`:
  1. Riom.c SETS Z if count==0 and CLEARS Z otherwise (lines ~258-262). This
     contradicts BOTH the microcode (no ST,SAV*) and the manual ("Data status bits:
     Unaffected"). Per ground truth, RIOM must leave Z (and all flags) UNCHANGED.
  2. Riom.c does NOT perform the privilege (IIC) check - it has a TODO for PIA and
     "for now, we allow the instruction to execute" (lines ~166-169). The microcode
     performs a mandatory MIC.STS privilege test and traps to ILLEG when unset.
  3. Riom.c range-checks count to 16 bits and ND-100 address to 22 bits and raises
     `trap_illegal_operand`; the specific bit-widths and IOV trigger are not
     confirmed by the traced microcode (UNKNOWN origin) - treat as inferred.

### Citations

- Microcode: label RIOM at octal 000745 / 000746; RIOM_0 011271; RIOM_1 011274;
  RIOM_2 011302; RIOM_3 011304-011306; ILLEG 000200; GET_NEXT 003231.
  File: `$ND5000UC/microcode/MICRO-5800-A30.md` (lines 499-500,
  4806-4820, 142, 1703).
- Field decode: `$ND5000UC/manual/mnemonics.md`
  (STATUS/ST,SAVA table ~line 646-665; MEMORY READ/WRITE/RD,POF ~885-901;
  LC_DECR ~683; COND,LCZ ~784; EA_SAVE ~903-912; TYP,HW ~234).
- Manual: ND-500 Reference Manual section 16.23 "Read I/O processor memory",
  `docs/ND-05.009.4 EN ND-500 Reference Manual.md`
  (lines 10826-10852; flag table line ~15046).
