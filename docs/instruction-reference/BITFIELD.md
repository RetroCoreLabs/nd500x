# ND-500 Instruction Category: BITFIELD - Functional Behaviour Reference

Reconstructed by tracing the ND-5000/5800 microcode, cross-checked against the
ND-500 Reference Manual. Pseudocode describes the actual microcode data path,
not just the flag summary.

Sources:
- Microcode: `/mnt/e/Dev/Ronny/ND5000UC/microcode/MICRO-5800-A30.md`
- Field decode: `/mnt/e/Dev/Ronny/ND5000UC/manual/mnemonics.md`
- Manual: `/home/ronny/repos/nd500x/docs/ND-05.009.4 EN ND-500 Reference Manual.md`
- C reference: `/home/ronny/repos/nd500x/src/cpu/instructions/BITFIELD/*.c`

Category members (from `src/cpu/instructions/BITFIELD/`): CLEBI, GETB, GETBF,
GETBI, PUTBF, PUTBI, SETBI. Note that GETB is a heap ("buddy") allocator that
lives in this category only because its opcode falls in the same block; it is
not a bit-manipulation instruction.

---

## Microcode reading key (decoded from mnemonics.md)

Field / mnemonic meanings used throughout the traces:

- `A,BMnn` / `A,BMLC` - A-bus is a single-bit constant `2^nn` (nn OCTAL), or a
  single-bit mask `2^LC` built from the Loop Counter. So `BM00`=1, `BM03`=8,
  `BM04`=16, `BM05`=32, `BM07`=128, `BM20`=2^16, `BM33`=2^27. `BMLC` = the mask
  `(1 << bit_number)`.
- `A,ALU,REG37` / `D,ALU,REG37` - the working-register-file entry selected by
  the instruction's operand specifier (via ORCON OR-logic): i.e. the operand
  register or the target register Rn.
- `LC` = Loop Counter (holds the bit number / a working count); `SCn` = scratch
  register; `Q` = shift register.
- ALU ops: `ALU,A` pass A; `ALU,FZRO` force 0; `ALU,AND` A&B; `ALU,OR` A|B;
  `ALU,XOR` A^B; `ALU,ANDCA` (~A)&B; `ALU,ANDCB` A&(~B); `ALU,A-B CRY,ONE` A-B;
  `ALU,A-1` decrement. `C,ALU ALU,x ALUF,y` = conditional ALU (true path x,
  false path y), chosen by the tested COND.
- `READ` / `WRITE C,MEMOT` - data-memory read / write of the operand (write only
  if the operand is a data operand); `EA1SAVE` saves the effective address.
- `ST,SAVA` - save Z, C, O, S into the status register from the ALU result. This
  is the mechanism that writes the data-status bits. `K,ONE/K,ZRO/K,1IFZ` (none
  appear in these bit routines) would be the only way to touch the K flag.
- `G,OOPS` / `DGET_NEXT` - fetch the next instruction (routine end).
- Trap plumbing: a range check computes `data_type_bits - bit_number` (e.g.
  `ALU,A-B A,BM03 B,LC`); an out-of-range result routes through `SET_IOV`
  (003132, raise Illegal-Operand-Value) instead of `CLE_IOV` (003131, clear the
  armed IOV and continue). `BM33`=2^27 is the STO (stack-overflow) status bit.

Manual status-bit rules applied when resolving flags:
- Rule 4040 / section 6.5.1 (manual lines 2022, 4040): "All data status bits not
  mentioned in the instruction description are always cleared." The data status
  bits are Z, C, S, O (section 6.5.1).
- The K flag (manual line 2235/2244) is a SIGNALLING flag (section 6.5.4), NOT a
  data status bit, so rule 4040 does not force it clear. None of the traced bit
  routines contain a K field, so K is UNCHANGED - except that descriptor
  addressing of the operand "may set but never clear the K flag" as an
  addressing side effect (manual section 8.15).

---

## GETBI - Get bit (extract single bit to register)

Opcodes (octal, n = target register 1..4): BYn 176264B+(n-1), Hn 176270B+(n-1),
Wn 176720B+(n-1). Format: `tn GETBI <operand/r/t>,<bit No./r/BY>`.

### Functional pseudocode (microcode trace)
```
1. SC6 <- read <operand>        ; TYP,DR (BY/H/W), READ, EA1SAVE   [GETBIBY 000414]
2. LC  <- read <bit No.>        ; TYP,BY, READ                     [000415]
3. range check: (data_type_bits - bit_number)                     [000416/000417]
      BM03=8 (BY) / BM04=16 (H) / BM05=32 (W) minus LC
      if bit_number >= data_type_bits (or negative) -> SET_IOV (IOV trap)
      else -> GET_BIT
4. GET_BIT (003335): isolated = BMLC AND SC6 = (1<<bit_number) & operand
5. GET_BIT_1 (003340): C,ALU ALU,FZRO / ALUF,A(=BM00=1)
      Rn <- (isolated != 0) ? 1 : 0     ; result to D,ALU,REG37
      ST,SAVA  -> Z,C,O,S from the 0/1 result
      G,OOPS   ; fetch next instruction
```

### Operands / datatypes
- `<operand>` r, type BY / H / W (8/16/32-bit) - source, read only.
- `<bit No.>` r, type BY (signed byte) - bit position, 0 = LSB.
- Target register Rn (I1..I4) selected by opcode; result to bit 0, upper bits 0.

### Result / side effects
Bit 0 of Rn = selected bit of operand; all higher Rn bits = 0. Operand unchanged.

### Status flags
| Flag | Effect |
|------|--------|
| Z | CONDITIONAL - set if extracted bit = 0, else cleared (`ST,SAVA` on the 0/1 result) |
| C | CLEARED (result 0/1 generates no carry; rule 4040) |
| O | CLEARED (no overflow; rule 4040) |
| S | CLEARED (result 0/1 has bit-31 = 0; rule 4040) |
| K | UNCHANGED (no K field; may be set as descriptor-addressing side effect) |

### Traps
Addressing traps; Illegal operand value (IOV) if bit_number < 0 or
bit_number >= data_type_bits.

### Citation
Microcode GETBIBY/H/W = 000414/000420/000424 -> GET_BIT 003335 -> GET_BIT_1
003340 (MICRO-5800-A30.md lines 282-293, 1771-1774). Manual section 10.27 "Get
bit" (lines 5240-5267): "transferred bit = 0 -> Z". Microcode and manual AGREE.

---

## PUTBI - Put bit (store register bit-0 into a single bit)

Opcodes (octal): BYn 176724B+(n-1), Hn 176730B+(n-1), Wn 176734B+(n-1).
Format: `tn PUTBI <operand/w/t>,<bit No./r/BY>`.

### Functional pseudocode (microcode trace)
```
1. SC6 <- read <operand>                                          [PUTBIBY 000460]
2. LC  <- read <bit No.>                                          [000461]
3. range check (BM03/04/05 - LC); out of range -> SET_IOV (IOV)   [000462/000463]
      else -> PUT_BIT (ORCON=20 selects Rn as B-operand)
4. PUT_BIT (003341): ALU,AND A,BM00(=1) ORB,IN(=Rn)
      bit_value = Rn & 1
      ST,SAVA  -> Z from (Rn & 1)
      -> PUT_BIT_1
5. PUT_BIT_1 (003343): C,ALU ALU,ANDCA / ALUF,OR ; A,BMLC B,SC6 ; COND,MZRO
      if bit_value == 0 : operand = (~(1<<bit_number)) & operand   ; clear
      else              : operand = (1<<bit_number)  | operand     ; set
      WRITE C,MEMOT  (operand written back; other bits preserved)
      G,OOPS
```

### Operands / datatypes
- `<operand>` r/w, BY / H / W - read-modify-written in place.
- `<bit No.>` r, BY (signed) - bit position.
- Source register Rn (I1..I4); only bit 0 of Rn is used.

### Result / side effects
Bit `<bit No.>` of operand := bit 0 of Rn. All other operand bits unaffected
(manual: unaffected even when the destination is a word register). Atomic
read-modify-write.

### Status flags
| Flag | Effect |
|------|--------|
| Z | CONDITIONAL - set if the transferred bit (Rn&1) = 0, else cleared (`ST,SAVA` at PUT_BIT) |
| C | CLEARED (rule 4040) |
| O | CLEARED (rule 4040) |
| S | CLEARED (rule 4040) |
| K | UNCHANGED (may be set by descriptor addressing) |

### Traps
Addressing traps; IOV if bit_number < 0 or bit_number >= data_type_bits.

### Citation
Microcode PUTBIBY/H/W = 000460/000464/000470 -> PUT_BIT 003341 -> PUT_BIT_1
003343 (lines 318-329, 1775-1777). Manual section 10.28 "Put bit"
(lines 5278-5304): "transferred bit = 0 -> Z". AGREE.

---

## SETBI - Set bit (force a single bit to 1)

Opcodes (octal): BY 176200B, H 176201B, W 176202B (no register variant).
Format: `t SETBI <operand/w/t>,<bit No./r/BY>`.

### Functional pseudocode (microcode trace)
```
1. SC6 <- read <operand>                                          [SETBIBY 000430]
2. LC  <- read <bit No.>                                          [000431]
3. range check (BM03/04/05 - LC); out of range -> SET_IOV (IOV)   [000432/000433]
      else -> SET_BIT_1
4. SET_BIT_1 (003344): ALU,A A,BM00(=1) ; ST,SAVA
      forces a result of 1 -> ST,SAVA sets Z=0, S=0, C=0, O=0 (all cleared)
      -> PUT_BIT_1
5. PUT_BIT_1 (003343): C,ALU ALU,ANDCA / ALUF,OR ; COND,MZRO
      result was 1 (Z=0) -> takes the OR branch:
      operand = (1<<bit_number) | operand      ; set the bit
      WRITE C,MEMOT ; other bits preserved ; G,OOPS
```
(SETBI and PUTBI share PUT_BIT_1; SETBI forces the "set" branch by loading 1,
PUTBI chooses set/clear from Rn&1.)

### Operands / datatypes
- `<operand>` r/w, BY / H / W - read-modify-written in place.
- `<bit No.>` r, BY (signed) - bit position.

### Result / side effects
Operand bit `<bit No.>` := 1; all other bits preserved. Atomic RMW.

### Status flags
| Flag | Effect |
|------|--------|
| Z | CLEARED (status saved from forced result 1) |
| C | CLEARED |
| O | CLEARED |
| S | CLEARED |
| K | UNCHANGED (may be set by descriptor addressing) |

Manual: "Data status bits: All cleared." Ground-truth mechanism: `ST,SAVA` on a
forced value of 1 -> Z=0 and no carry/overflow/sign.

### Traps
Addressing traps; IOV if bit_number < 0 or bit_number >= data_type_bits.

### Citation
Microcode SETBIBY/HW/W = 000430/000434/000440 -> SET_BIT_1 003344 -> PUT_BIT_1
003343 (lines 294-305, 1778, 1777). Manual section 10.30 "Set bit"
(lines 5346-5378): "All cleared". AGREE.

---

## CLEBI - Clear bit (force a single bit to 0)

Opcodes (octal): BY 177175B, H 177176B, W 177177B (no register variant).
Format: `t CLEBI <operand/w/t>,<bit No./r/BY>`.

### Functional pseudocode (microcode trace)
```
1. SC6 <- read <operand>                                          [CLEBIBY 000444]
2. SC5 <- read <bit No.>                                          [000445]
3. LC  <- SC5                                                     [000446]
4. range check: (BM03/04/05 - SC5); out of range -> CLE_BIT_1..IOV[000447]
5. CLE_BIT_1 (003346): ALU,FZRO ; ST,SAVA
      forces a result of 0 -> ST,SAVA sets Z=1 (S=0,C=0,O=0)
      -> CLE_BIT_2
6. CLE_BIT_2 (003350): C,ALU ALU,ANDCA / ALUF,OR ; A,BMLC B,SC6 ; COND,MZRO
      result was 0 (Z=1) -> takes the ANDCA branch:
      operand = (~(1<<bit_number)) & operand   ; clear the bit
      WRITE C,MEMOT ; other bits preserved
7. -> CLE_IOV (003351) -> DGET_NEXT (003352) ; fetch next instruction
```

### Operands / datatypes
- `<operand>` r/w, BY / H / W - read-modify-written in place.
- `<bit No.>` r, BY (signed) - bit position.

### Result / side effects
Operand bit `<bit No.>` := 0; all other bits preserved. Atomic RMW.

### Status flags
| Flag | Effect |
|------|--------|
| Z | SET (=1) - status saved from the forced-zero ALU result (`ST,SAVA` at CLE_BIT_1) |
| C | CLEARED |
| O | CLEARED |
| S | CLEARED |
| K | UNCHANGED (may be set by descriptor addressing) |

Manual: "Data status bits: 1 -> Z." Ground-truth: the microcode deliberately runs
`ALU,FZRO` + `ST,SAVA`, saving the status of a zero result, which is why Z is
unconditionally 1 (and S/C/O clear).

Note: the committed C model `Clebi.c` sets Z=1 but leaves S "unaffected". Per the
microcode `ST,SAVA` and rule 4040, S is CLEARED (0), not left unchanged - a minor
divergence in the C emulator.

### Traps
Addressing traps; IOV if bit_number < 0 or bit_number >= data_type_bits.

### Citation
Microcode CLEBIBY/HW/W = 000444/000450/000454 -> CLE_BIT_1 003346 -> CLE_BIT_2
003350 (lines 306-317, 1780-1784). Manual section 10.29 "Clear bit"
(lines 5310-5342): "1 -> Z". AGREE on Z (manual is silent on S; microcode clears it).

---

## GETBF - Get bit field (extract a 1..32-bit field to a register)

Opcodes (octal, n = target register): BYn 176740B+(n-1), Hn 176744B+(n-1),
Wn 176750B+(n-1). Format:
`tn GETBF <operand/r/t>,<bit No./r/BY>,<field size/r/BY>`.

### Functional pseudocode (microcode trace)
```
1. SC6 <- read <operand>                                          [GETBFBY 000474]
2. SC7 <- read <bit No.>                                          [000475]
3. SC13<- read <field size>   ; CSAVE MSGN (capture negative field-size)   [000476]
4. LC  <- SC7 + SC13 = bit_number + field_size                    [000477]
      if bit_number+field_size > data_type_bits -> GETBF_IOV (IOV)
      else -> GETBFBY1 / GETBFHW1 / GETBFW1
5. GETBFxx1 (003354 / 003360 / 003364): set up field mask and shift count,
      then GET_BIT_F loop (003437-003452):
         extract field_size bits starting at bit_number, shifting them
         right-aligned into the result via Q (Q,Q/LOG = logical shift, zero fill).
      result field = (operand >> bit_number) & ((1<<field_size)-1)
6. Completion: Rn <- field (upper bits zero-filled) ; ST,SAVA -> Z,S,C,O ; G,OOPS
```

### Operands / datatypes
- `<operand>` r, BY / H / W - source, read only.
- `<bit No.>` r, BY (signed) - LSB position of the field.
- `<field size>` r, BY (signed) - number of bits, 1..32.
- Target register Rn (I1..I4); field right-aligned, upper bits zero-filled.

### Result / side effects
Rn = the extracted field, right-aligned, upper bits 0. Operand unchanged. The
field is treated as a SIGNED quantity for the sign flag (manual section 6.2, line
2459: even a one-bit field is signed).

### Status flags
| Flag | Effect |
|------|--------|
| Z | CONDITIONAL - set if extracted field = 0, else cleared (`ST,SAVA`) |
| S | CONDITIONAL - set to the leftmost (most-significant) bit of the field, i.e. bit (field_size-1) (manual: "bit field.leftmost bit -> S") |
| C | CLEARED (rule 4040) |
| O | CLEARED (rule 4040) |
| K | UNCHANGED (may be set by descriptor addressing) |

Divergence note: the manual defines S = the field's own leftmost bit. The
committed C model `Getbf.c` computes S from the DATA-TYPE MSB (bit 7/15/31) via
`nd500_set_flags_zs`, not the field MSB - these differ whenever
field_size < data_type_bits. Manual (and the "signed field" statement at line
2459) is authoritative; the C emulator disagrees. See UNRESOLVED below for the
exact microcode bit feeding S.

### Traps
Addressing traps; IOV if bit_number < 0, or field_size <= 0, or
bit_number + field_size > data_type_bits.

### Citation
Microcode GETBFBY/H/W = 000474/000501/000506; GETBFBY1 003354, GETBFHW1 003360,
GETBFW1 003364; GET_BIT_F 003437-003452; GETBF_IOV 003353 (lines 330-344,
1785-1848). Manual section 10.31 "Get bit field" (lines 5384-5417). AGREE on
operation, Z, and the field-MSB rule for S.

---

## PUTBF - Put bit field (insert register low bits into a 1..32-bit field)

Opcodes (octal, n = source register): BYn 176754B+(n-1), Hn 176760B+(n-1),
Wn 176764B+(n-1). Format:
`tn PUTBF <operand/w/t>,<bit No./r/BY>,<field size/r/BY>`.

### Functional pseudocode (microcode trace)
```
1. SC6 <- read <operand>                                          [PUTBFBY 000513]
2. SC7 <- read <bit No.>                                          [000514]
3. SC10<- read <field size>  ; CSAVE MSGN (negative field-size)   [000515]
4. LC  <- SC10 + SC7 = field_size + bit_number                    [000516]
      if out of range -> PUTBF_IOV (IOV)
      else -> PUTBFBY2/3 ... PUTBF_1
5. Build field mask via the PUTBF_TAB / PUTBF_Txx computed-shift table
      (Q-register shift chain): mask = (1<<field_size)-1            [003472-003542]
      ST,SAVA at PUTBF_TAB (003503) records Z/S from the inserted field value.
6. PUTBF_3 (003543): ALU,ANDCB A,SC6 B,SC3 -> operand & ~(mask<<bit_number)
      = clear the target field.
7. 003544: ALU,OR A,SC10 B,SC6 -> (Rn_field << bit_number) | cleared_operand
      = insert the field ; WRITE C,MEMOT ; other bits preserved ; G,OOPS
```

### Operands / datatypes
- `<operand>` r/w, BY / H / W - read-modify-written in place.
- `<bit No.>` r, BY (signed) - LSB position of the field.
- `<field size>` r, BY (signed) - number of bits, 1..32.
- Source register Rn (I1..I4); only its low field_size bits are used.

### Result / side effects
Operand field [bit_number .. bit_number+field_size-1] := low field_size bits of
Rn. All bits outside the field preserved. Atomic RMW. Field treated as signed for
the S flag (manual line 2459).

### Status flags
| Flag | Effect |
|------|--------|
| Z | CONDITIONAL - set if the inserted field value = 0, else cleared (`ST,SAVA`) |
| S | CONDITIONAL - set to the field's leftmost bit, bit (field_size-1) (manual: "bit field.leftmost bit -> S") |
| C | CLEARED (rule 4040) |
| O | CLEARED (rule 4040) |
| K | UNCHANGED (may be set by descriptor addressing) |

Same S-source divergence as GETBF: manual = field MSB; C model `Putbf.c` uses the
data-type MSB. See UNRESOLVED.

### Traps
Addressing traps; IOV if bit_number < 0, or field_size <= 0, or
bit_number + field_size > data_type_bits.

### Citation
Microcode PUTBFBY/H/W = 000513/000520/000525; PUTBFBY2 003453, PUTBF_1 003472,
PUTBF_TAB 003503, PUTBF_3 003543-003544; PUTBF_IOV 003473 (lines 345-359,
1849-1906). Manual section 10.32 "Put bit field" (lines 5421-5456). AGREE on
operation, Z, and the field-MSB rule for S.

---

## GETB - Get buddy element from the heap (NOT a bit instruction)

Opcode (octal, n = target register Wn): 177114B+(n-1). Format:
`Wn GETB <log size/r/BY>`. Allocates a block of 2^(log size) WORDS from the heap
and returns its address in Wn. Present in this category by opcode grouping only.

### Functional pseudocode (microcode trace)
```
1. LC  <- read <log size>   ; TYP,BY, READ                        [GETB 000776]
2. -> GETB_1                                                      [000777]
3. GETB_1 (004376): SC6 <- BM33 = 2^27  (the STO status-bit mask) ; T,PUSH
      -> FINDBDY
4. FINDBDY (004322): ALU,ANDCB A,MIC,STS B,SC6 D,MIC,STS
      reset the STO status bit, then run the buddy-allocator:
        - if freelist[log_size] non-empty: unlink head block, address -> Wn
        - else scan larger freelists; split a larger block in halves repeatedly,
          returning the surplus halves to the appropriate freelists, until a
          block of the requested size is produced
        - if none available, or log_size > MAXL: raise STO (set 2^27) and leave
          freelists and Wn unchanged for the STO trap handler to re-seed
   TOS must point at the heap descriptor block (MAXL, STAH, ENDH, FLOG0..).
```

### Operands / datatypes
- `<log size>` r, BY - base-2 log of the block size in words.
- Target register Wn (I1..I4) receives the 32-bit block address.

### Result / side effects
On success: Wn = address of the allocated block; the block is unlinked (and
possibly split) from the freelists; STO status bit reset. On exhaustion: STO
status bit set, no state changed, trap taken.

### Status flags
| Flag | Effect |
|------|--------|
| Z | UNCHANGED (no `ST,SAVA` on data status; manual: data status "Unaffected") |
| C | UNCHANGED |
| O | UNCHANGED |
| S | UNCHANGED |
| K | UNCHANGED |
| STO (ST1 bit 27) | SET on heap exhaustion / log_size>MAXL; RESET on success (set/reset per GETB, per manual) |

### Traps
Addressing traps; Stack Overflow (STO) if no block of the requested size or
larger is available, or if log_size > MAXL.

### Citation
Microcode GETB = 000776 -> GETB_1 004376 -> FINDBDY 004322 (lines 524-525, 2272,
2316). Manual section 15.13 "Get buddy" (lines 9770-9801) and section 3.3
(lines 1151-1194); STO = status bit 27 (BM33 = 2^27). AGREE (data status
unaffected; STO set/reset). Matches C model `Getb.c`.

---

## Summary table

| Instr | Octal (BY/H/W) | Operation | Z | C | O | S | K |
|-------|----------------|-----------|---|---|---|---|---|
| GETBI | 176264/176270/176720 (+n-1) | Rn.0 = op[bit] | =1 if bit=0 | 0 | 0 | 0 | unch |
| PUTBI | 176724/176730/176734 (+n-1) | op[bit] = Rn.0 | =1 if Rn.0=0 | 0 | 0 | 0 | unch |
| SETBI | 176200/176201/176202 | op[bit] = 1 | 0 | 0 | 0 | 0 | unch |
| CLEBI | 177175/177176/177177 | op[bit] = 0 | 1 | 0 | 0 | 0 | unch |
| GETBF | 176740/176744/176750 (+n-1) | Rn = field(op) | =1 if field=0 | 0 | 0 | field MSB | unch |
| PUTBF | 176754/176760/176764 (+n-1) | field(op) = Rn | =1 if field=0 | 0 | 0 | field MSB | unch |
| GETB  | 177114 (+n-1) | Wn = heap alloc 2^n words | unch | unch | unch | unch | unch (STO set/reset) |

"unch" = unchanged. K may additionally be set (never cleared) as a side effect of
descriptor addressing of the operand (manual section 8.15).

---

## UNRESOLVED (needs deeper microtrace)

1. GETBF / PUTBF - the exact microcode bit that feeds the S flag. The manual
   states S = the field's leftmost bit (bit field_size-1), treating the field as
   signed. The traced completion uses `ST,SAVA` on the field result, and the
   field is assembled right-aligned with zero fill via the Q-register shift chain
   (GET_BIT_F / PUTBF_TAB). Whether the microcode sign-extends the field (so the
   ALU sign bit equals the field MSB) before `ST,SAVA`, or takes S from the raw
   data-type MSB, was not fully resolved from the shift-loop cells. The C emulator
   (`Getbf.c`/`Putbf.c`) uses the DATA-TYPE MSB, which contradicts the manual's
   field-MSB rule; confirming the microcode's choice requires tracing the
   sign-handling in GET_BIT_F_1..5 (003441-003452) and BFW_END (003431) /
   PUTBF_TAB (003503) at the bit level.
