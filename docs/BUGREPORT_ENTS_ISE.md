# Bug report: false "ENTS must be preceded by CALL/CALLG" (ISE) after a page fault

Handoff document. Every path below is absolute and complete.

## 1. Symptom

Booting NDIX on the `nd500x` emulator and running the native ND-500 assembler
in-guest halts the machine:

```
[TRAP] ENTS at PC=0x0001595E: Must be preceded by CALL/CALLG
[STOP] trap at PC=0x0001595E data=0x00000000 (B=0x08000230 R=0x00000000 L=0x000025A4)
```

The trap is an Instruction Sequence Error raised by the emulator's own
precondition check, not by the guest.

## 2. Exact reproduction

```
cd <nd500x repo root>
./run-ndix.sh                 # boots multiuser to a login: prompt
# log in as: root   (no password)
/lib/as -o /tmp/h.o /usr/src/hello.s
```

Scripted equivalent (pty harness, drives login + commands):

```
python3 tools/ptyboot.py \
        <logfile> "root" "/lib/as -o /tmp/h.o /usr/src/hello.s; echo AS=\$?"
```

Set `ND500X_THADBG=1` to see the trap dispatches that precede it.

Captured logs:
- `as_ents.log` (session scratch, not preserved) (with THADBG)
- `as_fix.log` (session scratch, not preserved)

Disk image: `$NDIX/rootfs_full.img`
Kernel: `$NDIX/kernel/MASTER/GENERIC/vmunix`
Emulator binary: `build/bin/nd500x`

This is only reachable as of commit `edcaf98` in `<nd500x repo root>`
("fix(cpu): abort a multi-part memory access at its first fault"). Before that
commit the assembler died earlier, on a double-fault halt, and never got here.

## 3. Verified facts

Each of these was read from source or from a live trace. Nothing here is
inferred.

1. The immediately preceding event is a normal, successfully dispatched user
   page fault:
   ```
   [THADBG] trap 38 trapPC=0x0000309D data=0x00001E28 CED=0 CAD=6
            THA=0xE800073C slot@0x004E07D4 haddr=0x00000381 xdom=1
   ```
   Trap 38 = page fault. `xdom=1` = cross-domain dispatch. `CAD=6` = trapping
   (user) domain, `CED=0` = handling (kernel) domain. The handler address
   `0x381` is non-zero, so dispatch succeeded.

2. The check that fires is the emulator's own, at
   `src/cpu/instructions/CALL/Ents.c:52-57`:
   ```c
   if (cpu->pending_call_return_address == 0) {
       printf("[TRAP] ENTS at PC=0x%08X: Must be preceded by CALL/CALLG\n", fi->address);
       trap_instruction_sequence_error(cpu, fi->address);
       return;
   }
   ```
   Identical checks exist in the sibling files in the same directory:
   `Entd.c:34`, `Entsn.c:61`, `Entm.c:210`.

3. `pending_call_return_address` is declared at
   `src/cpu/cpu_protos.h:77`. Searching the whole
   tree, it is written in exactly these places:
   - `src/cpu/cpu.c:94` - reset only
   - the `CALL`/`ENT*` instruction implementations under
     `src/cpu/instructions/CALL/`
   - `src/cpu/nd500_indirect.c:349,394,406`
   **It is NOT saved or restored by trap dispatch (`invoke_trap_handler` /
   `ENTT`) nor by `RETT`.** There is no per-context copy of it.

4. On a cross-domain trap dispatch the emulator DELIBERATELY keeps the
   trapping program's stack registers rather than loading the handler's -
   see the comment in `src/cpu/cpu.c` in `raise_trap`:
   "Deliberately leave TOS/LL/HL as the trapping program's live values so the
   register block that ENTS saves - and RETT restores - carries the trapping
   domain's stack registers, not the handler's."
   This is why the halt reports `B=0x08000230`, a USER stack address
   (user stacks start around 0x080000BC), even though the PC is in kernel text.

5. `PC=0x0001595E` is not inside the program being run. Header sizes, read with
   `$PCC_ND500/bin/nd500-dump -a`:
   | binary | magic | text | data | bss | entry |
   |---|---|---|---|---|---|
   | `/lib/as`  | ZMAGIC 0413 | 0xB000  | 0xF000  | 0x89A0  | 0x4A |
   | `/lib/cc1` | ZMAGIC 0413 | 0x13000 | 0x11800 | 0x18E48 | 0x4A |
   | `/lib/ld`  | ZMAGIC 0413 | 0x6800  | 0x2800  | 0x6110  | 0x4A |
   Text base is 0 (entry 0x4A). 0x1595E exceeds all three. Kernel text is
   ~0x41A94, so 0x1595E lies within it. Combined with fact 4 this is consistent
   with the ENTS executing in KERNEL text while B still holds the user value.

## 4. Hypothesis - NOT verified, must be tested before acting on it

`pending_call_return_address` is global CPU state with no save/restore across
traps (fact 3). A `CALL` sets it and jumps to the callee; the callee's FIRST
instruction is the `ENTS` prologue, which is frequently on a page that is not
yet resident. So the natural sequence is:

1. `CALL` executes, sets `pending_call_return_address`, PC := callee.
2. Fetching/executing the callee's `ENTS` page-faults.
3. The fault is dispatched (cross-domain, into the kernel). The kernel's own
   fault-service code runs and itself executes `CALL`/`ENT*` pairs, and every
   `ENT*` CLEARS `pending_call_return_address` (step 7 of the documented
   sequence in `Ents.c`).
4. `RETT` returns to the interrupted `ENTS`, which now sees
   `pending_call_return_address == 0` and raises a FALSE ISE.

If true, the bug is that this is emulator-invented state that must either be
part of the saved/restored trap context, or must not exist at all (see §5).

Prediction that would confirm it: instrument every write to
`pending_call_return_address` with PC + CED, and check whether the value set by
the `CALL` preceding PC=0x1595E is cleared by an intervening `ENT*` in the
kernel before the failing `ENTS` runs.

## 5. The question to put to the microcode oracle

The real question is not "when is the flag cleared" but **whether the ND-5000
hardware performs this check at all, and against what state.** If the hardware
does not have a "pending call" latch, the emulator's check is an invention and
the correct fix is to delete it rather than to save/restore it.

Microcode sources (period-correct, independent of this emulator):
- `$ND5000UC/microcode/MICRO-5800-B30.md` - decoded microword
  listing in markdown table form, 16403 lines
- `$ND5000UC/docs/MC/MICRO-5800-B30.LABE` - label/address
  cross-reference
- `$ND5000UC/docs/MC/MICRO-5800-B30.DATA` - raw control store
- Other CPU variants alongside: 5200, 5500, 5700 (`MICRO-52xx/55xx/57xx-*`)
- Manual: `$ND5000UC/manual/ND-05.022.1 EN ND-5000 Microprogram Guide.md`
- Manual: `$ND5000UC/manual/ND-05.020.01 EN ND-5000 Hardware Description.md`

Relevant microcode entry points already located in
`$ND5000UC/microcode/MICRO-5800-B30.md`:

| label | ucode addr | md line |
|---|---|---|
| `ENTS`       | 000662 | 455 |
| `ENTS_NEQ0`  | 004171 | 2190 |
| `ENTS_1`     | 004203 | 2200 |
| `ENTS_2`     | 004207 | 2201 |
| `ENTS_3`     | 004216 | 2211 |
| `ENTS_STO`   | 004213 | 2208 |
| `ENTS_END`   | 004206 | 2203 |
| `ENTSN`      | 000666 | 459 |
| `ENTSN_STO`  | 004253 | 2240 - jumps to `TRAP` |
| `ENTF`       | 000665 | - |
| `ENTT`       | 000673 | - |

Note `ENTSN_STO` (004253) branches to `TRAP`, so there IS at least one trap
exit in the ENTS-family microcode. Determine which condition reaches it -
the label name suggests Stack overflow (STO), not sequence error.

Concrete questions for whoever picks this up:
1. Does the `ENTS` microcode (000662 -> 004203 -> ...) test any "a CALL
   happened" latch? If so, which bit, and where is it set and cleared?
2. Is there an ISE (Instruction Sequence Error) exit anywhere in the ENTS
   family, or is `ENTSN_STO`'s `TRAP` purely stack overflow?
3. If such a latch exists, is it part of the context saved by `ENTT` and
   restored by `RETT`? The `ENTT` microcode starts at 000673 with sub-labels
   `ENTT_REGS` 014046, `ENTT_DITS` 014047, `ENTT_STAH` 014050,
   `ENTT_CLRTS` 014103, `ENTT_TRRET` 014124 - `ENTT_REGS` and `ENTT_CLRTS`
   look like the places to check.

A cross-check oracle for the instruction encoding (independent of both the
emulator and the microcode) is the 1988 dbx opcode table at
`$NDIX/baseline/ucb/dbx/ops.nd500`.

## 6. Constraints for whoever fixes this

- `$NDIX/kernel/` and `$NDIX/baseline/`
  are READ-ONLY historical preservation. Never edit them. A PreToolUse hook
  blocks it. The bug is in the emulator, not in NDIX.
- Writable: `<nd500x repo root>`, `$PCC_ND500`.
- Regression gates that must stay green:
  - `build/bin/test_page_straddle` - 18/18
  - `build/bin/test_double_register` - 48/48
  - `cd $PCC_ND500 && make test` - 21/21
  - `ctest --test-dir build -R nc_` - 4/4
  - image still boots multiuser to `login:`, root logs in
- Do not kill or restart processes without the owner's approval.

## 7. What already works, so you know the bar

In-guest on `$NDIX/rootfs_full.img`, verified:
- `/lib/cc1 /usr/src/hello.i > h.ic` exits 0 (the C front-end compiles)
- `/lib/ld -e start -x -o hello /lib/crt0.o hello.o /lib/libc.a && ./hello`
  links and runs, printing `HELLO-FROM-NATIVE-ND500-TOOLCHAIN` and `2+2 = 4`
- `/usr/bin/cc -E` preprocesses cleanly
- `/usr/src/ptest2` reports `PHASE-A-OK` and `PHASE-B-OK`

`/lib/as` is the last piece missing before NDIX self-hosts a full
`cc hello.c`.
