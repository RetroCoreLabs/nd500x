# CAT-500 code generator + 317B UECOM invocation - C# / RetroCore sync handoff

**Full path:** `/home/ronny/repos/nd500x/docs/CAT500_UECOM_CSHARP_HANDOFF.md`
Date: 2026-07-14
Audience: the LLM maintaining the C# RetroCore ND-500 emulator
(`/mnt/e/Dev/Repos/Ronny/RetroCore/Emulated.HW/ND/CPU/ND500/` + its MON layer).

> STATUS: DESIGN + BREAKTHROUGH RECORD. The architecture and the 317B UECOM implementation
> design below are settled and grounded in the nd500x internals. The concrete code changes are
> tracked in Part E (Change log) as they land. The MON-gap list (Part C) is finalised from the
> CAT-500 disassembly analysis in `/mnt/d/ND/500/CAT5-CAT/`.

This document exists so RetroCore can mirror every change. Ground truth remains the ND manuals +
the carved SINTRAN L07, not either emulator. Flag anything "INFERRED" identically on both sides.

---

## PART A - The breakthrough: why BOUT.NRF was always 0 bytes

The 0-byte object file was NEVER an emulator or MON-contract bug. It was a MISSING BACK-END.

The ND C compiler is part of a Vienna Development Method (VDM) MULTI-LANGUAGE compiler system
(ACM 10.1145/502949.502894). Language frontends (Pascal, and C = the "NC" compiler `nc-a06.dom`)
parse/preprocess and emit a MACHINE-INDEPENDENT intermediate ("CAT code") into the always-open
scratch file:
  - SINTRAN file number 0100 octal = 64 decimal = 0x40, named `SCRATCHnn:DATA`.
  - This is EXACTLY the file the object WFILEs go to. So the auto-opened scratch-64 (nd500x:
    `/home/ronny/repos/nd500x/src/libmon/mon_dispatch.c:73`, gated by `mon_config_get_auto_scratch_64()`;
    RetroCore: your `OpenScratchFile64`) is CORRECT BY DESIGN - not a pre-emption bug.

The frontend then invokes the shared back-end CODE GENERATOR as a SINTRAN command via MON 317B
UECOM. The command string it builds (live-captured, apostrophe-0x27-terminated) is `CAT-CAT5-B`
(and `NC-A`). By SINTRAN command abbreviation (Users Guide ND-60.050.06 line 1563: "-" separates
parts, each part abbreviated if unambiguous) these resolve to:
  - `NC-A`       -> `NC-A06` (the driver itself; nc-a06.dom).
  - `CAT-CAT5-B` -> the CAT-500 code generator, version B ("B" matches "B06").

The back-end reads the CAT intermediate from `SCRATCHnn:DATA` and emits the target object. CAT is
RETARGETABLE: CAT-500 -> `:NRF` (ND-500), CAT-100 -> `:BRF` (ND-100), m68k hypothetically. So the
same frontend cross-compiles by swapping the CAT back-end.

BOTH emulators stub 317B UECOM (log the command, return success, never run the program). So the
back-end never runs, `SCRATCHnn:DATA` is never turned into an object, and BOUT.NRF stays 0 bytes.
On the older code path this same gap surfaced as the 0x080241FC "T1-B" crash (NC walking a stale
pass-record because the pass never ran).

### The binary (now located)
`/mnt/d/ND/500/CAT5-CAT/cat-cat5-b06.dom` = "CAT-500 - Version B06 - 1988-01-05" (280957 bytes).
VALIDATED under nd500x (harness `/home/ronny/repos/nd500x/test/diag_domload.c`): loads, entry
0x08000004, issues TIME/TUSED/RSIO/GSWSP/ROBJE/SETBS/RMAX/SMAX, prints banner + "Cat-500:" prompt,
then reads commands via 503B DVINST (same device-0/1 interactive model as NC). Strings confirm the
role: "code generation : ok", "can't open CAT file", "can't map scratch file into memmory",
"can't write to object file", "can't set block size of object file", ENTEROBJECTS,
"inline-assembler not implemented for NRF output".

`nc-a06.dom` is a single-domain image (no overlays); the code generation is delegated to CAT-500.
RetroCore side: ask Ronny for the file; rename to your SINTRAN name form. Golden output reference:
`/mnt/d/ND/500/FraTor/test-real/test-real.nrf`.

---

## PART B - 317B UECOM implementation design (the shared contract)

Goal: `NC compile B,B,BOUT` (or `check` then a fresh `generate-code`) produces a real `BOUT.NRF`,
by having 317B UECOM actually run the named program (CAT-500) as a nested SINTRAN sub-process that
shares the file system with the caller.

The nd500x internals this maps to (RetroCore mirrors the BEHAVIOUR, not the API names):

1. **Handler reach into the machine.** `MonContext` exposes `void* cpu` and `void* machine`
   (`/home/ronny/repos/nd500x/src/libmon/mon_types.h:62-106`). The 317B handler
   (`/home/ronny/repos/nd500x/src/libmon/handlers/mon_317B_ExecuteCommand.c`) casts these to
   `Nd500Cpu*` / `Nd500Machine*`.

2. **Resolve the command to a program image.** Parse the UECOM command string (already decoded via
   the descriptor reader - see the descriptor fix note in Part C). Apply SINTRAN abbreviation to
   match a program name to a DOM path via an emulator-side name->path table, e.g.
   `CAT-CAT5-B`/`CAT-CAT5-B06` -> `/mnt/d/ND/500/CAT5-CAT/cat-cat5-b06.dom`. (Config-driven; do NOT
   hardcode a single path in the handler.) `NC-A`/`NC-A06` -> `nc-a06.dom` (recursive self-invoke).
   If the command is not a known program, keep today's benign stubbed success (existing behaviour).

3. **Load the program into a SEPARATE domain.** nd500x already supports this:
   `ndlib_dom_load_to_machine(machine, cpu, target_domain=-1 (auto-allocate), NULL, NULL,
   &start_addr, &out_domain)` (`/home/ronny/repos/nd500x/src/ndlib/ndlib.h:141`). The debugger's
   `loaddom <path> [domain] [autostart]` (`/home/ronny/repos/nd500x/src/debugger/commands.c:1300`)
   is the working template. The domain system provides the isolated program/data address space
   (`nd500_domain_allocate` / `nd500_domain_switch` / `nd500_domain_save_state` /
   `nd500_domain_load_state`, `/home/ronny/repos/nd500x/src/cpu/nd500_domain.h`). Note the DOM
   loader uses a global `g_dom_file` (`/home/ronny/repos/nd500x/src/ndlib/ndlib_dom.c:32`);
   loading CAT-500 overwrites it, which is safe because NC's segments are already resident in
   machine memory (only re-needed if we reload NC, which we do not).

4. **Run the sub-program to completion, then return to the caller.** MON 0B LEAVE does NOT hard-stop
   the machine; it sets `ctx->halt_requested` via `mon_request_halt`
   (`/home/ronny/repos/nd500x/src/libmon/handlers/mon_0B_ExitFromProgram.c:20`). The UECOM handler
   runs a NESTED step loop over the CAT-500 domain until that domain issues 0B LEAVE, then restores
   the caller's (NC's) domain/registers and returns from the MON call so NC continues. RetroCore
   must likewise treat a sub-process LEAVE as "return to invoker", not "terminate emulation".

5. **Share the SINTRAN file table.** The file table is a process-global in nd500x
   (`/home/ronny/repos/nd500x/src/libmon/mon_file_table.c`), so CAT-500 sees the caller's open files
   - crucially the always-open scratch (file 0x40 = `SCRATCHnn:DATA`) holding the CAT intermediate,
   and it can open/write `BOUT:NRF`. RetroCore's file table must be shared across the nested run too.

6. **Do NOT delete the scratch before the back-end runs.** Today both emulators delete scratch-64 /
   `SCRATCH-00001:*` at each program's LEAVE (nd500x: "Scratch file 64 deleted" at exit; NC also
   MDLFI-deletes SCRATCH-00001:*). For the chain to work, the intermediate in `SCRATCHnn:DATA` must
   SURVIVE from NC's write until CAT-500 reads it. Since CAT-500 runs nested INSIDE NC's UECOM call
   (before NC's own LEAVE/cleanup), the shared global scratch is still present - but verify neither
   side's LEAVE cleanup nor NC's MDLFI removes the specific intermediate scratch before the nested
   CAT-500 run consumes it.

### Manual end-to-end fallback (validation without the full feature)
Run NC (`compile B,B,BOUT`), PERSIST `SCRATCHnn:DATA` (suppress the scratch-delete-at-exit), then
run `cat-cat5-b06.dom` standalone against it to emit `BOUT.NRF`; diff vs
`/mnt/d/ND/500/FraTor/test-real/test-real.nrf`. Useful to validate the binary + pipeline before/
independently of the nested-invocation feature.

---

## PART C - MON calls CAT-500 needs (gap analysis vs nd500x)

CAT-500 must issue every MON it needs and get correct results, or it cannot emit the NRF.
Empirically it issues at startup: 11B TIME, 114B TUSED, 143B RSIO, 422B GSWSP, 41B ROBJE, 76B SETBS,
62B RMAX, 73B SMAX, 504B DVOUTS, 503B DVINST. Full extraction from the CAT-500 disassembly is in
`/mnt/d/ND/500/CAT5-CAT/cat-cat5-b06-MON-contract.md` (produced by the documentation pass).

CONFIRMED from the full CAT-500 disassembly (45 MON call sites, 31 distinct numbers; all 31 are
already REGISTERED in nd500x - none entirely missing). The SINGLE HARD BLOCKER:
- **412B FSCNT (FileAsSegment) / "map scratch file into memory" - THE blocker.** Wrapper at CAT-500
  `0x0801F080`, 4 callers. CAT-500 maps its CAT intermediate scratch into its address space as a
  segment (FSCNT) and then reads the CAT input via MEMORY LOADS from that segment - it does NOT
  RFILE it block by block. In nd500x, `mon_412B_FileAsSegment.c` is BOOKKEEPING-ONLY: it only sets
  `entry->mapped_as_segment / mapped_segment_no / segment_access_type` in the open-file table
  (lines 88-90) and returns the segment number; it NEVER backs that logical segment with the file's
  bytes. So CAT-500 reads garbage/unmapped memory from the segment and cannot generate the NRF.
  There is NO `connect_file_as_segment` callback in `mon_types.h` today (only `allocate_segment`,
  used by 422B GSWSP). FIX: add a host callback that (a) ensures a real MMU/PST-backed segment
  exists at the logical segment number in the caller's domain, and (b) loads the open file's bytes
  into that segment's memory (respecting AccessType), mirroring how `allocate_segment` wires an
  MMU-backed scratch segment for 422B GSWSP. 413B FSCDNT (currently PARTIAL) must write the segment
  back to the file if it was mapped writable, and free the mapping.
- Everything else CAT-500 needs is implemented: 422B GSWSP uses the real `allocate_segment` callback
  (genuine MMU-backed scratch segment - not a blocker); the object-output chain
  (50B/221B/41B/76B/73B/62B/120B/43B) is registered and just needs end-to-end verification vs the
  golden NRF.

FULL GAP TABLE + per-call contract: `/mnt/d/ND/500/CAT5-CAT/cat-cat5-b06-MON-contract.md`.
Disassembly: `/mnt/d/ND/500/CAT5-CAT/cat-cat5-b06.asm`. Analysis:
`/mnt/d/ND/500/CAT5-CAT/cat-cat5-b06-analysis.md`. README: `/mnt/d/ND/500/CAT5-CAT/README.md`.

Prerequisite already aligned: 317B UECOM must read its command as a `[Length:4][Pointer:4]`
DESCRIPTOR, not raw bytes (nd500x: switch `mon_read_sintran_string` -> `mon_read_descriptor_string`
in the 317B handler, as 50B OPEN does; RetroCore already switched UECOM to the descriptor reader).
Same for 54B MDLFI.

---

## PART D - Program-image resolution (both emulators)

NC/CAT invoke programs by SINTRAN name via UECOM. The emulator needs a name->DOM-path resolver with
SINTRAN abbreviation, config-driven (not hardcoded). Minimum entries for the C pipeline:
  - `NC-A` / `NC-A06`            -> `/mnt/d/ND/500/FraTor/nc/nc-a06.dom`
  - `CAT-CAT5-B` / `CAT-CAT5-B06`-> `/mnt/d/ND/500/CAT5-CAT/cat-cat5-b06.dom`
Also present in nc-a06.dom's pass-name table (offset 0x33008), for completeness: CAT-OPTI-B
(optimizer), CAT-COPY-B, CAT-NC-CROSS-A (xref), CAT-CAT1-B, CAT-CAT3-B, CAT-CAT6-B, CAT-CAT8-B.
Only CAT-CAT5 (the code generator) is needed for a basic `int x; main(){x=1;}` compile; the others
are optimization/xref passes invoked per options.

---

## PART E - Change log (running; every entry mirrors to RetroCore)

Host-tooling (NOT a RetroCore change, recorded for completeness):
- Rebuilt + reinstalled `nd500-dis` (`/home/ronny/repos/ragge/pcc-nd500/src/nd500-dis/nd500-dis.c`
  -> `/usr/local/bin/nd500-dis`). The old installed binary did not parse the DOM format and emitted
  no MON annotations; the current build parses DOM and its 233-entry MON name table matches the
  nd500x authoritative registry (`/home/ronny/repos/nd500x/src/libmon/mon_registry.c`) with 0
  mismatches. Use it for all ND-500 disassembly.

nd500x emulator changes (to be filled as implemented; each is a RetroCore parity item):
- [ ] 317B UECOM: descriptor-read command (if not already) + program-name resolution + nested
      DOM-load-into-domain + run-to-LEAVE + return-to-caller.
- [ ] Scratch persistence across the nested run (do not delete the intermediate scratch before
      CAT-500 consumes it).
- [ ] 412B FSCNT: real file-as-segment mapping so CAT-500 can map the scratch intermediate.
- [ ] Any further MON gaps from `/mnt/d/ND/500/CAT5-CAT/cat-cat5-b06-MON-contract.md`.
- [ ] Program-image name->path resolver (config-driven).

Validation target for every step: produce `BOUT.NRF` from `int x; main(){ x=VALUE; }` and diff its
structure vs `/mnt/d/ND/500/FraTor/test-real/test-real.nrf`.
