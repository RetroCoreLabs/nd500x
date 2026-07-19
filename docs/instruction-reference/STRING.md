# ND-500 STRING Instruction Category - Behavior Reference

Authoritative behavior reference for the STRING instruction category, built to
validate the nd500x emulator. Every statement below is taken directly from one of
the two cited sources. Anything not confirmed by either source is marked
`UNKNOWN (needs verification)`.

## Sources

- PRIMARY spec (operation, operands, opcodes, terminating conditions, traps):
  `/home/ronny/repos/nd500x/docs/ND-05.009.4 EN ND-500 Reference Manual.md`
  (Chapter 14 STRING INSTRUCTIONS, pages 243-264; SCPUNO in Chapter 16.36, page 333).
- GROUND-TRUTH flag micro-behavior (which flags the hardware writes, and how):
  `/mnt/e/Dev/Ronny/ND5000UC/microcode/MICRO-5800-A30.md`
- Micro-op field decode:
  `/mnt/e/Dev/Ronny/ND5000UC/manual/mnemonics.md`

## Instruction set (from `/home/ronny/repos/nd500x/src/cpu/instructions/STRING/*.c`)

SCHPAR, SCOPA, SCOPT, SCOTR, SCPUNO, SFILL, SFILLN, SMATCH, SMOVE, SMOVN,
SMVTR, SMVTU, SMVUN, SMVWH, SSCAN, SSKIP, SSPAN, SSPAR (18 files).

Note: the sibling instructions `SCOMP` (14.10) and `SLOCA` (14.15) exist in the
manual and microcode but have no file in the STRING folder, so they are outside
this set and are not documented here.

---

## Global rules for the STRING category

These come from the manual's chapter introduction, section 14.1
("DATA STATUS BITS" and "TERMINATION CONDITIONS", manual page 243-244) and apply
to EVERY string instruction unless a specific instruction states otherwise:

- **Carry (C): CLEARED.** Manual p243: "Carry and overflow are always cleared."
- **Overflow (O): CLEARED.** Same sentence, manual p243.
- **Data status bits Z and S not mentioned in an instruction's terminating-condition
  list are 0 after execution.** Manual p243: "The data status bits not mentioned in
  the string instruction description are all zero after the execution of the
  instruction."
- **K reflects the termination condition; the previous K is lost.** Manual p243.
  Destination full (I2 incremented past the last destination element) => `1 -> K`.
  Any other termination => `0 -> K` (manual p244).
- If a numeric argument is addressed via a descriptor, that descriptor addressing
  does not affect K (manual p243).
- **Addressing outside a string** (pointer already outside at start, or a
  zero-length string) causes a **descriptor-range (DR) trap condition**; no
  elements are processed and I1/I2 are left unmodified (manual p244, "ADDRESSING
  OUTSIDE STRINGS"). Outside `<source>` (or both) terminates with K=0; outside
  `<dest>` but inside `<source>` terminates with K=1.
- I1 indexes the source string, I2 the destination (or second source) string; the
  index is the element number starting at 0 and is not initialized by the
  instruction (manual p243).

### Microcode flag micro-op decode (from `/mnt/e/Dev/Ronny/ND5000UC/manual/mnemonics.md`, lines 653-656)

| Field    | Value | Meaning                                            |
|----------|-------|----------------------------------------------------|
| K,ONE    | 1     | set K: `1 -> K`                                    |
| K,ZRO    | 2     | clear K: `0 -> K`                                  |
| K,1IFZ   | 3     | `K := 1 if ALU operation result is 0`             |
| ST,SAVA  | 4     | save status from ALU operation into Z, C, O, S     |
| ST,SAVC  | 5     | save status from ALU in compare                    |

Note: the field `K,1IFFZ` mentioned in the task request does not exist in
mnemonics.md; only `K,1IFZ` (value 3) is defined. `COND,M...` fields (MSEXO, MCRY,
MCNZ, MZRO, MSGN, MSORZ) are microcode branch conditions, not flag writes.

**How to read the microcode citations below:** each STRING instruction has a
two-word dispatch entry (in the octal 0012xx region) that jumps to a byte/type
continuation routine (in the octal 005xxx-007xxx region). The K flag is written
explicitly by K,ONE / K,ZRO / K,1IFZ micro-ops; Z (and S/C/O) are written by
ST,SAVA in the compare/test steps. The exact terminal Z/S VALUE for a given
termination path is defined by the manual's terminating-condition table (primary
source); the microcode confirms WHICH flags the hardware physically writes and at
which micro-address. Full per-path Z/S value derivation from the raw microcode
would require a complete micro-trace, which was not performed; where a bit's value
is taken from the manual and only its "is written" status is confirmed by
microcode, that is stated explicitly.

---

## 14.2 SMOVE - String move

- **Opcodes (octal / hex):** BI 176546B/0FD66H, BY 176547B/0FD67H, H 176550B/0FD68H,
  W 176551B/0FD69H, F 176552B/0FD6AH, D 176553B/0FD6BH (manual 14.2).
- **Operation:** `while not end of strings do S(I1) -> D(I2), I1+1 -> I1, I2+1 -> I2 enddo`
  Move elements from `<source>` to `<dest>` until source empty or dest full.
  Overlap is handled by the microcode.
- **Operands:** `<source/r/t/I1>`, `<dest/w/t/I2>` (both implicit string descriptors).
- **Terminating conditions (manual 14.2):**
  - outside source: K=0, I1/I2 unmodified, DR trap
  - outside dest:   K=1, I1/I2 unmodified, DR trap
  - source empty:   K=0, I1/I2 :- next element
  - dest full:      K=1, I1/I2 :- next element

| Flag | Effect | Source |
|------|--------|--------|
| K | CONDITIONAL: dest full => 1; else (source empty / outside source) => 0 | Manual 14.2 table; microcode K,ZRO at 005376 (SMOVEBY_CHK_DR), K,1IFZ at 005400 (SMOVEBY_NSDR), K,ONE at 005402 (SMOVEBY_NDR), K,ZRO at 005403 |
| Z | CLEARED (Z not listed in 14.2 terminating table => 0 per p243 rule) | Manual p243; microcode ST,SAVA at 005276 (SMOVEBI_F04) physically writes Z but its terminal survival was not micro-traced |
| C | CLEARED | Manual p243 |
| O | CLEARED | Manual p243 |
| S | CLEARED (not mentioned => 0) | Manual p243 |

- **Traps:** descriptor-range trap on outside-string; addressing traps from
  descriptor or descriptor address field (manual p243).
- **Microcode:** dispatch SMOVEBI 001235->001236, SMOVEBY 001237->001240,
  SMOVEHW 001241->001242, SMOVEW 001243->001244, SMOVED 001245->001246;
  byte continuation SMOVEBY_CONT 005366.
- **Emulator note:** `/home/ronny/repos/nd500x/src/cpu/instructions/STRING/Smove.c`
  sets Z=1 on "source empty". The manual's 14.2 table lists only K, so per the
  p243 rule Z should be 0. Divergence to verify.

---

## 14.3 SMVWH - String move while

- **Opcode:** BY 176562B / 0FD72H (manual 14.3).
- **Operation:** `while not end of strings and S(I1) AND <mask> = <test> do S(I1) -> D(I2), I1+1 -> I1, I2+1 -> I2 enddo`.
  Overlap is NOT handled.
- **Operands:** `<source/r/BY/I1>`, `<dest/w/BY/I2>`, `<mask/r/BY>`, `<test/r/BY>`.
- **Terminating conditions (manual 14.3; the printed table is row-shifted - decoded reading):**
  - outside source: K=0 Z=0, I1/I2 unmodified, DR trap
  - outside dest:   K=1 Z=0, I1/I2 unmodified, DR trap
  - different bytes: K=0 Z=0, I1/I2 :- differing bytes
  - source empty:   K=0 Z=1, I1/I2 :- next element
  - dest full:      K=1 Z=1, I1/I2 :- next element

| Flag | Effect | Source |
|------|--------|--------|
| K | CONDITIONAL: dest full/outside dest => 1; else => 0 | Manual 14.3; microcode K,1IFZ at 006200 (SMVWHBY_F02), K,ZRO at 006205, K,ONE at 006206 (SMVWHBY_F05) |
| Z | CONDITIONAL: source empty / dest full => 1; different bytes / outside => 0 | Manual 14.3; microcode ST,SAVA at 006204 (SMVWHBY_F04) |
| C | CLEARED | Manual p243 |
| O | CLEARED | Manual p243 |
| S | CLEARED (not mentioned) | Manual p243 |

- **Traps:** DR trap on outside-string.
- **Microcode:** dispatch SMVWH 001247->001250.

---

## 14.4 SMVUN - String move until

- **Opcode:** BY 176563B / 0FD73H (manual 14.4).
- **Operation:** `while not end of strings and S(I1) AND <mask> != <test> do S(I1) -> D(I2), I1+1 -> I1, I2+1 -> I2 enddo`.
  The byte satisfying the until-condition is NOT moved. Overlap NOT handled.
- **Operands:** `<source/r/BY/I1>`, `<dest/w/BY/I2>`, `<mask/r/BY>`, `<test/r/BY>`.
- **Terminating conditions (manual 14.4):**
  - outside source: K=0 Z=0, I1/I2 unmodified, DR trap
  - outside dest:   K=1 Z=0, I1/I2 unmodified, DR trap
  - byte found:     K=0 Z=1, I1/I2 :- found byte in source
  - source empty:   K=0 Z=0, I1/I2 :- next element
  - dest full:      K=1 Z=0, I1/I2 :- next element

| Flag | Effect | Source |
|------|--------|--------|
| K | CONDITIONAL: dest full/outside dest => 1; else => 0 | Manual 14.4; microcode K,1IFZ at 006233 (SMVUNBY_F02), K,ZRO at 006240, K,ONE at 006241 (SMVUNBY_F05) |
| Z | CONDITIONAL: byte found => 1; source empty/dest full/outside => 0 | Manual 14.4; microcode ST,SAVA at 006237 (SMVUNBY_F04) |
| C | CLEARED | Manual p243 |
| O | CLEARED | Manual p243 |
| S | CLEARED (not mentioned) | Manual p243 |

- **Traps:** DR trap on outside-string.
- **Microcode:** dispatch SMVUN 001251->001252.

---

## 14.5 SMVTR - String move translated

- **Opcode:** BY 176564B / 0FD74H (manual 14.5).
- **Operation:** `while not end of strings do tr(S(I1)) -> D(I2), I1+1 -> I1, I2+1 -> I2 enddo`.
  Bytes translated via a 256-byte translation table found at `<trans table>`
  (addressed directly, not via implicit descriptor). Overlap is handled.
- **Operands:** `<source/r/BY/I1>`, `<dest/w/BY/I2>`, `<trans table/aa/BY>`.
- **Terminating conditions (manual 14.5):**
  - outside source: K=0, I1/I2 unmodified, DR trap
  - outside dest:   K=1, I1/I2 unmodified, DR trap
  - source empty:   K=0, I1/I2 :- next element
  - dest full:      K=1, I1/I2 :- next element

| Flag | Effect | Source |
|------|--------|--------|
| K | CONDITIONAL: dest full => 1; else => 0 | Manual 14.5; microcode K,1IFZ at 006273 (SMVTRBY_F03), K,ZRO at 006300, K,ONE at 006301 (SMVTRBY_F06) |
| Z | CLEARED (not listed => 0) | Manual p243; microcode ST,SAVA at 006277 (SMVTRBY_F05) writes Z, terminal survival not micro-traced |
| C | CLEARED | Manual p243 |
| O | CLEARED | Manual p243 |
| S | CLEARED (not mentioned) | Manual p243 |

- **Traps:** DR trap on outside-string; addressing traps from translation-table
  reference. If the translation table is addressed via an explicit descriptor
  operand, the index register is not incremented (manual p243, CHARACTER TRANSLATION).
- **Microcode:** dispatch SMVTR 001253->001254.

---

## 14.6 SMVTU - String move translated until

- **Opcode:** BY 176565B / 0FD75H (manual 14.6).
- **Operation:**
  ```
  while not end of strings and tr(S(I1)) != ASCII "escape" do
      if tr(S(I1)) != zero then tr(S(I1)) -> D(I2), I2+1 -> I2 endif
      I1+1 -> I1
  enddo
  ```
  ASCII "escape" = 01BH (033B). Translated NUL bytes are not moved. The escape
  character is not moved. Overlap NOT handled.
- **Operands:** `<source/r/BY/I1>`, `<dest/w/BY/I2>`, `<trans table/aa/BY>`.
- **Terminating conditions (manual 14.6):**
  - outside source: K=0 Z=0, I1/I2 unmodified, DR trap
  - outside dest:   K=1 Z=0, I1/I2 unmodified, DR trap
  - "escape" found: K=0 Z=1, I1/I2 :- position of escape
  - source empty:   K=0 Z=0, I1/I2 :- next element
  - dest full:      K=1 Z=0, I1/I2 :- next element

| Flag | Effect | Source |
|------|--------|--------|
| K | CONDITIONAL: dest full/outside dest => 1; else => 0 | Manual 14.6; microcode K,1IFZ at 006355 (SMVTUBY_F03) |
| Z | CONDITIONAL: escape found => 1; source empty/dest full/outside => 0 | Manual 14.6; microcode ST,SAVA at 006362 |
| C | CLEARED | Manual p243 |
| O | CLEARED | Manual p243 |
| S | CLEARED (not mentioned) | Manual p243 |

- **Traps:** DR trap on outside-string; translation-table addressing traps.
- **Microcode:** dispatch SMVTU 001255->001256.

---

## 14.7 SMOVN - String move n elements

- **Opcodes:** BI 176566B/0FD76H (manual prints "176568B", a typo - 8 is not an
  octal digit; the sequence is 176566B), BY 176567B/0FD77H, H 176570B/0FD78H,
  W 176571B/0FD79H, F 176572B/0FD7AH, D 176573B/0FD7BH (manual 14.7).
- **Operation:** `0->i; while not end of strings and i<m do S(I1)->D(I2), I1+1->I1, I2+1->I2, i+1->i enddo`.
  Move exactly m items unless source empty or dest full first. Overlap handled.
- **Operands:** `<source/r/t/I1>`, `<dest/w/t/I2>`, `<m/r/W>` (count is a word).
- **Terminating conditions (manual 14.7):**
  - outside source: K=0 Z=0, I1/I2 unmodified, DR trap
  - outside dest:   K=1 Z=0, I1/I2 unmodified, DR trap
  - m items moved:  K=0 Z=1, I1/I2 :- next element
  - source empty:   K=0 Z=0, I1/I2 :- next element
  - dest full:      K=1 Z=0, I1/I2 :- next element

| Flag | Effect | Source |
|------|--------|--------|
| K | CONDITIONAL: dest full/outside dest => 1; else => 0 | Manual 14.7; microcode K,1IFZ at 006442 (SMOVNBY_F02), K,ZRO at 006447/006455, K,ONE at 006450 (SMOVNBY_F05) |
| Z | CONDITIONAL: m items moved => 1; source empty/dest full/outside => 0 | Manual 14.7; microcode ST,SAVA at 006446 (SMOVNBY_F04) and 006454 (SMOVNBY_F67) |
| C | CLEARED | Manual p243 |
| O | CLEARED | Manual p243 |
| S | CLEARED (not mentioned) | Manual p243 |

- **Traps:** DR trap on outside-string. If m (a numeric argument) is addressed via a
  descriptor, that does not affect K (manual p243).
- **Microcode:** dispatch SMOVNBI 001257->001260, SMOVNBY 001261->001262,
  SMOVNH 001263->001264, SMOVNW 001265->001266, SMOVND 001267->001270.
- **Emulator note:** `Smovn.c` moves via `nd500_read_memory_8` / `write_memory_8`
  (byte-sized) for all data types; the BI/H/W/F/D variants are not size-correct.

---

## 14.8 SFILL - String fill

- **Opcodes:** BIn 176574B+(n-1)/0FD7C+(n-1), BYn 176600B+(n-1)/0FD80+(n-1),
  Hn 176604B+(n-1)/0FD84+(n-1), Wn 176610B+(n-1)/0FD88+(n-1),
  Fn 176614B+(n-1)/0FD8C+(n-1), Dn 176620B+(n-1)/0FD90+(n-1) (manual 14.8).
- **Operation:** `while not end of string do tn -> D(I2), I2+1 -> I2 enddo`.
  Fill every element of `<dest>` from I2 to the end with register tn.
- **Operands:** `<dest/w/t/I2>` (fill value is the register tn selected by n).
- **Terminating conditions (manual 14.8):**
  - outside dest:   K=1, I2 unmodified, DR trap
  - string filled:  K=1, I2 :- next element

| Flag | Effect | Source |
|------|--------|--------|
| K | SET (=1) in both terminating cases | Manual 14.8; microcode K,ONE at 005743 (SFILL_BY00) / 005723 (SFILLBI), K,1IFZ at 005722 (SFILLBI_F) |
| Z | CLEARED (not listed => 0) | Manual p243; microcode ST,SAVA at 005725 (SFILLBI_F01) writes Z, terminal survival not micro-traced |
| C | CLEARED | Manual p243 |
| O | CLEARED | Manual p243 |
| S | CLEARED (not mentioned) | Manual p243 |

- **Traps:** DR trap on outside-dest (including zero-length string).
- **Microcode:** dispatch SFILLBI 001271->001272, SFILLBY 001273->001274,
  SFILLH 001275->001276, SFILLW 001277->001300, SFILLD 001301->001302.

---

## 14.9 SFILLN - String fill n elements

- **Opcodes:** BIn 176624B+(n-1)/0FD94H+(n-1), BYn 176630B+(n-1)/0FD98H+(n-1),
  Hn 176634B+(n-1)/0FD9CH+(n-1), Wn 176640B+(n-1)/0FDA0H+(n-1),
  Fn 176644B+(n-1)/0FDA4H+(n-1), Dn 176650B+(n-1)/0FDA8H+(n-1) (manual 14.9).
- **Operation:** `0->i; while not end of string and i<m do tn->D(I2), I2+1->I2, i+1->i enddo`.
  Fill m elements (or fewer if the end is reached first). m is unsigned.
- **Operands:** `<dest/w/t/I2>`, `<m/r/W>`.
- **Terminating conditions (manual 14.9):**
  - outside dest:     K=1 Z=0, I2 unmodified, DR trap
  - m elements filled: K=0 Z=1, I2 :- next element
  - dest full:        K=1 Z=0, I2 :- next element

| Flag | Effect | Source |
|------|--------|--------|
| K | CONDITIONAL: dest full/outside dest => 1; m elements filled => 0 | Manual 14.9; microcode K,1IFZ at 006106 (SFILNBY_F00), K,ONE at 006107 |
| Z | CONDITIONAL: m elements filled => 1; dest full/outside => 0 | Manual 14.9; microcode ST,SAVA at 006111 (SFILNBY_F01) |
| C | CLEARED | Manual p243 |
| O | CLEARED | Manual p243 |
| S | CLEARED (not mentioned) | Manual p243 |

- **Traps:** DR trap on outside-dest. Descriptor addressing of m does not affect K
  (manual p243).
- **Microcode:** dispatch SFILLNBI 001303->001304 (->SFILN_BI0),
  SFILLNBY 001305->001306 (->SFILN_BY0), SFILLNH 001307->001310,
  SFILLNW 001311->001312, SFILLND 001313->001314.
- **Emulator note:** `Sfilln.c` sets Z=0 unconditionally and never sets Z=1 on the
  "m elements filled" termination, and fills byte-sized regardless of type.
  Divergence from manual 14.9. Verify.

---

## 14.11 SCOTR - String compare translated

- **Opcode:** BY 176655B / 0FDADH (manual 14.11 prints "0FAD", a truncation;
  octal 176655B corresponds to 0FDADH).
- **Operation:** `while not end of strings and tr(S(I1)) = tr(D(I2)) do I1+1->I1, I2+1->I2 enddo`.
  Bytes compared as unsigned after translation via `<trans table>`.
- **Operands:** `<source-1/r/BY/I1>`, `<source-2/r/BY/I2>`, `<trans table/aa/BY>`.
- **Terminating conditions (manual 14.11):**
  - both operands outside string: K=0 Z=1 S=0, I1/I2 unmodified, DR trap
  - exact match:                  K=0 Z=1 S=0, I1/I2 :- next element
  - source-1 longer:              K=0 Z=0 S=0, I1/I2 :- next element
  - source-2 longer:              K=0 Z=0 S=1, I1/I2 :- next element
  - greater byte in source-1:     K=1 Z=0 S=0, I1/I2 :- differing elements
  - smaller byte in source-1:     K=1 Z=0 S=1, I1/I2 :- differing elements

| Flag | Effect | Source |
|------|--------|--------|
| K | CONDITIONAL: unequal bytes found (byte greater/smaller in source-1) => 1; match / length-difference / outside => 0 | Manual 14.11; microcode K,1IFZ at 006623 (SCOTRBY_F03), K,ZRO at 006624 |
| Z | CONDITIONAL: exact match / both-outside => 1; else => 0 | Manual 14.11; microcode ST,SAVA at 006627 (SCOTRBY_F04), 006635 (F07), 006637 (F08) |
| S | CONDITIONAL: source-1 smaller/shorter => 1; source-1 greater/longer / match => 0 | Manual 14.11; microcode ST,SAVA saves S (006627/006635/006637) |
| C | CLEARED | Manual p243 |
| O | CLEARED | Manual p243 |

- **Traps:** DR trap when addressed outside string; translation-table addressing traps.
- **Microcode:** dispatch SCOTR 001317->001320 (->SCOTRBY_F01).

---

## 14.12 SCOPA - String compare with pad

- **Opcode:** BY 176676B / 0FDBEH (manual 14.12 prints "OFDBEI", an OCR error;
  octal 176676B = 0FDBEH).
- **Operation:** `while not end of strings and S(I1) = D(I2) do I1+1->I1, I2+1->I2 enddo`.
  The shorter string is logically extended with `<pad>` bytes so both have equal
  length; the pad byte is a scalar value (the third operand), not a descriptor.
  The index registers are not incremented while padding a string. Bytes unsigned.
- **Operands:** `<source-1/r/BY/I1>`, `<source-2/r/BY/I2>`, `<pad/r/BY>`.
- **Terminating conditions (manual 14.12):**
  - exact match (incl. pad):  K=0 Z=1 S=0, I1/I2 :- next element
  - greater byte in source-1: K=1 Z=0 S=0, I1/I2 :- differing elements
  - smaller byte in source-1: K=1 Z=0 S=1, I1/I2 :- differing elements
  - both operands outside strings compare as exact match, pointers unmodified, DR trap.

| Flag | Effect | Source |
|------|--------|--------|
| K | CONDITIONAL: unequal byte found => 1; exact match => 0 | Manual 14.12; microcode K,1IFZ at 006670, K,ONE at 007017 (SCOPABY_UNEQ), K,ZRO at 006701/006766/006776/007010 |
| Z | CONDITIONAL: exact match => 1; unequal => 0 | Manual 14.12; microcode ST,SAVA at 006765, 007020, 007021 |
| S | CONDITIONAL: smaller byte in source-1 => 1; greater / match => 0 | Manual 14.12; microcode ST,SAVA (007020/007021), COND,MSGN/MSORZ used for S branch |
| C | CLEARED | Manual p243 |
| O | CLEARED | Manual p243 |

- **Traps:** DR trap when both operands addressed outside the strings.
- **Microcode:** dispatch SCOPA 001321->001322 (->SCOPABY_CONT).

---

## 14.13 SCOPT - String compare translated with pad

- **Opcode:** BY 176677B / 0FDBFH (manual 14.13).
- **Operation:** `while not end of strings and tr(S(I1)) = tr(D(I2)) do I1+1->I1, I2+1->I2 enddo`.
  Translated compare with padding: the shorter string is extended with `<pad>`
  bytes; the pad byte is also translated. "The index registers are not incremented
  when padding a string." Bytes unsigned.
- **Operands:** `<source-1/r/BY/I1>`, `<source-2/r/BY/I2>`, `<trans table/aa/BY>`, `<pad/r/BY>`.
- **Terminating conditions (manual 14.13):**
  - exact match:              K=0 Z=1 S=0, I1/I2 :- next element or end of string
  - greater byte in source-1: K=1 Z=0 S=0, I1/I2 :- differing elements
  - smaller byte in source-1: K=1 Z=0 S=1, I1/I2 :- differing elements
  - both operands outside strings compare as exact match, pointers unmodified, DR trap.

| Flag | Effect | Source |
|------|--------|--------|
| K | CONDITIONAL: unequal byte found => 1; exact match => 0 | Manual 14.13; microcode K,1IFZ at 007034 (SCOPTBY_F03), K,ZRO at 007035/007037/007040/007041 |
| Z | CONDITIONAL: exact match => 1; unequal => 0 | Manual 14.13; microcode ST,SAVA at 007061, 007103, 007113 |
| S | CONDITIONAL: smaller byte in source-1 => 1; greater / match => 0 | Manual 14.13; microcode ST,SAVA (007061/007103/007113) |
| C | CLEARED | Manual p243 |
| O | CLEARED | Manual p243 |

- **Traps:** DR trap when both operands addressed outside the strings; translation
  addressing traps.
- **Microcode:** dispatch SCOPT 001323->001324 (->SCOPTBY_F01).
- **Emulator note (MAJOR):**
  `/home/ronny/repos/nd500x/src/cpu/instructions/STRING/Scopt.c` implements SCOPT as
  "string COPY translate" (copy source to dest with translation, stop on a stop
  byte). The manual defines SCOPT as "string COMPARE translated with pad" (no copy,
  no writes to a destination). The emulator's SCOPT does not match the manual at all.
  Verify against manual 14.13.

---

## 14.14 SSKIP - String skip elements

- **Opcode:** BY 176656B / 0FDAEH (manual 14.14).
- **Operation:**
  ```
  while not end of string and S(I1) = <test> do I1+1 -> I1 enddo
  if S(I1) > <test> then 0 -> S else 1 -> S endif
  ```
  Skip while equal to `<test>`; stop at first differing byte or end. Bytes unsigned.
- **Operands:** `<source/r/BY/I1>`, `<test/r/BY>`.
- **Terminating conditions (manual 14.14):**
  - outside source: K=0 Z=1 S=0, I1 unmodified, DR trap
  - byte > <test>:  K=0 Z=0 S=0, I1 :- differing element
  - byte < <test>:  K=0 Z=0 S=1, I1 :- differing element
  - source empty:   K=0 Z=1 S=0, I1 :- next element

| Flag | Effect | Source |
|------|--------|--------|
| K | CLEARED (=0) in all terminating cases (no destination string) | Manual 14.14; microcode K,ZRO at 007122, K,1IFZ at 007121 (SSKIPBY_F01) / 007126 (SSKIPBY_F03) |
| Z | CONDITIONAL: source empty / outside => 1; differing byte found => 0 | Manual 14.14; microcode ST,SAVA at 007124 (SSKIPBY_F02) |
| S | CONDITIONAL: byte < test => 1; byte > test / empty / outside => 0 | Manual 14.14 (the `if S(I1) > <test>` rule) |
| C | CLEARED | Manual p243 |
| O | CLEARED | Manual p243 |

- **Traps:** DR trap on outside-source.
- **Microcode:** dispatch SSKIP 001325->001327 (->SSKIPBY_F01).
- **Emulator note:**
  `/home/ronny/repos/nd500x/src/cpu/instructions/STRING/Sskip.c` sets Z=1 when it
  finds a DIFFERENT element and Z=0 + S=1 at end of string. The manual is the
  opposite: Z=1 on source-empty/outside, Z=0 on differing byte, with S showing
  greater(0)/smaller(1). Divergence to verify.

---

## 14.16 SSCAN - String scan

- **Opcode:** BY 176661B / 0FDB1H (manual 14.16).
- **Operation:** `while not end of string and tr(S(I1)) AND <mask> = zero do I1+1 -> I1 enddo`.
  Scan (translating each byte) until `(tr(byte) AND mask)` is nonzero or end.
- **Operands:** `<source/r/BY/I1>`, `<mask/r/BY>`, `<trans table/aa/BY>`.
- **Terminating conditions (manual 14.16):**
  - outside source:        K=0 Z=1, I1 unmodified, DR trap
  - byte AND mask > zero:   K=0 Z=0, I1 :- found element
  - source empty:          K=0 Z=1, I1 :- next element

| Flag | Effect | Source |
|------|--------|--------|
| K | CLEARED (=0) in all cases (no destination string) | Manual 14.16; microcode K,1IFZ at 007202, K,ZRO at 007204 (SSCANBY_F02) |
| Z | CONDITIONAL: source empty / outside => 1; nonzero AND found => 0 | Manual 14.16; microcode ST,SAVA at 007206 (SSCANBY_F03) and 007221 |
| C | CLEARED | Manual p243 |
| O | CLEARED | Manual p243 |
| S | CLEARED (not mentioned) | Manual p243 |

- **Traps:** DR trap on outside-source; translation-table addressing traps.
- **Microcode:** dispatch SSCAN 001336->001340 (->SSCANBY_F01).
- **Emulator note (MAJOR):**
  `/home/ronny/repos/nd500x/src/cpu/instructions/STRING/Sscan.c` implements a
  2-operand scan-for-a-test-byte with Z=1 on found and S=1 at end. The manual's
  SSCAN takes 3 operands (source, mask, trans table), scans on
  `(tr(byte) AND mask)==0`, and gives Z=0 on found / Z=1 on empty. Divergence to verify.

---

## 14.17 SSPAN - String span

- **Opcode:** BY 176662B / 0FDB2H (manual 14.17).
- **Operation:** `while not end of string and tr(S(I1)) AND <mask> != zero do I1+1 -> I1 enddo`.
  Span (translating each byte) while `(tr(byte) AND mask)` is nonzero; stop when it
  becomes zero or at end.
- **Operands:** `<source/r/BY/I1>`, `<mask/r/BY>`, `<trans table/aa/BY>`.
- **Terminating conditions (manual 14.17):**
  - outside source:            K=0 Z=0, I1 unmodified, DR trap
  - tr(byte) AND mask = zero:   K=0 Z=1, I1 :- found element
  - source empty:              K=0 Z=0, I1 :- next element

| Flag | Effect | Source |
|------|--------|--------|
| K | CLEARED (=0) in all cases (no destination string) | Manual 14.17; microcode K,1IFZ at 007224, K,ZRO at 007226 (SSPANBY_F02) |
| Z | CONDITIONAL: tr(byte) AND mask == 0 found => 1; source empty / outside => 0 | Manual 14.17; microcode ST,SAVA at 007230 (SSPANBY_F03) and 007243 |
| C | CLEARED | Manual p243 |
| O | CLEARED | Manual p243 |
| S | CLEARED (not mentioned) | Manual p243 |

- **Traps:** DR trap on outside-source; translation-table addressing traps.
- **Microcode:** dispatch SSPAN 001341->001343 (->SSPANBY_F01).
- **Emulator note (MAJOR):**
  `/home/ronny/repos/nd500x/src/cpu/instructions/STRING/Sspan.c` treats operand 2 as
  a character-SET descriptor and tests membership (no mask, no translation table).
  The manual's SSPAN uses `<mask>` and `<trans table>` with the AND-nonzero rule.
  Divergence to verify.

---

## 14.18 SMATCH - String match

- **Opcode:** BY 176663B / 0FDB3H (manual 14.18).
- **Operation:**
  ```
  while not end of <string>
    and <substring> != <string>(I2 .. I2+substring.length-1) do I2+1 -> I2 enddo
  if <substring> = <string>(I2 .. I2+substring.length-1) then 1 -> Z else 0 -> Z endif
  ```
  Search for `<substring>` within `<string>`. **I2** is the moving pointer into
  `<string>`; **I1 is left unmodified.**
- **Operands:** `<substring/r/BY/I1>`, `<string/r/BY/I2>`.
- **Terminating conditions (manual 14.18):**
  - outside substring: K=0 Z=1, I2 unmodified, DR trap
  - outside string:    K=0 Z=0, I2 unmodified, DR trap
  - substring found:   K=0 Z=1, I2 :- first matching byte
  - source empty:      K=0 Z=0, I2 :- next element

| Flag | Effect | Source |
|------|--------|--------|
| K | CLEARED (=0) in all cases | Manual 14.18; microcode K,1IFZ at 007250 (SMATCHBY_F02), K,ZRO at 007252 (F03) / 007254 (F04) |
| Z | CONDITIONAL: substring found / outside substring => 1; not found / outside string => 0 | Manual 14.18; microcode ST,SAVA at 007256 |
| C | CLEARED | Manual p243 |
| O | CLEARED | Manual p243 |
| S | CLEARED (not mentioned) | Manual p243 |

- **Traps:** DR trap when substring or string addressed outside.
- **Microcode:** dispatch SMATCH 001344->001345 (->SMATCHBY_F01).
- **Emulator note:**
  `/home/ronny/repos/nd500x/src/cpu/instructions/STRING/Smatch.c` uses I1 as the
  moving pointer (manual moves I2 and leaves I1 unmodified) and sets S=1 when not
  found (manual leaves S=0). Divergence to verify.

---

## 14.19 SSPAR - Set parity in string

- **Opcode:** BY 176664B / 0FDB4H (manual 14.19).
- **Operation:** `while not end of string do parity according to <mode> -> bit 7 of S(I1), I1+1 -> I1 enddo`.
  Set bit 7 of every byte from I1 to end. `<mode>` is a scalar value:
  0=clear parity (bit7:=0), 1=set parity (bit7:=1), 2=even parity, 3=odd parity.
- **Operands:** `<string/rw/BY/I1>`, `<mode/r/BY>`.
- **Terminating conditions (manual 14.19):** K=1.

| Flag | Effect | Source |
|------|--------|--------|
| K | SET (=1) | Manual 14.19 ("Terminating conditions: K=1"); microcode K,ONE at 007301 (SSPAR_F02 head), K,1IFZ at 007300 (SSPAR_F01) |
| Z | CLEARED (not listed => 0) | Manual p243; microcode ST,SAVA at 007307 writes Z, terminal survival not micro-traced |
| C | CLEARED | Manual p243 |
| O | CLEARED | Manual p243 |
| S | CLEARED (not mentioned) | Manual p243 |

- **Traps:** illegal-operand-value trap if `<mode>` is any value other than 0..3
  (manual 14.19). DR trap on outside-string / zero-length (global rule p244).
- **Microcode:** dispatch SSPAR 001346->001347 (->SSPAR_F01).

---

## 14.20 SCHPAR - Check parity in string

- **Opcode:** BY 176665B / 0FDB5H (manual 14.20).
- **Operation:**
  ```
  0 -> Z
  while not end of string and bit 7 of S(I1) = parity according to <mode> do I1+1 -> I1 enddo
  if bit 7 of S(I1) != parity according to <mode> then 1 -> Z endif
  ```
  `<mode>`: 0=clear, 1=set, 2=even, 3=odd parity.
- **Operands:** `<string/r/BY/I1>`, `<mode/r/BY>`.
- **Terminating conditions (manual 14.20):**
  - outside string:      K=0 Z=0, I1 unmodified, DR trap
  - string empty:        K=0 Z=0, I1 :- next element
  - parity error found:  K=0 Z=1, I1 :- element with wrong parity

| Flag | Effect | Source |
|------|--------|--------|
| K | CLEARED (=0) in all cases | Manual 14.20; microcode K,ZRO at 007354, K,1IFZ at 007353 (SCHPAR_F01) |
| Z | CONDITIONAL: parity error found => 1; string empty / outside => 0 | Manual 14.20; microcode ST,SAVA at 007362, 007365 (SCHPAR_F05), 007400/007407/007416/007425 |
| C | CLEARED | Manual p243 |
| O | CLEARED | Manual p243 |
| S | CLEARED (not mentioned) | Manual p243 |

- **Traps:** illegal-operand-value trap if `<mode>` is not 0..3 (manual 14.20).
  DR trap on outside-string.
- **Microcode:** dispatch SCHPAR 001351->001353 (->SCHPAR_F01).
- **Emulator note:**
  `/home/ronny/repos/nd500x/src/cpu/instructions/STRING/Schpar.c` sets S=1 on the
  outside-string path; the manual lists only K and Z there, so per the p243 rule S
  should be 0. Minor divergence to verify.

---

## SCPUNO - Store CPU number ('87 extension) -- NOT a string instruction

`SCPUNO` appears as a file in the STRING folder
(`/home/ronny/repos/nd500x/src/cpu/instructions/STRING/Scpuno.c`), but in the
ND-500 Reference Manual `SCPUNO` is a SPECIAL/SYSTEM instruction, not a string
instruction.

- **Manual section:** 16.36 (page 333), "SPECIAL INSTRUCTIONS".
- **Opcode:** 177774B / 0FF7CCH.
- **Format:** `SCPUNO <destination/w>`.
- **Operation:** `<CPUNO> -> <destination>`. Store CPU number in destination address.
- **Data status bits (manual 16.36, verbatim):** "Status bit set according to CPU
  number." No individual K/Z/C/O/S effect is broken out.

| Flag | Effect | Source |
|------|--------|--------|
| K | UNKNOWN (needs verification) | Manual 16.36 only says "status bit set according to CPU number"; SCPUNO microcode (octal 001047-001050) contains no K,ONE/K,ZRO/K,1IFZ micro-op |
| Z | UNKNOWN (needs verification) | as above; no ST,SAVA in SCPUNO microwords |
| C | UNKNOWN (needs verification) | not a string op, so the p243 "carry/overflow cleared" rule does not apply |
| O | UNKNOWN (needs verification) | as above |
| S | UNKNOWN (needs verification) | as above |

- **Traps:** UNKNOWN (needs verification) - none stated in manual 16.36.
- **Microcode:** SCPUNO dispatch at octal 001047 -> 001050 -> SCPUNO_1
  (`/mnt/e/Dev/Ronny/ND5000UC/microcode/MICRO-5800-A30.md` line 565-566). Neither
  microword writes K or ST status.
- **Emulator note (MAJOR):** `Scpuno.c` implements a "string copy until" instruction
  (copy source->dest, stop on a test byte, claimed opcode 0xFFFC) that does not
  exist under this mnemonic in the manual. The real SCPUNO (177774B) stores a CPU
  number. The file in the STRING folder does not correspond to the manual's SCPUNO.

---

## Summary of emulator divergences found (for validation)

These are places where the emulator source under
`/home/ronny/repos/nd500x/src/cpu/instructions/STRING/` differs from the manual and
should be checked:

1. **SCOPT** (`Scopt.c`): implemented as copy-translate; manual says compare-translated-with-pad.
2. **SSCAN** (`Sscan.c`): 2-operand test-byte scan; manual is 3-operand mask+translate with inverted Z.
3. **SSPAN** (`Sspan.c`): set-membership; manual is mask+translate AND-nonzero.
4. **SSKIP** (`Sskip.c`): Z/S sense differs from manual 14.14.
5. **SMATCH** (`Smatch.c`): uses I1 as pointer and sets S; manual moves I2, leaves I1, S=0.
6. **SCPUNO** (`Scpuno.c`): implements a nonexistent "string copy until"; real SCPUNO stores CPU number.
7. **SMOVE / SMVTR / SFILL / SSPAR**: emulator writes Z where manual lists only K
   (per p243 unmentioned Z should be 0).
8. **SMOVN / SFILLN** (`Smovn.c`, `Sfilln.c`): byte-only element size for all data
   types; SFILLN never sets Z=1 on "m elements filled".
9. **SCHPAR** (`Schpar.c`): sets S on outside-string; manual implies S=0.

## Open items / not verified

- Exact terminal VALUE of Z (and S) written by the ST,SAVA micro-ops for the
  move/fill instructions (SMOVE, SMVTR, SFILL, SSPAR) was not derived by a full
  micro-trace; those Z rows are taken from the manual's p243 "unmentioned => 0" rule
  and only the "is physically written" fact is confirmed in microcode.
- SCPUNO flag effects are undocumented at the bit level in both sources.
- Manual octal/hex typos noted inline: SMOVN BI "176568B" (should be 176566B),
  SCOTR "0FAD" (0FDADH), SCOPA "OFDBEI" (0FDBEH).
