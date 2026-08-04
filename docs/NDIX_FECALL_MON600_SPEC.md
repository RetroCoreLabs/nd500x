# NDIX ND-500 `fecall` / MON 600 Interface Specification

Reference for implementing the ND-100 front-end (fe) call interface as MON 600 (octal, =0x180=384 dec)
in the nd500x emulator, so the NDIX kernel can do disk / console / init I/O backed by
`$NDIX/rootfs.img`.

Kernel source: `$NDIX/kernel/MASTER/`. All facts source-cited.

## 0. Type sizes / struct packing (pcc-nd500 macdefs.h)

`SZCHAR 8, SZSHORT 16, SZINT/SZLONG/SZPOINT 32; ALL alignment = 8 bits (1 byte)` →
**byte-packed, zero padding**. `short`=2, `long`/`int`/pointer/`naddr_t`=4, `char`=1.
`naddr_t = int` = 4 bytes (`h/types.h:46`).

## 1. FE_INIT — struct _init_rpk (response, 320 bytes), `machine/if.h:66-80`

| Field | Type | Off | Notes |
|-------|------|-----|-------|
| completion | short | 0  | 0 = success (nonzero → kernel death()) |
| howto | long | 2 | boot flags |
| rootdev | long | 6 | cast to dev_t(16b) @machdep.c:818; **0 = di0a** |
| condev | short | 10 | console minor dev |
| spst | long | 12 | ND-100 WORD addr (kernel htob's ×2) |
| scont | long | 16 | word addr |
| sndix | long | 20 | word addr |
| stext | naddr_t | 24 | word addr |
| sdata | naddr_t | 28 | word addr |
| sstack | naddr_t | 32 | word addr |
| sfree | naddr_t | 36 | word addr |
| sbuffer | naddr_t | 40 | word addr (NOT transcribed by feinit) |
| sphys | naddr_t | 44 | word addr |
| private | long | 48 | **raw ND-500 BYTES, NOT htob'd**; must be NONZERO |
| cputype | short | 52 | |
| s3_vers | short | 54 | |
| sharedseg | long | 56 | word addr |
| contigno | short | 60 | |
| pageno | short | 62 | |
| booted | char[256] | 64 | boot file name |

Command `_init_cpk` (22B, `if.h:58-64`): ux_vers short@0, cxb0 long@2, trcxb long@6, trdata long@10, intvec ptr@14, trapvec ptr@18.
FE_INIT call: device=0x00000000, request=`QF_SYNC<<16|FE_INIT`=**0x00010001** (machdep.c:120-121). Uses `_feinit_fecall` which SKIPS packet-addr conversion.

Transcription `machdep.c:816-831`: every memory field htob'd (×2) to bytes; `private` stored raw.
- `firstaddr = (sfree_bytes - private)/NBPG` (machdep.c:205), NBPG=2048 → `private ≈ scont_bytes` (kernel contiguous byte base) for a valid low pageframe index.
- `private` MUST be nonzero: `dton` returns 0 when private==0 (locore.c:1475), zeroing all DMA addrs.
- In the DMA round-trip `private` CANCELS (kernel adds it, emulator subtracts it), so any consistent nonzero value works as long as the emulator uses the SAME value it reported.

## 2. Addressing (ND-500 phys byte ↔ ND-100 word)

`htob(x)=x<<1`, `btoh(x)=x>>1` (param.h:52-53). `dton(x)` (locore.c:1471): `private==0 ? 0 : (phyladr(x)+private)/2` = ND-100 WORD addr.
Packet/DMA addresses passed to MON 600 are ND-100 word addresses.
**Emulator invert:** `nd500_phys_byte = word*2 - private`. Physical-mapped kernel VA adds Physbase 0x10000000.
Confirmed by `trap.c:1232`: `errpkt = id_resp_pkt*2 - private + 0x10000000`.

## 3. FE_READ (async) — `io/di.c:415-419`

cpk `_read_cpk_disk` (12B, if.h:217-221): **nbytes long@0, physaddr naddr_t@4, devaddr long@8**.
rpk `_read_rpk_xxxx` (8B, if.h:236-240): **completion short@0, status short@2, nbytes long@4**.
- `physaddr = dton(bp->b_un.b_addr)` (ND-100 word addr of DMA buffer).
- `devaddr = daddr * (DEV_BSIZE/ssize)`; daddr in 1KB blocks; devaddr in `ssize`-byte sectors.
- DEV_BSIZE=1024 (h/param.h:144). ssize = per-disk sector size from FE_OPEN.
- **Disk byte offset in image** = `devaddr * ssize` = `daddr * 1024`.
- **DMA target ND-500 phys byte** = `physaddr*2 - private`.

## 4. ASYNC completion (DECISIVE)

FE_READ/FE_WRIT/FE_DCTL are **async**: `distart` issues the fecall and returns without sleeping;
caller `bread → biowait` sleeps until `B_DONE` (ufs_bio.c:452). `B_DONE` set only by `biodone/iodone`,
called only from `diintr()` (di.c:625), reached only via an ND-100 INTERRUPT:
`_intvec (locore.c:768) → dispatch(idp) (trap.c:602) → (*fr_intr)(sub)=diintr → iodone → B_DONE → wakeup`.

**A synchronous packet-fill+return is NOT enough for disk I/O.** The emulator must, for async FE_READ:
1. DMA data into ND-500 physical memory.
2. Fill rpk: completion=0, status=0, nbytes=count (rpk addr also ND-100 word → invert).
3. **Deliver an interrupt**: present `struct int_descr` (icb.h:16-25) on the IPL queue (`iplp->ip_next`)
   with `id_gen_dev=DISK(1)`, `id_sub_dev=sub`, `id_ipl=IPL_DK(4)`, `id_resp_pkt`=rpk word addr,
   then cause `_intvec`/`dispatch` to run → `diintr` completes the buffer.

**SYNC calls need NO interrupt** (fill rpk + return): FE_INIT, FE_IDEV, FE_OPEN, FE_CLOS, FE_RCON, FE_WCON.

## 5. Request types (`if.h:17-35`), device=`gen<<16|sub`, request=`qual<<16|req`

| Req | Val | Sync | cpk | rpk | login? |
|-----|-----|------|-----|-----|--------|
| FE_INIT | 1 | S | init_cpk 22B | init_rpk 320B | YES |
| FE_IDEV | 2 | S | ipl short@0, pparam long@2 | completion short@0, subdevc short@2 | YES |
| FE_OPEN | 3 | S | format short@0 | completion short@0, devsiz long@2, frmsiz long@6, secsiz short@10 | YES |
| FE_CLOS | 4 | S | dummy int@0 | completion short@0 | no |
| FE_READ | 5 | **A** | nbytes@0, physaddr@4, devaddr@8 | completion@0, status@2, nbytes@4 | YES |
| FE_RCON | 6 | S | physaddr naddr_t@0 | completion short@0 | YES |
| FE_WRIT | 7 | **A** | nbytes@0, physaddr@4, devaddr@8 | completion@0, status@2 | later |
| FE_WCON | 8 | S | physaddr naddr_t@0 | completion short@0 | YES |
| FE_DCTL | 9 | A | operation short@0, format_no short@2 | completion@0, status@2 | no |
| FE_EXIT | 0xb | S | exit_cpk 358B | dummy int@0 | no |
| FE_ERRM | 0xe | — | error_code short@0 | completion short@0 | no |

DISK=1, SIINTR=8, IPL_DK=4. Console device = mx (cdevsw[0]).
rootdev encoding: di major=0, `makedev(x,y)=(x<<8)|y`, `DIUNIT=(minor&0370)>>3`, `PART=minor&07`.
rootdev=0 = di0a; swapdev = minor+1 = di0b.

**Minimum to reach login:** FE_INIT, FE_IDEV, FE_OPEN, FE_READ(+interrupt), FE_RCON, FE_WCON.

## Emulator integration points
- MON dispatch: `src/cpu/nd500_indirect.c:370-490` (mon_dispatch, MonContext w/ read/write_phys_word/byte callbacks).
- MON handlers: `external/ndmonlib/src/handlers/` (one file per MON call).
- Kernel physical load: pseg@phys 0, dseg@0x41a94; Physbase=0x10000000; mmusetup PST@0x84000, DIT@0x90000.
- Need: derive stext/sdata/sstack/sfree/sphys/scont/spst from the emulator's actual physical layout;
  pick a consistent nonzero `private`; implement the interrupt-injection path for async FE_READ.

## 6. IMPLEMENTATION STATUS (2026-07-27)

Implemented in `src/cpu/nd500_fecall.c` (wired via `nd500_indirect.c`: intercept `mon_number==0x180`
before mon_dispatch; added to `src/cpu/CMakeLists.txt`). Env `ND500X_FEDBG` traces calls.
WORKING (sync): FE_INIT (config correct - boot prints full banner "NDIX Release 3", "real mem =
16777216"=16MB, "root on di0", buffers), FE_WCON/FE_RCON console (char is the LOW byte phys+3 of the
big-endian long `cout`/`cin`), FE_IDEV (subdevc=1), FE_OPEN (disk secsiz=512 devsiz=16064), FE_CLOS,
FE_DCTL(stub), FE_ERRM. private=0x2000, rootdev=0(di0a). Memory-layout words = (phys+private)/2 with
scont=0, sfree=0x100000, sphys=memory_size(16MB), spst=0x84000, sdata=0x41a94.
BLOCKED: after FE_OPEN the kernel issues ONE async FE_DCTL then spins forever in the idle loop
(_splx, PC=0x844, SOLO) waiting for the completion INTERRUPT. No FE_READ yet (it comes after). So the
async interrupt path is required to progress. fe_read_disk() DMA+fill is written but interrupt is TODO.

## 7. INTERRUPT MECHANISM (to build - the async completion path)

ND-500 interrupt: on an interrupt the microcode saves the live context into cxbtab[level], sets
IP_CURR, jumps to `_intvec` (raw vector, NOT ENTT). `_intvec` (locore.c:768): reads `_iplp`, sets up
_Kstack, reads IP_NEXT (word addr of int_descr), maps it into the shared seg
(idp_byte = (ip_next_word<<1) - _shseg + _sharebase), calls `dispatch(idp)` -> `(*fr_intr)(sub)` =
`diintr` -> `iodone` -> B_DONE -> wakeup. Returns via `lcntxt $CNTXMASK,cxbtab[old_ipl]` (restore
interrupted context).

Structures (verified, byte offsets):
- `_iplp` = BSS 0x1cb20 (holds POINTER to ipl_rec). ipl_rec: ip_next@0(IP_NEXT), ip_current@4(IP_CURR),
  ip_mask@6(IP_MASK), ip_lock@8(IP_LOCK).
- `_cxbtab` = ABS 0x40000008; cxbtab[ipl] = 0x40000008 + ipl*256. CX offsets (machine/locore.h):
  P@0, L@4, B@8, R@12, I1@16,I2@20,I3@24,I4@28, A1@32..A4@44, E1@48..E4@60, ST1@64, CED@92,
  TRAPNUM@196, BSP@200, TRAPP@204, INFO@208, VADDR@212. CNTXMASK=0x1c3ffff.
- `_intvec` = 0x4ed, `_dispatch` = 0x388b0, IPL_DK = 4, DISK gen = 1.
- int_descr (22B, icb.h): id_next@0(ptr), id_ipl@4, id_s3add@6, id_s3dev@8, id_gen_dev@10,
  id_sub_dev@12, id_flag@14, id_s3func@16, id_resp_pkt@18(naddr_t).

Delivery algorithm (emulator, PENDING - deliver at a safe instruction boundary AFTER the fecall
returns, when IP_CURR < IPL_DK and interrupts enabled):
1. (already done in fe_read/dctl) DMA + fill rpk.
2. Build an int_descr in reserved ND-500 memory: id_ipl=4, id_gen_dev=<gen>, id_sub_dev=<sub>,
   id_resp_pkt=<rpk word addr>.
3. Set _iplp->ip_next = word addr of int_descr (matching _intvec's shared-seg conversion:
   ip_next_word = ((idp_byte - _sharebase + _shseg) >> 1)). NEED _shseg/_sharebase values.
4. Save live CPU context -> cxbtab[IP_CURR] via CX offsets.
5. Set _iplp->IP_CURR = 4; PC = _intvec.
Complications: (a) shared-seg mapping of the int_descr addr (_shseg=htob(rpk.sharedseg)+NBPG per
machdep.c:830, _sharebase=?); (b) pending delivery + level/enable gating in cpu_step; (c) exact
context-save format lcntxt expects (mirror ENTT/lregbl). VERIFY by tracing _intvec/dispatch/diintr.

## 8. STATUS 2026-07-28 - sync + async fecall + interrupt injection all WORKING; boot reaches execve

Sections 6-7 above are the original TODO plan. Both are now IMPLEMENTED in
`src/cpu/nd500_fecall.c` (sync calls fill+return; async FE_READ/FE_WRIT/FE_DCTL + clock deliver
completion interrupts via `nd500_fecall_deliver_interrupt`/`nd500_fecall_tick`, called from cpu.c).
Disk reads are proven correct (superblock + inode + data DMA from the image).

### Disk image selection (IMPORTANT)
- Env var is **`ND500X_DISK`** (`nd500_fecall.c:516`), opened **read-only** (`"rb"`). Default is the
  hardcoded `FE_DISK_PATH` = `$NDIX/rootfs.img` (`:104`). **`NDIX_DISK_IMAGE` is NOT
  read** - older recipes using it silently booted the empty default image.
- Images (di70 FFS, big-endian, 8 MB): `rootfs.img` = empty (only `lost+found`); `rootfs_init.img`
  and `rootfs_mkproto.img` = populated (`/etc/init`, `/dev`, ...). FE_OPEN returns devsiz=1 -> di70
  (nspc=90=blkzero, ssize=1024). Disk byte offset = `(fs_sector + blkzero(90)) * 1024`.

### Correct boot recipe
```
cd $NDIX/kernel/MASTER/GENERIC
ND500X_DISK=$NDIX/rootfs_init.img ND500X_MMU_GUEST_TABLES=1 ND500X_NOXMSG=1 \
  ( printf 'load vmunix\nrun\n'; sleep 45 ) | timeout 70 \
  build/bin/nd500x --debug --sintran-root $NDIX
```
(`ND500X_NOXMSG=1` REQUIRED - else xgattach's synchronous XMSG probe succeeds and proc0 sleeps at the
`0x844` idle forever. Add `ND500X_DOMDBG=1` to SEE domain progress; the `0x844` idle is normal, not a
stall.)

### Current frontier: execve("/etc/init") -> ENOENT even on the populated image
Deterministic (3/3): banner -> `root on di0` -> init launches dom1 (`[DOMRET] RET@0x29`) -> both
`PS_ADI` pagein faults (init stack `0x08000014`, `_Udata` `0xF0000000`) serviced -> `[SYSDBG] enter
_execve I2=0x3B` -> execve returns K/ENOENT (`[DOMRETK] RETK@0x1E`).

Root-cause localization: `[INODEDBG] root(2).i_db[0]=120` - the kernel HAS the correct root inode
(root dir at fs block 120 = raw sector 210 = `img_off 0x34800`, holding `etc`/`dev`). But the only
disk reads are superblock(98)/block-104(194)/inodes(122) - **block 120 is NEVER read**, so
`namei("/etc/init")` fails BEFORE traversing the root dir. Since the inode is correct, `namei` is
almost certainly looking up a garbage/empty pathname: init's `/etc/init` string is in its user DATA
at VA 0, read via the `_Udata` (seg-30, `0xF0000000`) window; the fault is serviced by pagein but the
mapped page appears not to be init's real dcode page -> path reads as 0 -> ENOENT.

NEXT (emulator fix, NO userland build - `/etc/init` already exists in the image + `baseline/etc/init.c`):
dump what the kernel reads at `0xF0000000` during execve/namei (expect `2f 65 74 63 2f 69 6e 69 74 00`
= "/etc/init"; if `00`, the seg-30/`_Udata` page pagein mapped is wrong). Fix the `_Udata` window to
map init's actual user-data page; then `namei` reads block 120, finds `etc`->`init`, and execve loads
it. Probes: `ND500X_SYSDBG`, `ND500X_INODEDBG` (`nd500_fecall.c:306`), `ND500X_DOMDBG`.
