# ND-500 CPU Instruction Category: IO - Behavior Reference

Authoritative behavior reference for the ND-500 CPU instruction category **IO**,
built to validate the nd500x emulator. Every statement below is taken directly
from one of the two ground-truth sources named. Anything not confirmed by either
source is labelled **UNKNOWN (needs verification)**.

## Sources

- PRIMARY spec (architectural behavior):
  `/home/ronny/repos/nd500x/docs/ND-05.009.4 EN ND-500 Reference Manual.md`
- GROUND-TRUTH flag micro-behavior (ND-5000 microcode):
  `/mnt/e/Dev/Ronny/ND5000UC/microcode/MICRO-5800-A30.md`
- Micro-op field decode:
  `/mnt/e/Dev/Ronny/ND5000UC/manual/mnemonics.md`

## Category membership

Source of truth for the instruction set of this category is the emulator source
directory `/home/ronny/repos/nd500x/src/cpu/instructions/IO/`.

```
/home/ronny/repos/nd500x/src/cpu/instructions/IO/Riom.c
```

The IO category contains exactly ONE instruction:

| File    | Mnemonic |
|---------|----------|
| Riom.c  | RIOM     |

---

## Microcode flag semantics (how flags are decoded from the microcode)

Per `/mnt/e/Dev/Ronny/ND5000UC/manual/mnemonics.md`, the architectural status /
condition flags are written ONLY by the STATUS field (bits 75-72). Its relevant
values are:

| Value | Mnemonic  | Effect (from mnemonics.md line) |
|-------|-----------|---------------------------------|
| 0     | (hold)    | Hold status unchanged (mnemonics.md:652) |
| 1     | K,ONE     | Set K (flag) to 1 (mnemonics.md:653) |
| 2     | K,ZRO     | Clear K (flag) to 0 (mnemonics.md:654) |
| 3     | K,1IFZ    | Set K to 1 if ALU result is 0 (mnemonics.md:655) |
| 4     | ST,SAVA   | Save status from ALU operation - updates Z/C/O/S (mnemonics.md:656) |
| 5-14  | ST,SAVC / ST,SAVF / ST,SAVB / ST,LOAD / ST,SAVM / ST,ACC* | other status-save forms (mnemonics.md:657-664) |

IMPORTANT decode note: `COND,MSEXO`, `COND,MZRO`, `COND,LCZ`, `INVSEQ` are
microsequencer TEST conditions for the `T,JMP` micro-branch (mnemonics.md:766,
771, 784, 749). They select which micro-address executes next; they do NOT write
the architectural status register. Only a STATUS-field value of 1..14 (K,* or
ST,*) writes an architectural flag. A micro-op that carries no K,*/ST,* mnemonic
holds all flags unchanged.

---

## RIOM - Read I/O processor memory

### Opcode

| Form | Hex code | Octal code | Decimal |
|------|----------|------------|---------|
| RIOM | 0xFE76   | 0177166    | 65142   |

Source: ND-500 Reference Manual section 16.23 (manual line 10832) and the
instruction code table (manual line 14359). Confirmed against the emulator
header `/home/ronny/repos/nd500x/src/cpu/instructions/IO/Riom.c:11`.

### Assembly format

    H RIOM <ND-100 addr/r/W>, <buffer/w/H>, <no of halfwords>

Source: manual line 10828 (section 16.23). The leading `H` is the halfword
data-type prefix (manual page-382 table lists it as `Hn RIOM`, manual line 13534).

### One-line operation

I/O processor (ND-100) memory -> ND-500 memory. Copies halfwords from ND-100
physical memory into an ND-500 logical buffer via DMA. Privileged instruction.
Source: manual section 16.23 "Operation: I/O processor memory -> ND-500 memory"
(manual line ~10840) and "Description" (manual lines 10842-10846).

### Operands

Per manual section 16.23 format line (manual line 10828):

| # | Role                 | Access | Data type | Notes |
|---|----------------------|--------|-----------|-------|
| 1 | `<ND-100 addr>`      | read   | W (word)  | Physical ND-100 address, usually private ND-100 memory not directly addressable by the ND-500 (manual line 10844). |
| 2 | `<buffer>`           | write  | H (halfword) | Logical ND-500 address (manual line 10844). |
| 3 | `<no of halfwords>`  | read   | (count)   | Number of halfwords to transfer (manual example: 1024). |

Addressing-mode legend for RIOM, manual line 15046
(`| RIOM | :A | : | :A | :A A A:A | : | * |`, columns BI BY H W F D): read as
the per-datatype operand addressing-mode matrix from the manual's addressing
table. Exact per-column expansion is UNKNOWN (needs verification) without the
legend key for that table.

### Microcode entry points

Category-decode entry (dispatch): microcode octal `000745` label **RIOM**
(MICRO-5800-A30.md:499), falling through `000746` to label **RIOM_0** at octal
`011271` (MICRO-5800-A30.md:500, 4807). Main body:

| Octal  | Label   | Role (from micro-op) |
|--------|---------|----------------------|
| 000745 | RIOM    | operand fetch, READ, ADACT (MICRO-5800-A30.md:499) |
| 000746 |         | -> RIOM_0 (MICRO-5800-A30.md:500) |
| 011270 |         | G,OOPS -> RIOM_0 (MICRO-5800-A30.md:4806) |
| 011271 | RIOM_0  | (MICRO-5800-A30.md:4807) |
| 011272 |         | ALU,AND A,MIC,STS B,SC2 - AND the MIC status/mode bits (MICRO-5800-A30.md:4808) |
| 011273 |         | C,SEQ T,JMP COND,MZRO -> ILLEG : privilege/mode trap if result zero (MICRO-5800-A30.md:4809) |
| 011274 | RIOM_1  | G,OPS (MICRO-5800-A30.md:4810) |
| 011275 |         | LADDR EA2SAVE ADACT - form destination (buffer) address (MICRO-5800-A30.md:4811) |
| 011276 |         | D,LC READ ADACT - load loop counter, read source (MICRO-5800-A30.md:4812) |
| 011277 |         | DAC,DPA -> DUMMY (MICRO-5800-A30.md:4813) |
| 011300 |         | EA2SAVE (MICRO-5800-A30.md:4814) |
| 011301 |         | EA1SAVE -> RIOM_2 (MICRO-5800-A30.md:4815) |
| 011302 | RIOM_2  | LCDECR C,SEQ T,JMP INVSEQ COND,LCZ - decrement loop counter, test loop-counter-zero (MICRO-5800-A30.md:4816) |
| 011303 |         | -> GET_NEXT : loop exit (MICRO-5800-A30.md:4817) |
| 011304 | RIOM_3  | ALU,A TYP,HW ... RD,POF - physical read of one halfword (MICRO-5800-A30.md:4818) |
| 011305 |         | TYP,HW EA2SAVE (MICRO-5800-A30.md:4819) |
| 011306 |         | ALU,A TYP,HW ... WRITE -> RIOM_2 : write halfword to ND-500, loop back (MICRO-5800-A30.md:4820) |

### STATUS FLAGS

Manual section 16.23 states explicitly: **"Data status bits: Unaffected"**
(manual line ~10847, the line immediately preceding the RIOM example).

Microcode confirmation: across ALL RIOM micro-ops listed above (octal 000745,
000746, 011270-011306), NONE carries a STATUS-field flag-write mnemonic - there
is no `ST,SAVA`, `ST,SAVC`, `ST,SAVF`, `K,ONE`, `K,ZRO`, or `K,1IFZ` anywhere in
the RIOM microcode. The `COND,MZRO` / `COND,LCZ` / `COND,MSEXO` tokens present
are microsequencer branch conditions (see decode note above), not flag writes.
Therefore the microcode holds every architectural flag unchanged.

| Flag | Effect     | Manual citation | Microcode citation |
|------|------------|-----------------|--------------------|
| K    | UNCHANGED  | "Data status bits: Unaffected" (manual sec 16.23) | STATUS field = (hold) on every RIOM micro-op; no K,* mnemonic (MICRO-5800-A30.md:499-500, 4806-4820) |
| Z    | UNCHANGED  | "Data status bits: Unaffected" (manual sec 16.23) | no ST,SAV*/K,1IFZ writing Z (MICRO-5800-A30.md:499-500, 4806-4820) |
| C (carry)    | UNCHANGED | "Data status bits: Unaffected" (manual sec 16.23) | no ST,SAVA (MICRO-5800-A30.md:499-500, 4806-4820) |
| O (overflow) | UNCHANGED | "Data status bits: Unaffected" (manual sec 16.23) | no ST,SAVA (MICRO-5800-A30.md:499-500, 4806-4820) |
| S (sign)     | UNCHANGED | "Data status bits: Unaffected" (manual sec 16.23) | no ST,SAVA (MICRO-5800-A30.md:499-500, 4806-4820) |

No other architectural flag is written by RIOM in either source.

### TRAP conditions

Manual section 16.23 (manual line ~10847): **"Trap conditions: Addressing
traps, Illegal instruction code (IIC), Illegal operand value (IOV)"**.

Microcode corroboration:

- Illegal instruction code (IIC) / privilege: octal `011272` ANDs the MIC status
  bits (`A,MIC,STS`), and `011273` does `T,JMP COND,MZRO -> ILLEG`, i.e. if the
  mode/privilege test yields zero it branches to the ILLEG trap handler
  (MICRO-5800-A30.md:4808-4809). This is the privileged-instruction check.
- Addressing traps: the transfer uses `LADDR` (ladder / address request),
  `ADACT` (address arithmetic activate), `RD,POF` (physical read with MMS) and
  `WRITE` micro-ops (octal 011275, 011276, 011304, 011306;
  MICRO-5800-A30.md:4811-4812, 4818, 4820); these memory-access requests are the
  point at which addressing/protection traps are raised.
- Illegal operand value (IOV): stated by the manual. The exact micro-address
  that raises IOV for RIOM is UNKNOWN (needs verification) - not isolated in the
  RIOM micro-op block examined.

---

## Emulator discrepancy note (for validation)

The manual and the microcode BOTH say RIOM leaves all status flags unchanged
("Data status bits: Unaffected"; no STATUS-field write in any RIOM micro-op).

The current emulator implementation
`/home/ronny/repos/nd500x/src/cpu/instructions/IO/Riom.c:255-262` instead SETS
the Z flag when `count == 0` and CLEARS it otherwise:

```c
if (count == 0) {
    nd500_set_flag(cpu, ND500_FLAG_Z);
} else {
    nd500_clear_flag(cpu, ND500_FLAG_Z);
}
```

This contradicts both ground-truth sources: RIOM should NOT touch Z (or any
flag). This is flagged for verification/fix, not changed here.
