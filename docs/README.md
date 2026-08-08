# ND500X documentation

What each document is for, so there is an obvious place to look and an obvious
place to put something new.

Two rules keep this folder from silting up again:

1. **A document earns its place by being read, not by being written.** Progress
   reports, completion summaries and status snapshots go stale the day after
   they are written and are then actively misleading. Record the *finding*, not
   the fact that you worked on it.
2. **Cite the evidence.** Byte offsets, file and line, a manual page, a carve
   address. A claim that cannot be checked cannot be trusted later, by anyone.

---

## Using the emulator

| Document | Covers |
|---|---|
| [DEBUGGER_COMMAND_REFERENCE.md](DEBUGGER_COMMAND_REFERENCE.md) | Every interactive debugger command, the `.map` source-map format, debugging recipes, and manual verification procedures. The main reference. |
| [DAP_INTEGRATION.md](DAP_INTEGRATION.md) | Running the DAP server on port 4500 and attaching VS Code or another DAP client. |
| [DAP_COMMAND_TEST_MATRIX.md](DAP_COMMAND_TEST_MATRIX.md) | Every DAP command the adapter supports, with live test results. |
| [DAP_BUG_REPORTS.md](DAP_BUG_REPORTS.md) | Known DAP behaviour defects, with reproductions. |
| [INIT_SCRIPT_FORMAT.md](INIT_SCRIPT_FORMAT.md) | The debugger-command script format used to set up state before a run. |
| [SINTRAN-SHELL-SPEC.md](SINTRAN-SHELL-SPEC.md) | The `--monitor` SINTRAN shell: command set, and the C compile+link+run workflow. |
| [RUN_500_SH_CONFIG_RECONSTRUCTION.md](RUN_500_SH_CONFIG_RECONSTRUCTION.md) | How the launcher's ini config is put together and where the SINTRAN root points. |
| [PATH_CONVENTIONS.md](PATH_CONVENTIONS.md) | Why no tracked file names a path on a particular machine, and the two forms that are allowed. |

## The CPU

| Document | Covers |
|---|---|
| [instruction-reference/](instruction-reference/) | Per-category functional reference (13 files), built by tracing the ND-5000 microcode and cross-checking the printed manual. Disagreements between the two are called out per instruction. **The highest-confidence instruction documentation here.** |
| [instructions/](instructions/) | Generated per-instruction reference: 241 `asm/*.md` and 241 `yaml/*.yaml`, both produced from `instructions.json`. See its own README for how to regenerate. |
| [ND-500-TRAPS.md](ND-500-TRAPS.md) | The 64 status bits and the 40 defined trap conditions, by category. |
| [ND500_PACKED_BCD.md](ND500_PACKED_BCD.md) | Packed BCD format and the decimal instructions (manual chapter 17). |
| [cpu_implementation_changes.md](cpu_implementation_changes.md) | Corrections made to the emulator's CPU, each anchored to the validation test that caught it. |
| [INSTRUCTION-VALIDATION-REPORT.md](INSTRUCTION-VALIDATION-REPORT.md) | Functional validation audit: operation result, status flags and traps, anchored per source file and line. |

## Vendor manuals

The primary sources. Everything else defers to these.

| Document | Covers |
|---|---|
| ND-05.009.4 EN ND-500 Reference Manual.md | The ND-500 CPU. Cited by every file in `instruction-reference/`. |
| ND-60.113.02 EN Assembler Reference Manual.md | The assembler, including the data-type specifiers. |
| ND-860289-2-EN ND Linker User Guide and Reference Manual.md | The ND Linker and the NRF object format. |

## SINTRAN, MON calls and the vendor toolchain

| Document | Covers |
|---|---|
| [MON_TO_BINARY_PLAN.md](MON_TO_BINARY_PLAN.md) | The route from MON-call coverage to a linked ND-500 binary, and what still gates it. |
| [mon_tests_SCHEMA.md](mon_tests_SCHEMA.md) | The shared MON test spec that `test/mon_tests.json` follows. Marked DRAFT. |
| [NC_TOOLCHAIN_MON_PLAN.md](NC_TOOLCHAIN_MON_PLAN.md) | Running the real NC compiler and ND Linker under emulation: the MON workstream. |
| [NC_INTERACTIVE_COMMANDS.md](NC_INTERACTIVE_COMMANDS.md) | How NC (A06) talks to the terminal, probed live. |
| [NC_CRASH_0x08023EA4_ROOTCAUSE.md](NC_CRASH_0x08023EA4_ROOTCAUSE.md) | The NC codegen crash traced to node use-after-free, with the diagnostic that proved it. |
| [LINKER-LOAD-ERROR52-INVESTIGATION.md](LINKER-LOAD-ERROR52-INVESTIGATION.md) | Linker `LOAD` error 52. **Solved:** DEABF returned a version-less name. |
| [HELP-CRASH-ADVANCED-CMD-REGISTRATION.md](HELP-CRASH-ADVANCED-CMD-REGISTRATION.md) | The HELP crash, plus the two earlier theories that direct measurement disproved. |
| [CONVERT_DOMAIN_FORMAT_AND_USAGE.md](CONVERT_DOMAIN_FORMAT_AND_USAGE.md) | Old vs new domain format, and converting PSEG/DSEG to `:DOM`. |
| [CONVERT_DOMAIN_MON_ANALYSIS.md](CONVERT_DOMAIN_MON_ANALYSIS.md) | Which MON calls CONVERT-DOMAIN makes, observed under nd500x. |
| [VTM-FILE-FORMAT.md](VTM-FILE-FORMAT.md) | Byte-level format of the SINTRAN VTM terminal-table file. |

### Carve findings

Answers established against the byte-verified SINTRAN III segment carve, rather
than inferred from behaviour.

| Document | Covers |
|---|---|
| [CARVE_ANALYSIS_MON60_AND_HANDSHAKE.md](CARVE_ANALYSIS_MON60_AND_HANDSHAKE.md) | The MON 60B / N500M gateway and the CAT-500 handshake. |
| [CARVE-QUESTION-321B-UEADM.md](CARVE-QUESTION-321B-UEADM.md) | MON 321B UEADM sub-function semantics. |
| [CARVE-QUESTION-USER-SYSTEM-FILE-LOOKUP.md](CARVE-QUESTION-USER-SYSTEM-FILE-LOOKUP.md) | **Answered:** an unqualified OPEN tries the own directory, then `(SYSTEM)`, inside SINTRAN's own resolver. |

## NDIX (BSD on the ND-5000)

| Document | Covers |
|---|---|
| [NDIX_FECALL_MON600_SPEC.md](NDIX_FECALL_MON600_SPEC.md) | The front-end call interface implemented as MON 600, which backs the kernel's disk, console and init I/O. |
| [NDIX_SEG30_MICROCODE_ANSWER.md](NDIX_SEG30_MICROCODE_ANSWER.md) | Microcode-side answers on the seg-30 `_Udata` boot blocker. |

## Cross-emulator sync (`SYNC-*`)

Findings that must land identically in the C emulator and the C# RetroCore
emulator. Each states its ground truth and what changed.

| Document | Covers |
|---|---|
| [SYNC-FLOAT-NATIVE-REBASE.md](SYNC-FLOAT-NATIVE-REBASE.md) | Float arithmetic and LOOP rebased onto ND-500 native bias-256, superseding the IEEE-reinterpret assumption. Cited from `src/cpu/instruction_helpers.c`. |
| [SYNC-FLOAT-ARITHMETIC-FIXES.md](SYNC-FLOAT-ARITHMETIC-FIXES.md) | The earlier float fixes. Still stands except where the rebase above supersedes it. |
| [SYNC-STRING-WRONG-INSTRUCTION-FIXES.md](SYNC-STRING-WRONG-INSTRUCTION-FIXES.md) | The STRING wrong-instruction cluster. |
| [SYNC-MON-257B-FOPEN-PRESENT-IN-SINTRAN-L.md](SYNC-MON-257B-FOPEN-PRESENT-IN-SINTRAN-L.md) | MON 257B FOPEN is present in SINTRAN L and must be reported as such. |
