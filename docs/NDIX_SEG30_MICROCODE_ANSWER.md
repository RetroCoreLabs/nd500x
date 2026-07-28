# Microcode-side answers — NDIX seg-30 `_Udata` boot blocker

**Answering:** `NDIX_BOOT_MMU_SEG30_BUGREPORT.md`
**From:** ND-5000 microcode/MMS validation
**Verification basis (authoritative, OBSERVED from source):** the NDIX kernel itself encodes the
ND-500 MMS semantics —
- `kernel/MASTER/machine/locore.c` `__resume` (lines 920-932) — how seg-30 is programmed,
- `kernel/MASTER/machine/pcb.h` (lines 44-98) — the capability bit fields,
- `kernel/MASTER/machine/pte.h` (lines 74-76) — the paging modes.

Facts cited from those files are OBSERVED. Reasoning beyond them is marked INFERRED.

---

## Headline
**Your INFERRED model (bug report §3) is correct.** Seg-30 shares the running process's data
page table via a capability the OS rewrites each context switch — there is **no special hardware
indirection**. The emulator is reading the *correct* capability (`0xA02F`, PSN 47 = icode's data
table). The empty L2 (PFZ2) is therefore **not** a seg-30 routing error; it means the icode data
page was never made resident in the shared table PST[47]. Fix is on the kernel/emulator page-setup
side, not in seg-30 decode. **Two independent emulator decode bugs found** (Q2) — latent here, real
later.

---

## Q1 — How seg-30 follows the *current* process
**OBSERVED (`locore.c:920-932`, `__resume`)** — no hardware indirection; the OS reprograms the
dom0 capability on every resume, into the kernel domain's data-cap table (`r := $kern_dcap`):

```
w2 := b.20 ; w2 or $0x8000        # (incoming process index) | Write-Permit   -> _u  (u-area)
h2 =: r.((_u>>SGSHIFT)*2)
h2 + $0x2001                       # +1 to index, + Shared-Segment bit (0x2000)
h2 =: r.((_Udata>>SGSHIFT)*2)      # -> seg 30 (_Udata) = (p_addr+1) | DC_WRP | DC_SHS
h incr r2 ; h2 =: r.((_Ustack..))  # -> seg 31 (_Ustack) = (p_addr+2) | ...
h incr r2 ; h2 =: r.((_Utext..))   # -> seg 26 (_Utext)  = (p_addr+3) | ...
dctsb                              # flush TSB so the new caps take effect immediately
```

So "seg-30 follows the current process" **only because `__resume` rewrites its capability to
`p->p_addr + 1` each switch**. A process's PST layout is consecutive:
`p_addr`=u-area, **`p_addr+1`=data**, `p_addr+2`=stack, `p_addr+3`=text.

## Q2 — Capability layout of `0xA02F` (a DATA capability, read from DIT+64)
**OBSERVED (`pcb.h:90-97`)** — `pcb_dc[]` bit fields:

| bit | mask | meaning |
|---|---|---|
| 15 | `DC_WRP` 0x8000 | **Write Permitted** (`SG_RW`) — **NOT "indirect"** |
| 14 | `DC_PAC` 0x4000 | Parameter Access |
| 13 | `DC_SHS` 0x2000 | **Shared Segment**, don't use cache |
| 12..0 | `DC_PSN` **0x1FFF** | Physical Segment Number (**13 bits**) |

`0xA02F` = `DC_WRP | DC_SHS | PSN 47`.

**Two emulator bugs:**
1. **PSN mask wrong.** Emulator uses `cap & 0x7FF` (11 bits); the data-capability PSN is
   `DC_PSN = 0x1FFF` (13 bits). Harmless at 47, truncates any PSN ≥ 2048.
2. **Bit 15 mis-decoded.** Emulator reads `0x8000` as "indirect". `0x8000` = indirect **only for a
   *program* capability** (`PC_IND`, `pcb.h:46`, table at DIT+0). For the *data* capability that
   resolves seg-30 (table at DIT+64), `0x8000` = **Write-Permitted** (`DC_WRP`). The program-cap and
   data-cap tables reuse 0x8000 with different meanings.

**Paging mode is NOT in the capability.** `PS_AZI/ASI/ADI` (`pte.h:74-76` = direct/1-level/2-level)
is a property of the **PST[psn] entry**, not of `0xA02F`. "Treat `0xA02F` as PS_ADI" is right only
if **PST[47]** says 2-level — read the mode from PST[47], never from the capability bits.

## Q3 — Walk order & fault status
**OBSERVED:** `PS_ADI` = "double index page" (`pte.h:76`) → the walk `PST[psn] → L1 → L2 → page` is
correct.
**INFERRED / needs the MMS microcode dump to confirm literally:** the exact `MMWHERE` nibble values
(your PFZPST=0xD / PFZ1=0xE / PFZ2=0xF) are the emulator's own scheme — I cannot confirm the literal
ND-5000 codes from the OS source alone. What the OS source *does* imply: the PGF handler branches on
the fault **level** (PST-zero vs L1-zero vs L2-zero) to decide whether to grow the PST entry / the L1
/ the L2, so *distinct codes per level* is the right shape; verify the literal values against the
MMS microcode listing.

## Q4 — Post-fault contract
**OBSERVED:** seg-30's capability carries `DC_SHS` (shared) and its PSN is the running process's data
PST index (`p_addr+1`). So the page **must be installed into PST[47]'s own L1/L2** — the *same* table
the user process uses for its data. Once resident, the kernel's `0xF0000000+off` (seg-30 → PST 47)
and the user's own data read (its data segment → PST 47) resolve to the **identical physical page**.
That is the entire purpose of the shared bit. There is **no** alternate-domain / u-area hardware
pointer to chase.

## Q5 — Is PSN 47 plausible / shared?
**Yes — and it is dynamic.** `47 = p->p_addr + 1`, so icode's `p_addr` = 46 (u=46, **data=47**,
stack=48, text=49). It changes per process; `__resume` recomputes it every switch. `0xA02F` is
*exactly* what `__resume` writes for a process at `p_addr=46` — so **the emulator is reading the
correct, current capability**. PSN 47 is not stale.

---

## Root cause & fix direction
Not seg-30 routing (correct). The empty L2 means **the `"/etc/init"` page was never made resident in
PST[47]'s L2** — when the emulator built `icode` (`icode[]` copyout / `vmemall`), the copy landed in
a *different* table than the shared PST[47] that `__resume` later programs into seg-30. **Ordering
check:** the icode data copyout must populate the **same** PST index (`p_addr+1`) that `__resume`
writes into seg-30. If your copyout used identity translation, or a table built before `p_addr` was
assigned, PST[47] stays empty and the seg-30 read falls through to identity `0xF0000000` → out of RAM
→ `0x00`. **Install icode's data page into PST[p_addr+1]'s L2** and seg-30 will find it.

While you are in `nd500_mmu.c`, also apply the two capability-decode fixes from Q2 (PSN mask
`0x7FF`→`0x1FFF`; bit 15 = Write-Permit, not Indirect, for data capabilities).
