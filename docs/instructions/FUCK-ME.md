# ND500 Assembly Documentation - Complete Rescan

**Scan Date:** 10 Nov 2025 11:29:55

======================================================================
## EXECUTIVE SUMMARY
======================================================================

- **Total .md files in asm/ folder:** 241
- **Total instruction families in test generator:** 150
- **Families WITH assembly docs:** 150
- **Families WITHOUT assembly docs:** 0
- **Documentation coverage:** 100.0%

======================================================================
## SUCCESS: All Families Have Documentation!
======================================================================

**Every instruction family has a corresponding .md file!**

======================================================================
## Families WITH Documentation
======================================================================

**Total documented: 150**

### ARITHMETIC (33 documented)

  - **abs** -> `abs.md`
  - **add2** -> `add2.md`
  - **add3** -> `add3.md`
  - **addc** -> `addc.md`
  - **axi** -> `axi.md`
  - **decr** -> `decr.md`
  - **div2** -> `div2.md`
  - **div3** -> `div3.md`
  - **div4** -> `div4.md`
  - **incr** -> `incr.md`
  - **ixi** -> `ixi.md`
  - **mul2** -> `mul2.md`
  - **mul3** -> `mul3.md`
  - **mul4** -> `mul4.md`
  - **mulad** -> `mulad.md`
  - **neg** -> `neg.md`
  - **padd** -> `padd.md`
  - **paddr** -> `paddr.md`
  - **pmpy** -> `pmpy.md`
  - **pmpyr** -> `pmpyr.md`
  - **ppack** -> `ppack.md`
  - **ppackr** -> `ppackr.md`
  - **psub** -> `psub.md`
  - **psubr** -> `psubr.md`
  - **psum** -> `psum.md`
  - **pupack** -> `pupack.md`
  - **pupackr** -> `pupackr.md`
  - **rem** -> `rem.md`
  - **sub2** -> `sub2.md`
  - **sub3** -> `sub3.md`
  - **subc** -> `subc.md`
  - **udiv** -> `udiv.md`
  - **umul** -> `umul.md`

### BITFIELD (7 documented)

  - **clebi** -> `clebi.md`
  - **getb** -> `getb.md`
  - **getbf** -> `getbf.md`
  - **getbi** -> `getbi.md`
  - **putbf** -> `putbf.md`
  - **putbi** -> `putbi.md`
  - **setbi** -> `setbi.md`

### BRANCH (8 documented)

  - **go** -> `go.md`
  - **ifkgo** -> `ifkgo.md`
  - **ifstgo** -> `ifstgo.md`
  - **jumpg** -> `jumpg.md`
  - **jumps** -> `jumps.md`
  - **loop** -> `loop.md`
  - **loopd** -> `loopd.md`
  - **loopi** -> `loopi.md`

### CALL (18 documented)

  - **call** -> `call.md`
  - **callg** -> `callg.md`
  - **chain** -> `chain.md`
  - **entb** -> `entb.md`
  - **entd** -> `entd.md`
  - **entf** -> `entf.md`
  - **entfn** -> `entfn.md`
  - **entm** -> `entm.md`
  - **ents** -> `ents.md`
  - **entsn** -> `entsn.md`
  - **entt** -> `entt.md`
  - **ifkret** -> `ifkret.md`
  - **ret** -> `ret.md`
  - **retb** -> `retb.md`
  - **retbk** -> `retbk.md`
  - **retd** -> `retd.md`
  - **retk** -> `retk.md`
  - **rett** -> `rett.md`

### COMPARE (5 documented)

  - **comp** -> `comp.md`
  - **comp2** -> `comp2.md`
  - **pcomp** -> `pcomp.md`
  - **scomp** -> `scomp.md`
  - **test** -> `test.md`

### CONTROL (9 documented)

  - **bp** -> `bp.md`
  - **clte** -> `clte.md`
  - **init** -> `init.md`
  - **noop** -> `noop.md`
  - **set1** -> `set1.md`
  - **sete** -> `sete.md`
  - **setk** -> `setk.md`
  - **solo** -> `solo.md`
  - **tset** -> `tset.md`

### IO (1 documented)

  - **riom** -> `riom.md`

### LOGICAL (5 documented)

  - **and** -> `and.md`
  - **inv** -> `inv.md`
  - **invc** -> `invc.md`
  - **or** -> `or.md`
  - **xor** -> `xor.md`

### MOVE (6 documented)

  - **bmove** -> `bmove.md`
  - **clr** -> `clr.md`
  - **clrk** -> `clrk.md`
  - **move** -> `move.md`
  - **stz** -> `stz.md`
  - **swap** -> `swap.md`

### SHIFT (5 documented)

  - **pshift** -> `pshift.md`
  - **pshiftr** -> `pshiftr.md`
  - **sha** -> `sha.md`
  - **shl** -> `shl.md`
  - **shr** -> `shr.md`

### STRING (18 documented)

  - **schpar** -> `schpar.md`
  - **scopa** -> `scopa.md`
  - **scopt** -> `scopt.md`
  - **scotr** -> `scotr.md`
  - **scpuno** -> `scpuno.md`
  - **sfill** -> `sfill.md`
  - **sfilln** -> `sfilln.md`
  - **smatch** -> `smatch.md`
  - **smove** -> `smove.md`
  - **smovn** -> `smovn.md`
  - **smvtr** -> `smvtr.md`
  - **smvtu** -> `smvtu.md`
  - **smvun** -> `smvun.md`
  - **smvwh** -> `smvwh.md`
  - **sscan** -> `sscan.md`
  - **sskip** -> `sskip.md`
  - **sspan** -> `sspan.md`
  - **sspar** -> `sspar.md`

### SYSTEM (35 documented)

  - **bladdr** -> `bladdr.md`
  - **cind** -> `cind.md`
  - **cpgu** -> `cpgu.md`
  - **cwip** -> `cwip.md`
  - **dcc** -> `dcc.md`
  - **dctsb** -> `dctsb.md`
  - **ddirt** -> `ddirt.md`
  - **dmof** -> `dmof.md`
  - **dmon** -> `dmon.md`
  - **freeb** -> `freeb.md`
  - **int** -> `int.md`
  - **intr** -> `intr.md`
  - **laddr** -> `laddr.md`
  - **lcntxt** -> `lcntxt.md`
  - **lind** -> `lind.md`
  - **lregbl** -> `lregbl.md`
  - **pcc** -> `pcc.md`
  - **pctsb** -> `pctsb.md`
  - **phyladr** -> `phyladr.md`
  - **pmof** -> `pmof.md`
  - **pmon** -> `pmon.md`
  - **rdus** -> `rdus.md`
  - **rladdr** -> `rladdr.md`
  - **rpgu** -> `rpgu.md`
  - **rphs** -> `rphs.md`
  - **rwip** -> `rwip.md`
  - **scntxt** -> `scntxt.md`
  - **sloca** -> `sloca.md`
  - **sregbl** -> `sregbl.md`
  - **svers** -> `svers.md`
  - **tutti** -> `tutti.md`
  - **wdus** -> `wdus.md`
  - **wphs** -> `wphs.md`
  - **zpgu** -> `zpgu.md`
  - **zwip** -> `zwip.md`

======================================================================
## Orphaned Documentation Files
======================================================================

**Total orphaned: 91**

These .md files exist but have no corresponding test generator family:

- `a1=_.md`
- `a1_=.md`
- `a2=_.md`
- `a2_=.md`
- `a3=_.md`
- `a3_=.md`
- `a4=_.md`
- `a4_=.md`
- `acos.md`
- `add.md`
- `alog.md`
- `alog10.md`
- `alog2.md`
- `asin.md`
- `assignfrom.md`
- `assignto.md`
- `atan.md`
- `atan2.md`
- `b_assignfrom.md`
- `b_assignto.md`
- `biconv.md`
- `byconr.md`
- `byconv.md`
- `cad=_.md`
- `cad_=.md`
- `ced=_.md`
- `cos.md`
- `cte1=_.md`
- `cte2=_.md`
- `dconv.md`
- `div.md`
- `e1=_.md`
- `e1_=.md`
- `e2=_.md`
- `e2_=.md`
- `e3=_.md`
- `e3_=.md`
- `e4=_.md`
- `e4_=.md`
- `exp.md`
- `fconr.md`
- `fconv.md`
- `hconr.md`
- `hconv.md`
- `hl=_.md`
- `hl_=.md`
- `if-kgo.md`
- `if-stgo.md`
- `if<<=go.md`
- `if<<go.md`
- `if<=go.md`
- `if<go.md`
- `if=go.md`
- `if><go.md`
- `if>=go.md`
- `if>>=go.md`
- `if>>go.md`
- `if>go.md`
- `l=_.md`
- `l_=.md`
- `ll=_.md`
- `ll_=.md`
- `mte1=_.md`
- `mte2=_.md`
- `mul.md`
- `ote1=_.md`
- `ote1_=.md`
- `ote2=_.md`
- `ote2_=.md`
- `p=_.md`
- `poly.md`
- `ps=_.md`
- `ps_=.md`
- `pwconv.md`
- `r_assignfrom.md`
- `r_assignto.md`
- `sin.md`
- `sqrt.md`
- `st1=_.md`
- `st1_=.md`
- `sub.md`
- `tan.md`
- `temm1=_.md`
- `temm2=_.md`
- `tha=_.md`
- `tha_=.md`
- `tos=_.md`
- `tos_=.md`
- `wconr.md`
- `wconv.md`
- `wpconv.md`
