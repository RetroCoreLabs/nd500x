# Sync Handoff: STRING Wrong-Instruction Cluster (Section 3A)

Date: 2026-07-19
Repos to keep identical:
- nd500x (C):     /home/ronny/repos/nd500x
- RetroCore (C#): /mnt/e/Dev/Repos/Ronny/RetroCore

Ground truth: ND-500 Reference Manual /home/ronny/repos/nd500x/docs/ND-05.009.4 EN
ND-500 Reference Manual.md (verified below), which matches the ND-5000 microcode carve.
The docs/instructions/asm/*.md notes and the prior C/C# code described the WRONG
instructions and are NOT authoritative.

## Root cause

Five STRING-opcode instructions implemented a DIFFERENT instruction than their opcode:

| Mnemonic | Opcode | Was (wrong) | Is (manual) |
|----------|--------|-------------|-------------|
| SCPUNO | 177774B / 0xFF7CC | 3-op string-copy-until | 16.36 Store CPU number: `SCPUNO <dest/w>`, `<CPUNO> -> dest` |
| SSCAN  | 176661B / 0xFDB1  | 2-op plain equal scan (set S) | 14.16 masked+translated scan: `SSCAN <src>,<mask>,<table>` |
| SSPAN  | 176662B / 0xFDB2  | 2-op char-set membership | 14.17 complement of SSCAN |
| SMATCH | 176663B / 0xFDB3  | operand-swapped, updated I1, set S | 14.18 substring search: sub=op0 (I1 kept), string=op1 (I2 advances) |
| SCOPT  | 176677B / 0xFDBF  | 4-op copy-translate-until-stop (wrote memory) | 14.13 compare-translated-with-pad (SCOPA + table on every byte) |

Manual verification (grep of ND-05.009.4):
- 16.36 "SCPUNO - Store CPU number", format `SCPUNO <destination/w>`, op `<CPUNO> -> <destination>`.
- 14.16 `BY SSCAN <source/r/BY/I1=>, <mask/r/BY>, <trans table/aa/BY>` (example line 9010:
  `BY SSCAN IND(B.FUNCTION), ACTIVE, ALT(FNTAB)`).
- 14.18 `BY SMATCH <substring/r/BY/I1=>,<string/r/BY/I2=>`.
- Dispatch table operand_counts already agree: sscan=3, sspan=3, smatch=2, scopt=4, scpuno=1.

## Correct behavior (from manual + carve)

- SCPUNO: CPUNO (0 for single-CPU emulation, same as C# STUB analysis) -> destination;
  ST,SAVA status (Z if 0, S if sign); C,O cleared (rule 4040). One operand.
- SSCAN: while I1<count and (table[src[I1]] AND mask)==0: I1++. Found (masked!=0) Z=0,
  I1:=found; end/empty Z=1, I1:=next. K/S/C/O cleared.
- SSPAN: complement - while (table[src[I1]] AND mask)!=0: I1++. Stop-on-zero Z=1;
  exhausted Z=0. K/S/C/O cleared.
- SMATCH: substring=op0 (indexed by I1, KEPT/unmodified), string=op1 (indexed by I2).
  Naive substring search; found Z=1 I2:=match; string-exhausted Z=0 I2:=next. K/S/C/O cleared.
- SCOPT: = SCOPA (compare-with-pad) but every compared byte, including the pad past a
  string's end, is translated through a 256-byte table. equal K=0 Z=1 S=0; src1>src2
  K=1 Z=0 S=0; src1<src2 K=1 Z=0 S=1. Indices advance only while inside own string. C,O cleared.

## nd500x changes - DONE, committed, green

- src/cpu/instructions/STRING/Scpuno.c   (commit 1defdb1)
- src/cpu/instructions/STRING/Sscan.c, Sspan.c, Smatch.c, Scopt.c  (commit 16f3b0f)
- Build clean; nc compile gate 4/4 on each commit.
- Note: nd500x reads descriptor/table BASE via `fi->operands[i].effective_address`
  and the mask/pad byte via `nd500_read_operand_value(..., ND500_DTYPE_BYTE)`.

## RetroCore (C#) emulator mirror - DONE (subagent), uncommitted

- Emulated.HW/ND/CPU/ND500/Instructions/STRING/{Sscan,Sspan,Smatch,Scopt}.cs and Scpuno.cs.
- Portable idioms only, modeled on the correct sibling Scopa.cs:
  StringDescriptor.LoadFromMemory(cpu, addr) -> .ElementCount/.BaseAddress;
  descriptor/table address AND mask/pad byte both via ReadOperandValue(fi.Operands[i],
  fi.DataType, fi.DataWidth); table byte via ReadMemory(addr,1)&0xFF; regs.I1/I2;
  regs.ST.K/Z/S/C/O.
- OPEN VERIFICATION ITEM: nd500x uses effective_address for the descriptor/table base
  while C# uses ReadOperandValue. Scopa.cs (30 passing SCOMP cases via the same idiom)
  implies ReadOperandValue returns the ADDRESS for these absolute operand encodings, so
  the two are consistent - but this is CONFIRMED only by the regen+validate below.

## Generator - rewrite in progress (subagent)

ComprehensiveStringGenerator.cs: GenerateSscan/Sspan/Smatch + the scopt path of
GenerateScopVariantScenarios rewritten to emit CORRECT scenarios (descriptors +
256-byte identity translate table at 0x3000 + mask/pad byte operands), expected values
per the nd500x oracle. scpuno/scotr paths left intact.

## Regen (USER runs - heavy dotnet step; also settles the encoding open item)
```
cd /mnt/e/Dev/Repos/Ronny/RetroCore
dotnet test Emulated.Tests.ND500 --filter "Generate_Master_JSON"
cp Emulated.Tests.ND500/bin/Debug/net9.0/nd500_tests.json /home/ronny/repos/nd500x/test/nd500_tests.json
cd /home/ronny/repos/nd500x && make
./build/bin/test_instruction_validation --continue
```
Expected: new SSCAN/SSPAN/SMATCH/SCOPT/SCPUNO cases assemble and PASS. If some fail to
assemble (operand form) or the PC check fails (instrSize), the exporter/runner reports
which - fix the operand encoding / instrSize in the generator and re-run. Once green,
commit RetroCore (5 emulator files + generator) and the refreshed nd500x test JSON.
