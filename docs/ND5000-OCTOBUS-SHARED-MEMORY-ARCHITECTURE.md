# ND-5000 / Octobus / shared memory - architecture and implementation plan

**Audience:** a future nd500x or nd100x session, and the C# (RetroCore) session.
**Written:** 2026-09-23.
**Status:** research complete, nothing implemented. This document is the agreed
starting point; it is not a record of work done.

**Scope.** Emulating an ND-5000 macro CPU (ND-500 instruction set with the
ND-5000 extensions) coupled to the ND-100/ND-120 over the **Octobus** and the
**shared multifunction-bus memory**. The ND-500 generation's 3022/5015 card pair
is **out of scope** and is not to be built.

**Path convention.** Per `docs/PATH_CONVENTIONS.md`: paths inside this repository
are repo-relative; everything outside is an environment variable
(`NDINSIGHT`, `RETROCORE`, `ND100X`). No machine-specific path appears here.

---

## 0. How to read the evidence grades

Every factual claim below carries a grade. Nothing is stated as fact that was
not read from a manual page, from bytes, or from source.

| Grade | Meaning |
|---|---|
| `[V-MAN]` | Verified in a reference manual. Manual number **and page number** given, with a quoted phrase so it can be found again after any re-conversion. |
| `[V-BYTE]` | Verified against binary bytes by the SINTRAN carve, with the carve document named. |
| `[V-SRC]` | Verified by reading source in this repository, nd100x or RetroCore, with file and line. |
| `[I]` | Inference. The reasoning is given. **Not** a fact. |
| `[U]` | Unknown. No source read has answered it. An experiment is proposed. |

A claim with no grade is an instruction or a proposal, not a finding.

---

## 1. Source register - where each statement can be re-verified

### 1.1 Reference manuals

All under `$NDINSIGHT/Reference-Manuals/`.

**How page numbers are cited, and why there are two of them.** The markdown
transcriptions carry `## Page N` markers that count from the front cover, while
the page number *printed on the paper* starts later. The two differ, and the
offset is not the same in every manual:

| Manual | Measured example | Offset |
|---|---|---|
| ND-05.020.01 | the octobus station table is at marker **T329**; the manual's own index gives it as printed page **315** | T = printed + 14 |
| ND-60.136.04 | `DEFINE-MEMORY-CONFIGURATION` is at marker **T165**; the manual's own contents list gives **152** | T = printed + 13 |
| ND-10.004.01 | section 1.4.1 is at marker **T18**, printed **5**; section 1.4.4 is at marker **T20**, printed **9** | **not constant** - do not assume one |

So: **every citation below gives the transcription marker as `T<n>`**, because
that is what can be found mechanically, plus the printed page where the manual's
own contents list states it. **The quoted phrase is the reliable anchor** - use it
if a re-conversion shifts the markers.

| Short name used below | File |
|---|---|
| **ND-05.020.01** | `500/ND-05.020.01 EN ND-5000 Hardware Description.md` |
| **ND-05.017.01** | `500/ND-05.017.01 EN ND-5000 HARDWARE MAINTENANCE.md` |
| **ND-10.004.01** | `500/ND-10.004.01-MPM 5 Technical Description.md` |
| **ND-10.003.01** | `500/ND-10.003.01 TECHNICAL INTRODUCTION TO MULTIPORT 4.md` |
| **ND-60.136.04** | `ND-60.136.04A ND-500 Loader Monitor.md` |
| **ND-SAMSON-1** | `500/ND-SAMSON-1-EN SAMSON Expected Behaviour.md` |
| **ND-830147-3** | `500/ND-830147-3-EN ND-5000 ES System Administrator Guide.md` |
| **ND-ND5000-08** | `500/ND-ND5000-08-EN ND-5000 Memory Management System.md` |
| **ND-05.022.1** | `500/ND-05.022.1 EN ND-5000 Microprogram Guide.md` |

### 1.2 SINTRAN carve documents

All under `$NDINSIGHT/SINTRAN/`.

| Short name | File |
|---|---|
| **CARVE-DEFMC** | `ND500/CARVE-ANSWER-DEFMC-MPM-BASE-051302-2026-08-10.md` |
| **CC-P2** | `ND500/CC-P2-N500.md` |
| **MP-P2** | `ND500/MP-P2-N500.md` |
| **SWMSG-DOSSIER** | `ND500/N5SWAP-SWMSG-FIELD-DOSSIER-RELAY-2026-08-17.md` |
| **STATUS-INDEX** | `ND500/ND500-STATUS-AND-INDEX.md` |
| **CS-DEBUG-HANDOFF** | `ND500/nd-500-mon/nd-500-control-store-debug-handoff.md` |
| **N500DF-STRUCT** | `OS/N500DF-STRUCTURE-COMPLETE-REFERENCE.md` |
| **DOMAIN-DEEP-DIVE** | `ND500/SINTRAN-DOMAIN-SETUP-DEEP-DIVE.md` |
| **MON-J04** | `ND500/nd-500-mon/nd-500-mon-j04.prog.md` |
| **WHERE-IS-5MPM** | `ND500/WHERE-IS-5MPM-LOCATED.md` - **partly refuted, see section 9.1** |

### 1.3 Source trees

| Short name | Location |
|---|---|
| nd500x | this repository |
| nd100x | `$ND100X` |
| RetroCore | `$RETROCORE` |

---

## 2. The hardware architecture, as the manuals state it

### 2.1 There is ONE shared memory, and the ND-5000 CPU has no memory of its own

`[V-MAN]` **ND-05.020.01 T23**, the ND-5000 CPU's main tasks, first item:

> "Fetch macroinstructions from the **MFbus memory**"

`[V-MAN]` **ND-05.020.01 T40**, section 1.10 Memory:

> "**Shared memory** The ND-120 and the ND-5000 processors can exchange code and
> data in the multifunction bus system, where both processors have access to the
> shared memory. This allows easy access and control by all components of the
> system.
>
> Direct Memory Access (DMA) devices, such as disks and magnetic tape drives,
> can also access the shared memory via the same memory channel as the ND-120,
> or have their own separate ports direct to the shared memory.
>
> **Local memory** In addition to the shared memory, the ND-120 can have its own
> local memory. This memory is part of the SINTRAN address space, but **it cannot
> be reached by ordinary programs in the ND-5000 processor**. The local memory is
> installed in the ND-120 bus."

`[V-MAN]` **ND-05.020.01 T99**:

> "There can be several I/O processors and **several ND-5000s connected to the
> shared memory**. Compared to the existing configuration in the ND-120/ND-500
> systems, the octobus replaces one pair of interface modules for each ND-500 CPU
> that is connected to the ND-120."

**Consequence for the emulator.** "Local memory" in these machines means the
**ND-100's** memory, which the ND-5000 cannot see. The ND-5000 has no private
RAM. Every ND-5000 runs out of one common pool.

### 2.2 Memory capacity - three different numbers that are easily confused

| Number | What it actually is | Grade | Source |
|---|---|---|---|
| **4, 8 or 16 Mbyte** | the capacity of **one MFbus Dynamic RAM card**, card position 4 in the Compact card rack | `[V-MAN]` | ND-05.020.01 **T23**, Table 3: "MFbus Dynamic RAM - 4, 8 or 16 Mbyte Dynamic RAM" |
| **1 Mbyte or 4 Mbyte** | the older MPM-5 Dynamic RAM module, PCB 5411: "1 Mbyte (4 rows with 64 K x 1 bit memory chips)" / "4 Mbyte (4 rows with 256 K x 1 bit memory chips)" | `[V-MAN]` | ND-10.004.01 **T34** |
| **29 address bits addressing 32-bit words** | the MPM-5 memory **channel** address width | `[V-MAN]` | ND-10.004.01 **T13**: "The module accepts one address cable with up to 29 address bits (addressing 32-bit words)." |

**16 MB is the size of a RAM card, not a per-CPU allocation.** A system's shared
memory is the sum of its fitted RAM cards. Nothing in any manual read assigns a
quantity of memory to a CPU.

### 2.3 MFbus is MPM-5 plus the Octobus

`[V-MAN]` **ND-05.020.01 T24**:

> "The multifunction bus (MFbus) system is the follow-up to the MPM-5 system. It
> has several new features, in addition to all the functional features of the
> MPM-5 system. The most important new feature is the Octobus, a serial message
> bus for communication and test purposes."

`[V-MAN]` **ND-05.020.01 T314** (terminology table) states the equivalences
directly: "MPM-5 Bus / Multifunction Bus (MFbus): The MFbus is the same as the
MPM-5 bus"; "MPM-5 Port (324355) / Multifunction Bus Port (324355): The MFbus
Port equals the MPM-5 Port"; "MPM-5 Dynamic RAM / MFbus Dynamic RAM (324158)".

**Consequence.** The MPM-5 technical description (ND-10.004.01) is a valid
reference for MFbus memory behaviour. That is why it is cited throughout.

### 2.4 Where "address zero" comes from - the port, not the CPU

This is the single most important mechanism in this document, and it is the one
that was previously guessed at.

`[V-MAN]` **ND-10.004.01 T18**, section 1.4.1 Address Windows:

> "A memory port is assigned its own address range within the total range of the
> memory system. The addresses specified define the address range that the source
> sees in a memory bank. This is done by setting a lower and an upper address
> limit on the port **with the test and maintenance program on the controller
> module**."

> "When an address is received by the port from the memory channel, the port
> tests the address against its address windows. **The port will respond only if
> the channel address is within the address range of the port.**"

`[V-MAN]` **ND-10.004.01 T20-T21**, section 1.4.4 Address Conversion:

> "The **base** is essential in the address conversion. A bank has its own
> internal physical memory range. To convert the channel address into bank
> address, **a base register is added to the channel address.** … The base is the
> start address, which means the first physical address in the bank, and it has
> an increment of 128 Kbyte."

`[V-MAN]` **ND-10.004.01 T34**, Dynamic RAM module:

> "The lower address limit and the size of the module are supplied from the
> **maintenance processor**."

`[V-MAN]` **ND-10.004.01 T19-T20**: the 5155 port version carries two
16 K x 1 bit window RAMs, "One RAM is used for decoding of LOCAL access, i.e.,
within the MPM-5 bank, and the other RAM is used for decoding of GLOBAL access,
i.e., through a line driver module to other banks." Window resolution is 128 Kbyte
for 32-bit words, 64 Kbyte when the port acts as a 16-bit data port.

**So: a CPU emits addresses starting at 0. Its port decides whether to answer and
which bank cell to reach. No CPU stores or knows its own base. The base and the
windows are set from the MPM cabinet's own service console, before any software
runs.**

`[V-BYTE]` This is independently confirmed from the SINTRAN side. **CARVE-DEFMC**
disassembled the range that was believed to program these registers and found:

> "**The range contains NO `IOX`/`IOXT` instruction at all** - it is four software
> memory-accounting routines (`RLSWM`, `RELAA`, `RELAL`, `PTOS3`). `DEFMC` never
> touches any hardware register, and the MPM-5 port BASE/limit registers are not
> programmable from the ND-100 at all: per the manual they are set only from the
> MPM cabinet's own Test and Maintenance Program console. **No new emulator
> register is needed.**"

### 2.5 Private per-CPU memory: possible in hardware, forbidden by SINTRAN

`[V-MAN]` **ND-10.003.01 T25**:

> "Correct setting of the lower, upper and base limit switches enables the PORT
> and the BUSC to see all or part of the local memory. By introducing the BASE
> switch the local address of the PORT or the ND-100 can be offset by the BASE
> switch setting. This feature is valuable in multiprocessor configurations such
> as the ND-500, **where part of the memory is shared and part of the memory is
> private.**"

`[V-MAN]` **ND-60.136.04 T165**:

> "Note: **Local ND-500 memory is not legal in the ND-500 multiuser Monitor.**"

Both are true at once. The hardware can give a CPU a private region; the SINTRAN
ND-500 Monitor refuses to use it.

### 2.6 The Octobus

`[V-MAN]` **ND-05.020.01 T99**:

> "The octobus is a serial, self-arbitrating bus used to transfer messages between
> **up to 62 devices**. … The octobus is used for messages to initiate operations.
> As a general rule, these operations work on data in shared memory in the MFbus
> system. **Thus, the octobus is normally not used to transport data.** The only
> exception is during debugging and testing."
>
> "The time to send one byte of information over the octobus is **8 µs** when it is
> operated at the maximum speed of **4 MHz**."

`[V-MAN]` **ND-05.020.01 T329** - the station number table. **All values octal.**

| Station no. (octal) | Octobus device |
|---|---|
| 1 | ND-120 CPU |
| 2 - 7 | MFbus controllers |
| 10 - 13 | SCSI controllers (disk) |
| 14 - 15 | Matra VME |
| 16 - 17 | Multifunction communication |
| 20 | Hyperchannel |
| 21 - 23 | FDDI (fibernet) |
| 24 - 27 | FPS-5000 |
| 30 - 33 | Graphic controller |
| 34 - 67 | Free for expansion |
| **70 - 76** | **ND-5000 CPU** |

70B-76B = **56-62 decimal = seven ND-5000 slots.** Station 0 and 77B are not in
the table.

`[V-MAN]` **ND-05.020.01 T330**: global vs local octobus; the local octobus
carries XREQ (request), XCLK (clock), XDAT (transmit), XRFO (refresh).

> "If XRFO is not pulsing, indicating that no MASTER is selected, the stations
> connected to the octobus automatically start to assign a MASTER. **The one with
> the lowest station number ends up as the MASTER** and starts transmitting the
> refresh signal (XRFO)."

Arbitration: "Each requesting station goes on transmitting until it receives a
'1' while transmitting a '0' itself. Then it ceases transmitting, waits until the
current frame is finished, and then starts again. At the time a station gives up,
**its priority is incremented**."

`[V-MAN]` **ND-05.020.01 T331**, frame format: start bit + 30 bits + stop bit,
fields *Priority / Destination / C / B / Source / Information / Parity / Ack*.
Appendix 2 (pages 329-339) is the whole protocol.

`[V-SRC]` RetroCore's fabric implements the destination-to-source rewrite on
delivery - `$RETROCORE/Emulated.HW/ND/CPU/NDBUS/OctobusFabric.cs`, lines 23-32,
which cites the same appendix.

### 2.7 Mutual exclusion lives in the shared memory

`[V-MAN]` **ND-05.020.01 T100**, Chapter 5 The Access Module:

> "In the communication between the ND-120 and ND-5000, shared memory is used for
> busy waiting on semaphores using the **test-and-set** function. This allows
> several processes to share a common device by reserving and freeing a common
> semaphore. In a test-and-set cycle, **the memory is locked between the read and
> write cycles** to guarantee that only one process reserves a free semaphore."

`[V-BYTE]` The SINTRAN side is `SLOCK` / `SUNLOCK`, described in **MP-P2** as
"Critical for multiprocessor systems with multiple ND-500 CPUs".

### 2.8 The Access Module (ACCP) - the ND-5000's octobus front end

`[V-MAN]` **ND-05.020.01 chapter 5**, T97-T149 (printed 83-135). Structure, with
the printed page from the manual's own contents list and the transcription marker:

| Topic | printed | marker |
|---|---|---|
| Functional description, Access Module tasks | 83-84 | T97-T98 |
| Interface between the ND-5000 and the ND-120: Octobus | 85 | T99 |
| Interface between the ND-5000 and the ND-120: Shared Memory | 86 | T100 |
| Interface between the Access Module and the ND-5000 (AIB/AOB, APR/ASR) | 87 | T101 |
| Hardware implementation, interrupt, speed | 89-91 | T103-T105 |
| Device descriptions: ROM/RAM, **Octobus Controller (OCTC)**, FIFO, AIB/AOB, APR/ASR | 92-95 | T106-T109 |
| Octobus messages | 102 | T116 |
| Console monitor | 104 | T118 |
| ACCP command structure | 109 | T123 |
| **ACCP command specifications** | 110 | T124 |

`[V-MAN]` **ND-05.020.01 T101**: the ACCP-to-ND-5000 interface is AIB (input
buffer), AOB (output buffer), APR (parallel register), ASR (serial register).
"The AIB/AOB is connected to the lower 16 bits of the ND-5000 data bus (DB),
while the ASR/APR is connected to the upper 16 bits of DB." The ASR/APR "is also
used during 32-bit access to MFbus memory, i.e. **when loading the control
store**."

`[V-MAN]` **ND-05.020.01 T122**:

> "The ND-120 station number and the OMD number for error messages are given to
> the ACCP by the ND-120 at boot time with the command **LOAD-SYSTEM-PARAMETERS**."

`[V-MAN]` **ND-05.020.01 T123**:

> "The command **LOAD PARAMETER POINTER** is used during initialization to specify
> the location of the parameter area in shared memory. The ND-120 can check that
> the ACCP agrees on this address by writing a 32-bit word in the parameter area
> and sending the command **VERIFY PARAMETER POINTER**."

**The complete ACCP command set**, `[V-MAN]` ND-05.020.01 T126-T149. Markers are
from the section headings in the transcription; every one was located in this
session.

> **The manual's own contents list stops at 5.3.49.** The body carries **eight
> more** commands, 5.3.50 to 5.3.57, which the contents list omits. They are
> included below and are marked `†`. A port that works only from the contents
> list will be missing TERM, ARES and the whole self-test and identification
> group.

| § | Command | marker | § | Command | marker |
|---|---|---|---|---|---|
| 5.3.12 | Echo Test (ECHO) | T126 | 5.3.35 | Load AOB16 (LAOB16) | T139 |
| 5.3.13 | Load System Parameters (LSYSPAR) | T126 | 5.3.36 | Load AOB32 Directly (LAOB32D) | T139 |
| 5.3.14 | ACCP Microtrap (AMICTRAP) | T127 | 5.3.37 | Load AOB32 Via Memory (LAOB32M) | T140 |
| 5.3.15 | Load Parameter Pointer (LPARP) | T128 | 5.3.38 | Read ASTS (RASTS) | T140 |
| 5.3.16 | Verify Parameter Pointer (VPARP) | T128 | 5.3.39 | Load Mode (LMODE) | T140 |
| 5.3.17 | Load Control Store Directly (LOCSD) | T128 | 5.3.40 | Load CON (LCON) | T141 |
| 5.3.18 | Load Control Store Via Memory (LOCSM) | T129 | 5.3.41 | Write Multiport (WMPM) | T141 |
| 5.3.19 | Dump Control Store Directly (DCSD) | T130 | 5.3.42 | Read Multiport (RMPM) | T141 |
| 5.3.20 | Dump Control Store Via Memory (DUCS) | T131 | 5.3.43 | Test Multiport (TESTMPM) | T142 |
| 5.3.21 | Dump Control Cache Directly (DCCD) | T132 | 5.3.44 | Set Trace Selector (SETTRAC) | T142 |
| 5.3.22 | Dump Control Cache Via Memory (DUCC) | T133 | 5.3.45 | Loop (LOOP) | T143 |
| 5.3.23 | **Start Microprogram (STARTMIC)** | T134 | 5.3.46 | Restart Microprogram (RESTMIC) | T143 |
| 5.3.24 | Stop Microprogram (STOPMIC) | T134 | 5.3.47 | Enable Kicks (ENKICK) | T144 |
| 5.3.25 | Continue Microprogram (CONTMIC) | T135 | 5.3.48 | Disable Kicks (DISKICK) | T144 |
| 5.3.26 | **Alive Check (ALIVE)** | T135 | 5.3.49 | Reset CPU (CPURES) | T144 |
| 5.3.27 | Load MAR (LMAR) | T136 | 5.3.50 † | Terminate (TERM) | T145 |
| 5.3.28 | Load MIR (LMIR) | T136 | 5.3.51 † | ACCP Reset (ARES) | T145 |
| 5.3.29 | Read MIR (RMIR) | T136 | 5.3.52 † | Read ECO Levels (RECO) | T146 |
| 5.3.30 | Test Buffer (TBUF) | T137 | 5.3.53 † | Run Self-test (RUNTST) | T147 |
| 5.3.31 | Test Bus (TBUS) | T137 | 5.3.54 † | Read Self-test Status (RTEST) | T148 |
| 5.3.32 | Read AIB16 (RAIB16) | T137 | 5.3.55 † | Set Clock Speed | T148 |
| 5.3.33 | Read AIB32 Directly (RAIB32D) | T138 | 5.3.56 † | Read ACCP PROM Version | T148 |
| 5.3.34 | Read AIB32 Via Memory (RAIB32M) | T138 | 5.3.57 † | Read CPU Model | T149 |

The preceding sections 5.3.1 to 5.3.11 are not commands but context: Octobus
Messages (T116), Console Monitor (T118), Interrupts / AIB flag / AOB flag (T119),
ACCP Messages to Microprogram and back (T120), Memory Error (T121), ACCP Timeout
(T122), ACCP Command Structure (T123), ACCP Command Specifications (T124).
**Read T123-T124 before implementing any command** - they define the frame the
commands sit in.

### 2.9 SAMSON's own bus connections

`[V-MAN]` **ND-SAMSON-1 T3**, block diagram and abbreviation table: the CPU
has **two** multiport channels (`MPM1 -> MPC1`, `MPM2 -> MPC2`) and **two**
octobus interfaces (`OCT1`, `OCT2`). `MPC` = "Multi-port channel", `MPM` =
"Multi-port memory", `OCT` = "Octo-bus interface".

`[U]` Whether both channels and both octobus interfaces are used in a normal
configuration, and what each is for, is **not answered** by anything read. The
SAMSON document is marked by its own authors as unfinished.

### 2.10 The multi-CPU product is the ND-5900

`[V-MAN]` **ND-05.017.01 T131 and T137** (and three further repeats of the
same service instruction):

> "The slot position from the error message can be used to locate the failing
> ND-5000 CPU in the MF crate, in case there is an **ND-5900 system with more than
> one ND-5000 CPU**."

---

## 3. The software configuration, as SINTRAN defines it

### 3.1 DEFINE-MEMORY-CONFIGURATION

`[V-MAN]` **ND-60.136.04 T165**, section 8.10.4 Memory Configuration:

> "In an ND-500 computer system, the processors may be connected to either local
> memory (memory that can be addressed from only one processor) or to a multiport
> memory system (shared memory). **By processor, in this context, is meant the
> disk, the ND-100 CPU, the ND-500 CPU program channel or the ND-500 data
> channel.**"

Two stated restrictions, same page:

> "First, the physical addressing range for program and data memory may not
> overlap if the memory addressed is not the same physical memory.
>
> Secondly, if the disks have access to a memory cell, it is assumed that the
> ND-100 CPU also has access to that memory cell, and vice versa."

> "The ND-500 system has itself limited capability to investigate its own memory
> configuration. Therefore the memory configuration must be defined by the command
> DEFINE-MEMORY-CONFIGURATION."

The command, `[V-MAN]` **ND-60.136.04 T165**:

```
DEFINE-MEMORY-CONFIGURATION <ND-100 page# for ND-500 phys.addr# 0>
```

> "The parameter is ND-100 page number for which the ND-500 physical address is
> zero, i.e. **the difference between the ND-500 and ND-100 physical addresses for
> the same physical cell in common memory.**"

Subcommands, `[V-MAN]` **ND-60.136.04 T166** - asked once **per memory part**:

- size in number of pages for the memory part
- Does ND-100 have access to the part?
- Does ND-500 have access to the part as program?
- Does ND-500 have access to the part as data?
- Is this the last memory part?

> "Default is access for both CPUs, both P and D for ND-500."

Persistence, `[V-MAN]` same page: the definition "is saved and will survive a
normal restart ('warm start')", and "When Sintran III is restarted by the MACM
`)HENT` / `22!` commands ('cold start'), the memory configuration information is
lost. For convenience a permanent macro with the memory configuration definition
should be made."

`[V-MAN]` **ND-830147-3 T85**: on an ND-5000 ES that permanent macro is
`START-ND5000:MODE` - "This file is used for defining the memory configuration of
your ND-5000 system."

`[V-MAN]` **ND-60.136.04 T166**: `MEMORY-CONFIGURATION` prints the current
configuration.

### 3.2 Page ownership

`[V-MAN]` **ND-60.136.04 T166**, section 8.10.5:

> "When the ND-500 is started the first time, **every page of ND-100/ND-500 shared
> memory belongs to ND-100**. Memory is administered through the commands
> GIVE-ND-500-PAGES and TAKE-ND-500-PAGES."
>
> "`GIVE-ND-500-PAGES <no. of pages>` … The specified number of pages are taken
> from the ND-100 and released to the ND-500. If ND-500 already has pages, the
> specified number of pages is added to those ND-500 had previously. **All system
> tables are located in memory belonging to the ND-100.** Thus, the number of pages
> specified will all be available for user processes."

### 3.3 Where the configuration is stored, and the per-CPU question

`[V-BYTE]` **CARVE-DEFMC** section 1. `DEFMC` is MON 60B subfunction 040B,
dispatch table `FUNCS` @142031B entry 040B = 155742B = `DEFMC`. Its value read is:

```
155745: 054721  LDX ,B -57     ; X := the per-CPU N500 datafield pointer
155746: 046060  LDA ,X 60      ; A := N500DF offset 60B = ADRZE / ADRZERO
                               ;      the ND-100 page number where ND-500 address 0 starts
```

The carve grades these separately: the instruction read is `[V-BYTE]` against
`030-S3SM5.bin`; the identification of `B -57` as `N500DF` is **inferred** from
consistent driver-wide usage.

`[V-BYTE]` **CC-P2**, routine `GCPUDF` ("Get CPU datafield of CPU on which process
is running"):

```
GCPUDF: T:=5MBBANK; *AAX 5CPUN; LDATX; AAX -5CPUN   % CPU number from message.5CPUN
     IF A/\377-1>=0 THEN                            % CPU number valid (0 or 1)?
        A*5CPUDFSZ+"S5CPUDF"                        % datafield = base + CPUnum * size
        IF A<<="E5CPUDF" THEN EXITA FI              % within bounds?
     FI
     EXIT                                           % illegal CPU number in message
```

So there is an **array** of CPU datafields, `S5CPUDF` … `E5CPUDF`, stride
`5CPUDFSZ`, indexed by a CPU number that arrives in the mailbox message.

`[V-BYTE]` **SWMSG-DOSSIER**: the message field is `5CPUN = 177772` (offset -6),
graded PROVEN, L07:1543.

`[V-BYTE]` In the carved L07 build the CPU number is validated as **0 or 1** -
two ND-500 CPUs, not four and not the hardware's seven.

`[V-SRC]` **DOMAIN-DEEP-DIVE** quotes a declaration `INTEGER ARRAY C5DF(0:15)
% CPU datafields (one per CPU)` - sixteen slots declared. Which SINTRAN version
this is from is not stated there, so the discrepancy with the 0-or-1 validation
is `[U]`.

`[V-BYTE]` **STATUS-INDEX**: "ADRZERO value = software allocation: `ALLOC`
@171076 -> `SSYSE` store @170705". SINTRAN allocates an ND-100 region and records
the page. It configures no hardware - consistent with section 2.4.

`[V-SRC]` A CPU-numbered monitor command does exist elsewhere: **MON-J04** lists
`SET-CPU-STATUS` at 017541B taking "CPU Number / Image / Save / Status".
`DEFINE-MEMORY-CONFIGURATION` itself takes **no** CPU parameter - the CPU is the
one the issuing process is attached to.

### 3.4 A live capture of a real configuration

`[V-SRC]` **CS-DEBUG-HANDOFF** section 4 records real `MEM-CONF` output:

```
PART      WIDTH        N100   N500P  N500D
  0B      0B-  7777B    Y      Y      Y

                        PAGE          WORD          BYTE
                   ND-100  ND-500   ND-100        ND-500
ND-500 address 0:  004100  000000   00010200000   00000000000
Register block:    004212  000112   00010424000   00000450000
Phys segment tbl:  004252  000152   00010524000   00000650000
WIP/PGU table:     004211  000111   00010422000   00000444000
```

Derived (1 ND-100 page = 1024 words = 2048 bytes):

| Structure | ND-100 page | ND-100 word addr | ND-100 byte addr |
|---|---|---|---|
| ND-500 address 0 | 004100B = 2112 | 0x210000 | 0x420000 |
| WIP/PGU table | 004211B = 2185 | 0x222400 | 0x444800 |
| Register block | 004212B = 2186 | 0x222800 | 0x445000 |
| Phys segment table | 004252B = 2218 | 0x22A400 | 0x454800 |

Every row differs by exactly 004100B. **One part, one offset, one map.**

---

## 4. The question that started this: is ADRZERO per CPU?

**Answer: the storage is per CPU; the value, on all available evidence, is the
same for every CPU; and no CPU knows it.**

Broken down by grade:

1. `[V-BYTE]` The **storage** is per CPU - `DEFMC` writes into a CPU datafield and
   `GCPUDF` indexes an array of them (section 3.3).
2. `[V-MAN]` **No CPU knows where its address zero is.** The port's window and base
   registers decide, and they are set from the MPM service console before software
   runs (section 2.4). `ADRZERO` is the ND-100's own bookkeeping of the resulting
   difference, so ND-100 software can convert between the two numbering schemes.
3. `[I]` **The value is the same for every CPU.** Reasoning: the physical segment
   table sits at ND-500 page 000152B and is read by the swapper and by every CPU's
   microcode (section 3.4). A different `ADRZERO` for CPU 2 would put its page
   000152B on a different physical cell, so the two CPUs could not share one PST -
   which is the point of putting several ND-5000s on one shared memory
   (section 2.1). **This is reasoning from the capture, not a statement found in
   any manual.**
4. `[U]` Whether a real ND-5900 ever gave two CPUs different `ADRZERO` values.

**Experiment to settle item 3** - either is sufficient:

- Dump offset 60B of both CPU datafields (`S5CPUDF` and `S5CPUDF + 5CPUDFSZ`) on a
  running two-CPU SINTRAN and compare.
- Carve request: does any SINTRAN routine ever write a **different** `ADRZERO` into
  a second CPU's datafield, or is `DEFMC` the only writer and always for the
  attached CPU?

**Unresolved offset discrepancy** `[U]`: **CARVE-DEFMC** puts the stored `ADRZERO`
at N500DF offset **60B**. **N500DF-STRUCT** puts `5D12` at offset **41B** and
labels it "MEMDEF: ADRZERO page number", while also calling `5D11`/`5D12`
"Parameter 11/12". The consistent reading is that 41B is the incoming MON 60
parameter slot and 60B is where `DEFMC` stores it, but **that reading is an
inference and is not stated in either document.** Settle it before any C header
names a number.

---

## 5. What already exists - do not rebuild it

### 5.1 nd100x already has the LOCAL-vs-MPM5 ECC probe, and it is faithful

`[V-SRC]` This is the mechanism that was attempted before and believed to have
failed. **It is present and correct.**

| What | Where |
|---|---|
| `NDMemoryType` with the SINTRAN K-symbols: `ND_MEM_MPM5 = 0x04` (KMPM5), `ND_MEM_LOCAL = 0x08` (KMECCR), plus PIOC / MPM3 / MPM4 | `$ND100X/src/cpu/cpu_types.h:330-338` |
| `mms_get_physical_memory_type()` - the classifier | `$ND100X/src/cpu/cpu_mms.c:910` |
| `nd_ecc_write_latch()` - store-on-write, **per word** | `$ND100X/src/cpu/cpu_mms.c:938` |
| `nd_ecc_read_detect()` - detect-on-read, level-14 parity interrupt | `$ND100X/src/cpu/cpu_mms.c:988` |
| ECCR simulate-bit map `0x13 = SimBit0(1<<0) \| SimBit15(1<<1) \| SimBit6(1<<4)` | `$ND100X/src/cpu/cpu_mms.c:936` |

The probe sequence, as the C comment at `$ND100X/src/cpu/cpu_mms.c:929-936`
records it: arm `ECCR = SimBit0 + DisableECC(bit 3)`, **write** the bank (which
must latch the simulated bad ECC even with bit 3 set), clear bit 3, **read** back;
a level-14 parity interrupt means the bank has ECC and is therefore **LOCAL**,
silence means **MPM5**.

Both traps that cost RetroCore a regression are already written into the C:

- **Classify by WORD address, never byte.** `$ND100X/src/cpu/cpu_mms.c:908-912`.
  RetroCore's own post-mortem is at
  `$RETROCORE/Emulated.HW/ND/CPU/ND100/CpuND100.MMS.cs:529-546`: commit
  `b1b08a83e` established the word form, `84a5634c1` regressed it with a
  speculative `<<1`, and local RAM then read back as MPM5.
- **DisableECC (ECCR bit 3) gates detection only, never the write latch.**
  `$ND100X/src/cpu/cpu_mms.c:985-1008`; RetroCore's note at
  `CpuND100.MMS.cs:521-525`.

**What is actually missing** `[V-SRC]`: the classifier is a hard-coded stub with
**no memory behind it**. `$ND100X/src/cpu/cpu_types.h:340-357` defines

```c
#define ND_MPM5_WINDOW_START_WORD  0x00210000u   /* = byte 0x420000 */
#define ND_MPM5_WINDOW_SIZE_WORD   0x00400000u   /* 4 MW = 8 MB     */
```

and its own comment says: "nd100x does not yet back it with a device - it is a
documented STUB used only for TYPE classification."

`[I]` **That is the likely explanation of the earlier failure**: SINTRAN's probe
found a bank that was not LOCAL, mapped its shared window there, and every access
reached nothing. The classification was right; the RAM was absent.

Note that 0x210000 words / 0x420000 bytes is **exactly** the `ND-500 address 0`
row of the live capture in section 3.4 - the stub was set from a real machine.

### 5.2 nd500x is already a library set, already linked into the browser build

`[V-SRC]`

| Target | Defined at |
|---|---|
| `nd500_cpu` | `src/cpu/CMakeLists.txt:47` |
| `nd500_machine` | `src/machine/CMakeLists.txt:2` |
| `nd500_ndlib` | `src/ndlib/CMakeLists.txt:2` |
| `nd500_disasm` | `src/disasm/CMakeLists.txt:2` |
| `nd500_debugger` | `src/debugger/CMakeLists.txt:2` |

`[V-SRC]` `$ND100X/src/frontend/nd100wasm/CMakeLists.txt:80` already links
`nd500_machine nd500_cpu nd500_ndlib`.

`[V-SRC]` nd500x is **not** a git submodule of nd100x. `git submodule status` in
`$ND100X` lists cJSON, libdap, libsymbols, ndmonlib, RetroTermWeb and
norskdata-ndfs only; `.gitmodules` has no `nd500` line. The coupling is a cache
path: `$ND100X/CMakeLists.txt:303` sets `ND100X_ND500X_DIR` defaulting to
`../nd500x`, and `:317` calls `add_subdirectory(... EXCLUDE_FROM_ALL)`.

`[V-SRC]` nd500x has **no** `install()` or `export()` rules anywhere.

### 5.3 The NDIX path in the browser already works and needs none of this

`[V-SRC]` `src/cpu/nd500_fecall.c` (1943 lines) serves MON 600 / `fecall` for
NDIX; `src/cpu/nd500_xring.c` owns the XMSG ring discipline; `src/cpu/nd500_xmsg.c`
is the server that **answers for the absent ND-100**.
`$ND100X/src/frontend/nd100wasm/nd500_wasm.c:25-29` states the current position:
the ND-500 "runs on its own, answering its own fecalls the way nd500x does
natively (front_end = synthetic)".

**This satisfies the browser requirement as it stands.** The browser target is one
ND-5000, NDIX, emulated MON 600, no ND-100, no octobus, no threads. Nothing in
this plan may break it, and no phase below depends on changing it.

### 5.4 nd500x is single-instance today

`[V-SRC]` `src/cpu/nd500_host.h:37-42`:

> "nd500x runs one ND-500 per process, and even in the eventual ND-100 + ND-500
> pairing there is one ND-500 per front end. Revisit only if a second ND-500 ever
> has to exist side by side."

Measured scope of that revisit - mutable file-scope statics:

| File | Count |
|---|---|
| `src/cpu/nd500_fecall.c` | 20 |
| `src/machine/debug_api.c` | 15 |
| `src/cpu/nd500_xmsg.c` | 10 |
| `src/cpu/nd500_page_bits.c` | 7 |
| `src/cpu/nd500_mmu.c` | 6 |
| `src/machine/machine.c`, `src/cpu/cpu.c` | 5 each |
| `src/machine/nd500_ndix_boot.c`, `src/machine/io.c`, `src/cpu/cpu_instr.c` | 4 each |
| `src/cpu/nd500_host_posix.c` | 3 |
| `src/machine/memory_map.c`, `src/cpu/nd500_settings.c` | 2 each |
| `src/cpu/nd500_segment_alloc.c` | 1 |
| **`src/cpu/instructions/**/*.c`** | **0** |
| **Total in `src/cpu/*.c` + `src/machine/*.c`** | **88** |

Plus the module-level host-ops pointer (`src/cpu/nd500_host.h`) and the
`nd500_settings()` singleton (`src/cpu/nd500_settings.h:186`).

**Zero statics across the 242 instruction files** is the important number: the
instruction core is already re-entrant, state lives in `Nd500Cpu*` and
`Nd500Machine*`, and the work is confined to the front-end, MMU-cache and debug
files.

### 5.5 The ND-100's memory representation, and the endianness rule

`[V-SRC]`

| | nd500x | nd100x |
|---|---|---|
| Backing store | `uint8_t* memory` + `memory_size`, `src/machine/machine_types.h:44-45` | `union ndram { unsigned char c_Array[]; uint16_t n_Array[]; }`, `$ND100X/src/cpu/cpu_types.h:267-270` |
| Access | `nd500_bus_read8` / `nd500_bus_write8`, `src/machine/io.c:192` and `:215` | `ReadPhysicalMemory` / `WritePhysicalMemory`, `$ND100X/src/cpu/cpu_mms.c`, wrapped by `PhysMemRead` / `PhysMemWrite` |
| Unit | byte, big-endian by construction | 16-bit word, **host-endian `uint16_t`** |
| Physical address width | - | **24-bit WORD**, `$ND100X/src/cpu/cpu_mms.c:618`: `((ppn << 10) \| dip) & 0xFFFFFF` |
| Installed size | - | 1-16 MB, `$ND100X/src/cpu/cpu.c:195-198`, `--memory=MB` |

`n_Array` is a native `uint16_t` array, so on x86 and under Emscripten the byte
view of ND-100 memory is byte-swapped inside every word relative to the ND-500's
big-endian bytes. **A shared window can therefore never be a plain pointer
alias.** RetroCore names the same lesson at
`$RETROCORE/Emulated.HW/ND/CPU/NDBUS/MpmWindow.cs:19-26` - "one backing array,
two ports".

The ND-100's 32 MB physical ceiling is not a limit on the shared pool. The pool
may be larger than the ND-100 can see; `DEFINE-MEMORY-CONFIGURATION`'s per-part
"Does ND-100 have access to the part?" flag (section 3.1) is exactly the mechanism
for a part the ND-100 cannot reach.

---

## 6. What to learn from RetroCore, file by file

All under `$RETROCORE/`. Line counts measured 2026-09-23.

### 6.1 In scope - the ND-5000 / Octobus generation

| File | Lines | What to take |
|---|---|---|
| `Emulated.HW/ND/CPU/NDBUS/OctobusFabric.cs` | 263 | The bus itself: registration, station numbers, destination-to-source rewrite on delivery. Smallest and most reusable piece. |
| `Emulated.HW/ND/CPU/NDBUS/MpmWindow.cs` | 261 | Bounds-guarded big-endian shared window; the single `SyncRoot` that is the atomicity domain for X5SEM and the NUCLEUS TSET lock. Read the class comment before writing any C. |
| `Emulated.HW/ND/CPU/NDBUS/NDBusOctobus.cs` | 3657 | The ND-100-side octobus card. Register model, FIFO, interrupt, loopback, the ND-100 station adapter. |
| `Emulated.HW/ND/CPU/NDBUS/OctobusND5000Station.cs` | 4327 | The ND-5000 station: mailbox, doorbell, CPU attach. |
| `Emulated.HW/ND/CPU/NDBUS/OctobusND5000Station.Config.cs` | 255 | The configuration contract, and a documented trap - see section 9.2. |
| `Emulated.HW/ND/CPU/NDBUS/AccpOctobusStation.cs` | 182 | The ACCP as an octobus station. |
| `Emulated.HW/ND/CPU/NDBUS/AccpCommandGuards.cs` | 230 | Validation of the ACCP command set of section 2.8. |
| `Emulated.HW/ND/CPU/NDBUS/MpmBackedMicroMemory.cs` | 186 | Control store backed by MPM - the LOCSM path, ND-05.020.01 T115. |
| `Emulated.HW/ND/CPU/NDBUS/Nd5000ControlStoreSink.cs` | 159 | Where a loaded control store lands. |
| `Emulated.HW/ND/CPU/NDBUS/Nd5000AccpAttachment.cs` | 122 | Joining a real ACCP to a station. |
| `Emulated.HW/ND/CPU/NDBUS/BdioEngine.cs`, `BdioRecord.cs`, `BdioRecordScanner.cs` | 540 | Block device I/O over the shared memory. |
| `Emulated.HW/ND/CPU/NDBUS/NucleusClient.cs`, `NucleusStructures.cs` | 281 | NUCLEUS data structures in shared memory. |
| `Emulated.HW/ND/CPU/ND500/Servicer/Nd5000CpuProcessBridge.cs` | 723 | Mailbox message to CPU process control. |
| `Emulated.HW/ND/CPU/ND500/Servicer/N5MailboxProtocol.cs` | 227 | The 5MPM message vocabulary. Explicitly shared by both doorbell generations, so it applies to the octobus path too. |
| `Emulated.Machines/ND/ND100/ND100Machine.ND5000.cs` | - | The machine-level attach recipe, and the honest record of its own limits - see section 6.3. |

### 6.2 Out of scope

| File | Lines | Why |
|---|---|---|
| `Emulated.HW/ND/CPU/NDBUS/NDBusND500IF.cs` | 2897 | PCB 3022, the ND-500 generation. Not to be built. |
| `Emulated.HW/ND/CPU/NDBUS/ND500ControlII.cs` | 293 | PCB 5015. Same. |
| `Emulated.HW/ND/CPU/NDBUS/ND500InterfaceLink.cs` | 65 | The 3022-to-5015 cable. Same. |
| `Emulated.HW/ND/CPU/ND500/Servicer/Nd500CpuProcessBridge.cs` | 3278 | The ND-500-generation bridge. The ND-5000 twin (6.1) is the one to port. |

`NDBusND500IF.cs` still holds two constants the octobus path reuses -
`DEFAULT_SHARED_MEMORY_START` and `DEFAULT_SHARED_MEMORY_SIZE`. Copy the values,
not the file.

### 6.3 What RetroCore has NOT solved

`[V-SRC]` `$RETROCORE/Emulated.Machines/ND/ND100/ND100Machine.ND5000.cs:37-43`:

> "PHASE-2 LIMITATION (recorded, not solved here): NDBusOctobus keeps a SINGLE
> `_nd5000Station` field and `AttachCpu` replaces it, so this one-call path
> supports ONE ND-5000 CPU. Multiple CPUs (stations 70B-73B) will register the
> extra stations directly on the shared fabric (octobus.RegisterStation /
> octobus.Fabric) instead of the card's single slot."

The slot dictionary `_nd5000Cpus` is keyed 1..4 and `ApplyNd500Identity()` loops
1..4, so the intent is present and the plumbing is not. **Multi-CPU is new design
work in both emulators, not a port.**

### 6.4 The ND-100-side octobus card registers are NOT in the manual

`[V-MAN]` A search of ND-05.020.01 for `IOX`, `IOXT`, `IOCT` and `100400` returns
**nothing**. The manual documents the ACCP's Octobus Controller (T106, printed 92) but not
the ND-120's own card registers.

`[V-SRC]` RetroCore derived them from a SINTRAN symbol file:
`$RETROCORE/Emulated.HW/ND/CPU/NDBUS/NDBusOctobus.cs:2120-2127` - "Source:
s3vs-4.symb shows IOCT0 at 100400, OOCT0 at 100400+4", 8 registers, interrupt
level 13, and the card identified as PCB 3109 (octobus + MPM on one card).

`[U]` **The provenance of the PCB 3109 identification and of the full register
semantics is not established by any manual read here.** Before the C card is
written, either locate the ND-100-side octobus interface manual or raise a carve
request against the SINTRAN driver.

`[V-SRC]` **The address width is NOT a problem.** IOX 100400 octal = 33024 decimal
needs more than the 11 bits that `$ND100X/src/devices/devices_types.h:187`
documents for a plain `IOX`, so the card sits in the **extended `IOXT`** space -
and nd100x already reaches it, end to end, with nothing to change:

| Step | Evidence |
|---|---|
| `IOXT` (opcode 150415 octal) implemented and registered | `$ND100X/src/cpu/cpu_instr.c:4481` and `:7586` |
| it passes the **full 16-bit T register**, unmasked | `cpu_instr.c:4492` - `gA = io_op(gT, gA)` |
| only plain `IOX` masks to 11 bits, correct for that instruction | `cpu_instr.c:4557` - `io_op(operand & 0x07ff, gA)` |
| `io_op` applies no mask | `$ND100X/src/machine/io.c:82` |
| `io_read` / `io_write` take `uint32_t` | `$ND100X/src/machine/io.c:55`, `:60` |
| `devmgr_read(uint32_t address)` walks devices on the full address | `$ND100X/src/devices/devicemanager.c:395` |
| `Device.startAddress` and the `Read`/`Write` hooks are `uint32_t` | `$ND100X/src/devices/devices_types.h:154`, `:176-177` |

So the octobus card registers itself like any other device; only its register
semantics (above) are still unknown.

---

## 7. Target architecture

### 7.1 Shape

```
  nd100x native frontend                      nd100wasm (browser)
  spawns one host thread per ND-5000          no threads, no octobus, no ND-100
        |                                             |
  +-----v---------------------------------------------v-----+
  |  ndbus - the new code                                    |
  |    octobus fabric (frames, 62 stations, arbitration)     |
  |    ND-5000 station (mailbox, doorbell, X5SEM)            |
  |    ACCP command layer (the ACCP command set of section 2.8)   |
  |    shared-memory pool + big-endian accessors + lock      |
  |    ND-100 memory bank table (base page, size, type)      |
  +--------+---------------------------------+--------------+
           | ndbus_host_ops                  | ndbus_cpu_ops
           v                                 v
  nd100x                                nd500x
    octobus IOX device                    Nd500Machine.memory points INTO
    bank-table hook in                    the shared pool; runs its own loop
    mms_get_physical_memory_type()
```

### 7.2 Five rules, each traceable to a finding above

1. **One shared-memory object per machine.** Every ND-5000's
   `Nd500Machine.memory` points into it at offset 0. No per-CPU RAM, no copying.
   *From section 2.1.*
2. **The ND-100 reaches it only through big-endian byte-pair accessors**, never by
   aliasing `n_Array`. *From section 5.5.*
3. **Threads live in the native frontend only.** The library must be correct with
   zero threads and safe with several. Locking goes behind a two-function shim
   (`ndbus_lock` / `ndbus_unlock`) that is a real mutex natively and empty under
   `__EMSCRIPTEN__`. *From section 5.3 - the browser build has no `-pthread`.*
4. **One lock is the atomicity domain for the whole shared pool**, taken inside
   the accessors, covering test-and-set and every cell that races it. *From
   section 2.7 and RetroCore's `MpmWindow.SyncRoot`.*
5. **No port base or window registers are modelled.** They are set from a service
   console that does not exist here; the window base is machine configuration.
   *From section 2.4 and CARVE-DEFMC's explicit conclusion.*

### 7.3 The bank table - the change that also fixes the LOCAL probe

Replace the two hard-coded `if` statements in
`$ND100X/src/cpu/cpu_mms.c:910` with a small registered table:

```c
typedef struct {
    uint32_t     start_word;   /* ND-100 physical WORD address (never byte) */
    uint32_t     length_word;
    NDMemoryType type;         /* ND_MEM_LOCAL | ND_MEM_MPM5 | ... */
    /* backing accessor; NULL = plain local RAM */
} NdMemoryBank;
```

Local RAM registers itself as `ND_MEM_LOCAL`. The shared pool registers as
`ND_MEM_MPM5` at the configured base page. `ND_MPM5_WINDOW_START_WORD` and
`ND_MPM5_WINDOW_SIZE_WORD` are then retired.

The existing ECC code needs no change: `nd_ecc_write_latch()` and
`nd_ecc_read_detect()` already call `mms_get_physical_memory_type()` at
`$ND100X/src/cpu/cpu_mms.c:971` and `:1011`. **This one change turns the probe
from a stub into a working LOCAL/MPM5 answer.** Keep the word-address rule and the
DisableECC rule of section 5.1 intact - they are both regression-proven.

### 7.4 Where the code lives

**Recommendation: start it as `src/ndbus/` inside this repository**, under a hard
rule that nothing in it includes an `Nd500Machine` or an ND-100 type - it reaches
both CPUs only through the two vtables it defines. Lift it into its own repository
once its own tests are green, following the `ndmonlib` precedent
(`$ND100X/external/ndmonlib`, a submodule of both emulators).

Rationale: the coupling depends on both machines by construction. Putting it in
nd100x makes the ND-5000 half untestable without a whole ND-100; putting it in
nd500x with direct includes inverts the dependency. The vtable rule gives the
correct structure immediately and defers only the repository cost.

Also do, in phase 0: add `install()` / `export()` and `nd500::` alias targets to
nd500x, and make nd500x a real submodule at `$ND100X/external/nd500x` with
`ND100X_ND500X_DIR` retained as a developer override. This removes the
header-collision hazard already documented at
`$ND100X/src/frontend/nd100wasm/nd500_wasm.c:53-64`.

### 7.5 The concurrency model in concrete C

Section 2.7 and rule 4 above say ordinary accesses take no lock. **There is no
multiport memory here - there is a `uint8_t *` and a set of host threads - so
"the hardware arbitrates" is not an implementation.** This is the implementation.

#### 7.5.1 What the host gives for free, and what it does not

**Free, no code:**

- **The host's cache-coherence fabric IS the multiport memory's arbitration.** A
  store by thread A becomes visible to thread B with nothing written by us. That
  is the whole of what the MPM's per-cycle arbitration provided.
- **Naturally-aligned 8/16/32/64-bit accesses are single-copy atomic** on x86-64
  and AArch64 - no torn values at the machine level.

**NOT free. Exactly four things need code, and only four:**

1. the **compiler**, which may tear, fuse, duplicate or invent accesses on a plain
   `uint8_t*` shared between threads, because C11 makes a data race undefined;
2. **read-modify-write** (`TSET`);
3. **ordering** on the doorbell handshake;
4. **cross-CPU TLB invalidation.**

#### 7.5.2 The pool and its accessors - relaxed atomics, zero instruction cost

Keep the backing store a plain `uint8_t *pool` so bulk paths (image load, DMA,
`memcpy`) still work, and route every **guest-visible scalar** access through:

```c
static inline uint8_t ndbus_pool_r8(const uint8_t *p)
{
    return __atomic_load_n(p, __ATOMIC_RELAXED);
}

static inline void ndbus_pool_w8(uint8_t *p, uint8_t v)
{
    __atomic_store_n(p, v, __ATOMIC_RELAXED);
}
```

The GCC/Clang `__atomic_*` builtins take **plain** pointers - the array need not be
`_Atomic`-qualified. On x86-64 and AArch64 they emit exactly the same `movzbl` /
`movb` (`ldrb` / `strb`) as a plain dereference. **Defined behaviour, identical
machine code, no lock.** QEMU solves the same problem the same way for
multi-threaded TCG guest RAM.

**Do not declare the array `_Atomic unsigned char[]`** - that makes `memcpy` and
every bulk transfer ill-formed and buys nothing over the builtins.

nd500x has only 12 direct `memory[` dereferences, in `src/machine/io.c`,
`src/cpu/instructions/CALL/Chain.c` and `src/cpu/instructions/IO/Riom.c`, so
routing everything through accessors is a small change.

#### 7.5.3 Multi-byte guest accesses - tearing here is CORRECT

The ND-500 is byte-addressed, so a 32-bit reference can land on an odd byte. That
is not single-copy atomic on any host - **and it is not on the real machine
either.** `[V-MAN]` ND-10.004.01 T21-T22: the MPM is 32 bits wide with interleave,
so an unaligned reference is more than one memory cycle and another port can land
between them.

**Build multi-byte values out of the byte accessors and let them tear. Do not add a
lock to prevent it** - a lock there would be less faithful, not more.

#### 7.5.4 TSET - the only real read-modify-write

`[V-SRC]` `src/cpu/instructions/CONTROL/Tset.c`, opcode `0xFD40`: **32-bit (W)
operand**, writes `0xFFFFFFFF`, sets `Z = (old == 0)`. That is the entire
primitive, and the only place mutual exclusion is genuinely required.

**Use one mutex for all LOCK cycles - not one per address, one globally.**

- **Faithful:** the real bank serializes every LOCK cycle at one point too
  (ND-05.020.01 T100, "the memory is locked between the read and write cycles").
- **Correct regardless of alignment and byte order.** The alternative,
  `__atomic_exchange_n` on a 32-bit view of the pool, requires the guest's operand
  to be naturally aligned (it need not be) and requires the expected and desired
  values byte-swapped, because the pool holds big-endian bytes on a little-endian
  host. More code, more ways to be wrong, no measurable gain.
- **Cheap:** TSET is rare next to ordinary accesses and an uncontended mutex is
  tens of nanoseconds.

**The mailbox and doorbell operations take the same mutex**, because they must be
mutually exclusive with the semaphores that guard them - RetroCore's "half a lock
is no lock" (`$RETROCORE/Emulated.HW/ND/CPU/NDBUS/MpmWindow.cs:26-33`).

**A spinning guest is an emulator problem with no hardware analogue.** The guest's
acquire loop is `TSET` / branch / `TSET` / … at emulated speed and burns a whole
host core. If a CPU's TSET finds the cell held **and** it is the same address that
CPU failed on last time, count it; past a small threshold, `sched_yield()`. Without
that, several spinning threads can starve the thread that holds the lock.

#### 7.5.5 Doorbell ordering - release/acquire, and only here

A CPU fills a message in the pool, then rings the doorbell. The reader must not see
the doorbell before the message.

- writer: message bytes `__ATOMIC_RELAXED`, then the doorbell cell
  **`__ATOMIC_RELEASE`**;
- reader: doorbell cell **`__ATOMIC_ACQUIRE`**, then the message bytes relaxed.

On x86-64's TSO model the relaxed version happens to work. **Do not rely on it** -
AArch64 and the browser will reorder it, and the failure mode is an occasionally
half-written message, which presents as "the ND-5000 hung" with nothing in any log.

#### 7.5.6 Cross-CPU TLB invalidation - the one genuinely new mechanism

`[V-SRC]` Today `src/cpu/nd500_tlb.h:81-95` keeps `g_nd500_xlat_bm`, a bitmap of
physical pages a translation walk read from, and `nd500_tlb_on_phys_write()` -
called from `src/machine/io.c:278` on **every** physical byte written - flushes the
whole TLB when a write lands on one. With one CPU that is a correct crutch. With
several it is broken: CPU 1 edits a page table and only CPU 1's cache is flushed.

- **`g_nd500_xlat_bm` stays shared** - it describes the shared memory's role, not a
  CPU's. Set bits with `__atomic_fetch_or(&bm[i], mask, __ATOMIC_RELAXED)`. Bits
  are set rarely (on a walk) and read constantly (on every write), which is the
  right way round.
- **`g_nd500_tlb[]`, `g_nd500_tlb_on`, `g_nd500_tlb_init` become per CPU**
  (`src/cpu/nd500_tlb.h:50-64`). Several CPUs sharing one translation cache is a
  wrong-answer bug before it is a performance bug.
- On a hit in `nd500_tlb_on_phys_write()`, set `flush_pending = 1` (relaxed store)
  in **every attached CPU's struct, including the writer's**. Do not flush inline.
- **Each CPU tests its own `flush_pending` once per instruction**, at the top of
  its step loop - never per memory access - and flushes if set.
- **Put each CPU's `flush_pending` on its own 64-byte cache line**, or the threads
  ping-pong one line on every page-table write and that costs more than the flush.

Once-per-instruction is safe, and it is worth recording why: it is already far
stricter than the real machine, where a stale translation after another CPU edits a
page table is the guest's problem, handled by the guest issuing `DCTSB` / `PCTSB`.
This crutch exists because nd500x chose not to trust the guest to do that; making
it per-instruction rather than per-access loses nothing.

#### 7.5.7 False sharing

Pad per-CPU structs to a cache line. The **pool itself** will false-share between
CPUs, and that is exactly what shared memory does on the real machine - leave it.

#### 7.5.8 What is deliberately NOT modelled

Per-port base and window registers, bus arbitration order and priority, and MPM
interleave timing. **No instruction the guest can execute observes any of them**,
and two of the three are service-console settings no software ever touches
(section 2.4).

#### 7.5.9 Summary

> The host's coherence fabric replaces the multiport memory's arbitration; relaxed
> atomics replace only the guarantees the C standard withholds, at no instruction
> cost; and exactly three things get real synchronization - **TSET, the doorbell,
> and the TLB shootdown**. Ordinary loads and stores get nothing, on either side,
> because the hardware gave them nothing.

Do not add per-page or striped locks. They put real cost on the hot path to solve a
problem that does not exist.

---

## 8. Configuration - per ND-5000 CPU, in the nd100x .ini

### 8.1 What exists now

`[V-SRC]` nd100x already parses a **single, unnumbered** `[nd500]` section:

| Where | What |
|---|---|
| `$ND100X/src/machine/machine_config.c:562-568` | naming `[nd500]` is what enables it |
| `$ND100X/src/machine/machine_config.c:920-990` | keys: `enabled`, `memory` (1-64 MB), `kernel`, `pseg`, `dseg`, `disk0`..`disk15` |
| `$ND100X/src/machine/machine_config.h:127-140` | `McNd500` struct, `MC_ND500_MAX_DISKS 16` |
| `$ND100X/src/machine/machine_config.c:1564-1593` | the writer |
| `$ND100X/src/machine/machine_config_json.c:257-274` | the JSON export |

It carries no station number, no base page, no memory parts, and there can only
be one.

### 8.2 Proposed schema

Follow the numbered-section pattern already used for controllers
(`[controller.<type>.<wheel>]`, parsed at
`$ND100X/src/machine/machine_config.c:569-600`).

```ini
# ---------------------------------------------------------------------
# Shared multifunction-bus memory. ONE pool for the whole machine.
# Every ND-5000 runs out of this; the ND-100 sees it as MPM5 memory.
#   ND-05.020.01 T40 and T99.
# ---------------------------------------------------------------------
[mfbus]
enabled   = yes
; Total pool, megabytes. A real system is the sum of its RAM cards,
; each 4, 8 or 16 MB - ND-05.020.01 T23.
size      = 16
; The ND-100 PAGE at which ND-500 physical address 0 appears. This is
; exactly the parameter of DEFINE-MEMORY-CONFIGURATION - ND-60.136.04
; T165 (printed 152). Octal accepted with a trailing B.
; 004100B = 2112 = ND-100 byte 0x420000 (live capture, section 3.4).
base_page = 004100B

; Memory parts, in order, mirroring the DEFINE-MEMORY-CONFIGURATION
; subcommands - ND-60.136.04 T166. Default is access for all.
; A part with nd100 = no is memory the ND-100 cannot reach, which is how
; a pool larger than the ND-100's 32 MB view is expressed.
[mfbus.part.0]
pages     = 4096
nd100     = yes
nd500_p   = yes
nd500_d   = yes

; ---------------------------------------------------------------------
; Octobus controller in the ND-100. The ND-100 is always station 1B.
;   ND-05.020.01 T329.
; ---------------------------------------------------------------------
[controller.octobus.0]
enabled   = yes

; ---------------------------------------------------------------------
; ND-5000 CPUs. Station numbers 70B-76B - ND-05.020.01 T329.
; Section number is the CPU slot; the station is explicit so a
; configuration reads the way the hardware is wired.
; ---------------------------------------------------------------------
[nd5000.1]
enabled      = yes
station      = 070B
; Per-CPU ND-100 page for this CPU's ND-500 address 0. Stored per CPU
; because SINTRAN stores it per CPU (section 3.3); on all evidence the
; value is the same for every CPU (section 4), so omitting it inherits
; [mfbus] base_page. Do not set it differently without the experiment
; of section 4 first.
; base_page  = 004100B
cpu_type     = 5000
; NDIX / standalone boot material, as the current [nd500] section takes.
kernel       =
pseg         =
dseg         =
disk0        =

[nd5000.2]
enabled      = no
station      = 071B
```

### 8.3 Rules for the parser

- `[nd5000.<n>]`, n = 1..7, matching the seven hardware slots of section 2.6.
- Duplicate `n`, or two CPUs on the same `station`, is an error, the same way a
  duplicate controller+thumbwheel is at
  `$ND100X/src/machine/machine_config.c:579-592`.
- `station` outside 070B..076B is an error.
- A section with no `station` gets 070B + (n-1).
- The existing `[nd500]` section stays and keeps working as an alias for
  `[nd5000.1]` with no octobus, so no existing .ini file breaks.
- `base_page` accepts octal with a trailing `B` and decimal without, because every
  page number in every manual and in `MEM-CONF` output is octal.
- Everything reaches the JSON export at
  `$ND100X/src/machine/machine_config_json.c` so the browser sees the same schema.

---

## 9. Traps, and claims that are refuted

Recording these so no future session re-derives them.

### 9.1 SINTRAN does NOT program the MPM port registers

`$NDINSIGHT/SINTRAN/ND500/WHERE-IS-5MPM-LOCATED.md`, section "SINTRAN's Role",
claims SINTRAN "Configures MPM5 Port Modules (via register writes to 3022 card) -
sets address windows … sets BASE registers".

**Refuted** by CARVE-DEFMC (section 2.4): `DEFMC` contains no IOX instruction at
all, and the port base and limit registers are not reachable from the ND-100.
The rest of that document is sound; do not build from that section.

### 9.2 The doorbell self-discovery sniff threshold is a trap

`[V-SRC]` `$RETROCORE/Emulated.HW/ND/CPU/NDBUS/OctobusND5000Station.Config.cs:88-104`.
The X5ACT sniff keys on a `0xFFFF -> 0` transition. Per the byte-verified
microcode reference, `XMSINIT` initialises X5ACT to -1 but the microcode re-arms
it by writing **1**, not -1 (microword `0o24722`'s literal 1 at `IDLE_2`). So the
genuine doorbell produces exactly **one** -1 to 0 transition per `XMSINIT`. A
repeat threshold of 2 or more can never be met, the sniff never latches, and
**nothing errors - the machine simply sits looking idle.** Port the warning along
with the setting.

### 9.3 Station numbers are octal, and 70 does not fit in six bits

`[V-SRC]` `$RETROCORE/Emulated.HW/ND/CPU/NDBUS/NDBusOctobus.cs:22-30` records that
an earlier version used the octal digits as decimal literals (`ND5000_CPU = 70`),
which does not fit the 6-bit station field. Decimal 56 is correct for 70B. Write
octal literals as `070` or `0x38` in C, never `70`.

### 9.4 Three memory sizes that are easy to confuse

Section 2.2. 16 MB is a RAM card. 1/4 MB is an MPM-5 RAM module. 2 GB is the
channel. None of them is a per-CPU allocation.

### 9.5 The ND-100's 32 MB ceiling is not a limit on the pool

Section 5.5. The per-part `nd100` access flag exists precisely so a pool can be
larger than the ND-100's view.

---

## 10. Plan - numbered work, one CPU first

### 10.0 Status, 24-SEP-2026

Everything that could be built without new evidence or a decision from Ronny is
built and green: **nd100x 20/20, nd500x 43/43**, `test_ndbus` at 307 checks,
ThreadSanitizer clean, and the WASM frontend builds (`nd100wasm.wasm`).

What a machine can do today, from an `.ini`:

- allocate ONE shared MFbus pool and register it as an `ND_MEM_MPM5` bank at a
  configured ND-100 page, with local RAM classified `ND_MEM_LOCAL` from a real
  bank table rather than a hard-coded window;
- put the ND-100's octobus card at 100400 and up to seven ND-5000 stations at
  070B..076B on the bus;
- carry the ACCP bring-up sequence over the octobus - ECHO, LSYSPAR, LPARP,
  VPARP, STARTMIC, STOPMIC, CPURES - with the real firmware-measured guard table;
- hold the mailbox in shared memory, with CPUNO 1-based off a global header and
  the asymmetric X5ACT doorbell: SINTRAN's ACT51 rings by writing 0 and sends no
  kick, the microcode IDLE loop polls and re-arms with 1;
- give each station an `Nd500Machine` whose memory IS the pool, on its own host
  thread, with a clean stop-and-join.

**WHAT REMAINS, and why none of it is code that can simply be written:**

| # | Item | Why it is not done |
|---|---|---|
| 1.9 | Boot SINTRAN and confirm `MEMORY-CONFIGURATION` reports LOCAL | Parked at Ronny's request. Blocks nothing |
| 3.3a | `nd500_xmsg.c` statics | Group 2 by the classification in phase 3: no effect until two NDIX guests run in one process, which nothing asks for. Its public API is called from six files including both frontends |
| ~~4.5~~ | ~~The mailbox layout~~ | **DONE - the earlier "no evidence" was wrong.** The layout was never searched for properly. It is recorded in `$RETROCORE/Emulated.HW/ND/CPU/NDBUS/OctobusND5000Station.cs`, cited to NPL source plus the microcode: a global header (X5SEM word 0, X5HEN 3, X5FYL 4, X5MXF 5, X5FIF ring base 6-7) and per-CPU extension blocks at a 200B-word (256-byte) stride holding X5BEX 0-1, X5ACT 5, X5PRO 6, X5CLR 10B, X5CCL 11B. Built as `src/ndbus/ndbus_mailbox.{h,c}` |
| 8 | Golden trace replay | Needs octobus and mailbox traces exported from RetroCore, which is a C# test run on Ronny's side |
| - | Loading a program into an ND-5000 | The CPU runs, but nothing puts code in the pool for it yet. The path exists - `ndbus_pool_write_bytes()` - and what to load is a decision, not a gap |
| - | Committing and pushing | Ronny's call. Until nd500x is pushed, the `external/nd500x` submodule is pinned at a commit without `src/ndbus/`, which is why the sibling checkout is still the CMake default |



Every item below is **remaining work**. Each phase ends with something that runs
and is covered by tests.

**Three standing constraints, true in every phase:**

- **The browser NDIX path of section 5.3 must keep working.** One ND-5000, MON 600,
  no ND-100, no octobus, no threads. It is the regression canary: build
  `nd100wasm` and boot NDIX at the end of every phase.
- **No `pthread_create` outside the native frontend.** The bus code is correct with
  zero threads and safe with several (section 7.5).
- **Nothing in `src/ndbus/` includes an `Nd500Machine` or an ND-100 type.**

**One open item blocks phase 4 and nothing else** (section 12): the ND-100 octobus
card register provenance (#3). **Raise that carve request at the start of phase 1**,
so the answer is back before phase 4 needs it. It is the only item on this plan that
cannot be resolved by editing code.

---

### Phase 0 - packaging (~half a day remaining)

| # | Task | Where |
|---|---|---|

**`install()` / `export()` rules are deliberately NOT part of this.** nd100x consumes
nd500x with `add_subdirectory()`, not `find_package()`, so nothing installs it and
export rules would be speculative. They also force every
`target_include_directories()` in this repository to be rewritten with
`$<BUILD_INTERFACE:>` generator expressions, or `install(EXPORT)` refuses the
targets - a real chance of breaking the nested and WASM builds for no current
consumer. Add them when something actually needs `find_package(nd500x)`.

**Exit:** native and `nd100wasm` both build; NDIX still boots in the browser; no
behaviour changed.

---

### Phase 1 - the ND-100 memory bank table (nd100x)

All code tasks are written and `ctest -R memory_bank` is green in `$ND100X/build`
(18/18 overall). What REMAINS in this phase:

| # | Task | Where |
|---|---|---|
| 1.9 | Boot SINTRAN and confirm `MEMORY-CONFIGURATION` reports local RAM as LOCAL from the registered banks. **Parked at Ronny's request** until he is back; it blocks nothing | runtime |

Written in this phase (records, not work): `NdMemoryBank` plus
`mms_memory_bank_register` / `_unregister` / `_lookup` and `mms_memory_banks_init`
in `$ND100X/src/cpu/cpu_mms.c` and `$ND100X/src/cpu/cpu_types.h`; the local-RAM
registration call in `cpu_init()` at `$ND100X/src/cpu/cpu.c`; the hard-coded
`ND_MPM5_WINDOW_START_WORD` / `_SIZE_WORD` stub retired;
`mms_get_physical_memory_type()` reduced to a table lookup; and the two test
layers in `$ND100X/tests/test_memory_banks.c` (bank boundaries, refused overlap,
the bound, the named classify-by-word-not-byte regression, and the ECC probe
firing on a LOCAL bank while staying silent on an MPM-5 bank carved out of the
same backing array).

**Exit:** SINTRAN's `MEMORY-CONFIGURATION` reports local RAM as LOCAL from
registered banks, not from a hard-coded range.

---

### Phase 2 - `src/ndbus/` (this repository)

**COMPLETE.** `ctest -R ndbus` is green in `$ND500X/build` (137 checks, no
emulator linked) and the isolation check runs as a POST_BUILD step of
`nd500_ndbus`. Nothing remains in this phase.

Written in this phase (records, not work), all under
`$ND500X/src/ndbus/`:

- `ndbus_types.h` - the `NdbusHostOps` and `NdbusCpuOps` vtables, and the rule
  that nothing in the directory may include an emulator type.
- `$ND500X/tools/check_ndbus_isolation.sh` - enforces that rule as a POST_BUILD
  step of `nd500_ndbus`, refusing both a forbidden header name and any `"../"`
  include that climbs out of the directory.
- `ndbus_pool.h` / `.c` - the pool, `ndbus_pool_r8` / `_w8` as `__ATOMIC_RELAXED`
  builtins over a plain `uint8_t *`, big-endian 16/32-bit helpers built from
  them, overflow-safe bounds (an out-of-pool read is 0, a write is refused
  whole), and `memcpy` bulk paths for image load and DMA.
- `ndbus_lock.h` / `.c` - the single bus-wide mutex, statically initialised so
  there is no start-up order to get wrong; `ndbus_tset32` (32-bit, writes
  `0xFFFFFFFF`, taken only when the cell was zero) and `ndbus_tset16` with a
  caller-chosen taken value; the release helpers; and the spin damper, which
  yields only when the same CPU fails repeatedly at the same address.
- `ndbus_doorbell.h` / `.c` - release on the store, acquire on the load, and
  nothing else in the directory stronger than relaxed.
- `ndbus_octobus.h` / `.c` - station registry refusing 0 and 77B and refusing a
  duplicate rather than replacing it; the software frame format with the
  destination-to-source rewrite on delivery; timeout reported as `-1`, kept
  distinct from `0` so an absent station never looks like a quiet one; MASTER =
  lowest registered station; and the arbitration priority counter, saturating at
  the 4-bit field width.
- `$ND500X/test/test_ndbus.c` - layers 3 and 4. Verified against a negative
  control: with the mutex removed from `ndbus_lock()`, the four-thread TSET test
  loses updates and fails, so the test genuinely measures the lock cycle.

**Exit:** `ctest` green with mock stations and no emulator linked; the whole
phase also passes with zero threads.

---

### Phase 3 - nd500x multi-instance (this repository)

**Independent of phase 2** - different files, no overlap.

Tasks 3.1, 3.2 and the `nd500_mmu.c` part of 3.3 are done and `ctest` is green
at 43/43. What REMAINS:

**A note on task 3.3, from doing the first file of it.** The 88 statics are not
one kind of thing, and "move them all into the structs" is the wrong instruction.
They fall into three groups, and only the first is a correctness problem:

1. **Per-CPU state.** Two CPUs sharing it give WRONG ANSWERS - not slow ones.
   `nd500_mmu.c`'s PST, PCB table and the two I&D enable flags were exactly this:
   one CPU's `DMON` would have switched on the other's data MMU. **Move these.**
2. **Per-machine state.** One pool, one XMSG endpoint, one page-bit map. Correct
   as a singleton today and correct as a `Nd500Machine` field later; move them
   when a second machine exists, not before.
3. **Process-wide singletons.** One front-end console, one debugger, one settings
   catalog. There is one of each no matter how many CPUs run, so moving them into
   a CPU struct would make them wrong. **Leave these, and say so in the file.**

The count in section 5.4 (88) is a count of statics, not a count of defects.

| # | Task | Where |
|---|---|---|
| 3.3a | `nd500_xmsg.c` (10): the subdevice table and the RX queue are one machine's XMSG endpoint, so two CPUs running NDIX would collide in them. **DEFERRED to phase 5 on purpose** - group 2 above, and the public API (`nd500_xmsg_reset`, `_note_datbuf`, `_full_word`, `_set_uplink`) is called from 6 files including both frontends, so the signature change is churn with no effect until a second machine exists. `g_uplink_fn` / `g_uplink_ctx` are group 3 and stay: there is one TCP/TAP uplink per process, set by the frontend | `src/cpu/nd500_xmsg.c` |
| 3.3b | `nd500_page_bits.c` (7): **surveyed - all group 3, leave them.** They are diagnostics: query counters, the marked-page range, and ONE atexit reporter. `g_report_machine` is deliberately a single registration, and the file says why - two handlers would each install themselves with `signal()` and the second would silently replace the first. With two machines the summary would cover whichever registered last, which is a diagnostics limitation, not a wrong answer | `src/cpu/nd500_page_bits.c` |
| 3.3c | `nd500_fecall.c` (20) and `debug_api.c` (15): **survey first, move only what is per CPU.** Both are dominated by process-wide singletons - one front-end console, one debugger - and moving those into the CPU would be churn that makes them wrong, not right | `src/cpu/nd500_fecall.c`, `src/machine/debug_api.c` |
| 3.4 | **Surveyed, and the GAP is closed rather than the refactor done.** `nd500_settings()` is a knob catalog - `ND500X_NOTLB`, `ND500X_FEDBG` - and is process-wide by nature (group 3). The HOST OPS are a real gap: the `.ini` accepts per-CPU `kernel`/`pseg`/`dseg`/`disk0..15`, but `nd500_host.h` serves one set of block devices and one console per process, so two enabled CPUs with their own discs would both be handed the same one. `mc_validate()` now REFUSES that configuration with the reason, so the schema cannot promise what the code does not do. Making the host interface per instance is what later relaxes the check; it is only needed if two NDIX guests must run in one process, which the SINTRAN multi-CPU scenario does not require | `$ND100X/src/machine/machine_config.c` |
| 3.5 | ~~Route the direct `memory[` dereferences through accessors~~ **DONE.** It was NINE sites, all in `src/machine/io.c` - the `Chain.c` and `Riom.c` hits were comments, not code. They now use `ndbus_pool_r8` / `_w8`, and `nd500_machine` links `nd500_ndbus` rather than keeping a second copy of the rule. Verified on this compiler that the relaxed builtin emits the identical instruction: `movzbl (%rdi), %eax` and `movb %sil, (%rdi)`, the same as the plain dereference | done |
| 3.6 | Run the existing ctest suite green **after every sub-step**, not just at the end | `test/CMakeLists.txt` |

Written in task 3.3, `nd500_mmu.c` (records, not work):

- `src/cpu/nd500_mmu.h` - `Nd500MmuState` holds `pst`, `pcb_table`,
  `data_enabled` and `program_enabled`, with `nd500_mmu_state_create()` /
  `_free()`. The two tables stay lazily allocated: a CPU that never enables the
  MMU never pays for 8192 PST entries and 256 PCBs.
- `src/cpu/nd500_mmu.c` - all 57 uses of the four statics now go through
  `cpu->mmu->`; `ensure_mmu_tables()` takes the CPU; `nd500_mmu_state_save()` and
  `_restore()` take the CPU whose state they save, which also settles which CPU
  a nested UECOM run is restoring. The file's own standing comment - "MMU STATE
  (stored in CPU structure - to be added in Phase 3)" - is now accurate.
- `src/cpu/cpu.c`, `src/frontend/nd500x/nd500x_shell.c`,
  `test/test_phys_alloc.c` - allocation, release and the two save/restore call
  sites.
- `ND500X_MMU_GUEST_TABLES` and the demand-segment knob stay file statics: they
  are settings, group 3 above, and they belong to task 3.4.

Written in tasks 3.1 / 3.2 (records, not work):

- `src/cpu/nd500_tlb.h` - `Nd500Tlb` holds `entries[]`, `on` and `init` per CPU;
  `flush_pending` is first in the struct and `_Alignas(64)`-padded so it sits
  alone on a cache line. `g_nd500_xlat_bm` stays ONE shared bitmap, set with
  `__atomic_fetch_or` and read with a relaxed load, because it describes the
  shared memory's role rather than any CPU's state.
- `src/cpu/nd500_tlb.c` - the registry (bounded at `ND500_TLB_MAX_CPUS`, and a
  registration past the bound is refused **loudly**, because a CPU that silently
  failed to attach would never receive a shootdown);
  `nd500_tlb_signal_flush()`, which sets `flush_pending` in every attached cache
  including the writer's and flushes nothing inline; `nd500_tlb_flush_one()`;
  and `nd500_mmu_tlb_flush()` kept for the coarse callers, now meaning "every
  registered cache".
- `src/cpu/cpu.c` - `nd500_cpu_init()` allocates and registers the cache;
  `nd500_cpu_free()` detaches before freeing; `nd500_cpu_step()` calls
  `nd500_tlb_take_pending()` once per instruction, never per memory access.
- `src/machine/io.c` - `nd500_machine_free()` releases the CPU's cache first.
- `src/cpu/instructions/SYSTEM/Dctsb.c` and `Pctsb.c` - flush **this CPU's**
  cache only. They are instructions the guest executes on one CPU, not bus
  operations; another CPU's stale translations are handled by the shootdown on
  the page-table write.
- `test/test_tlb_shootdown.c` - the registry, the shared bitmap, the flag, the
  cache-line alignment, the 2KB page boundary, detach, the bound, and NULL
  safety.

**Exit:** two `Nd500Machine` instances coexist in one process and both run the
conformance corpus to completion.

---

### Phase 4 - ONE ND-5000 CPU, end to end (~4 weeks)

**This is the milestone to reach before any second CPU is attempted.**

| # | Task | Where |
|---|---|---|
| 4.1 | ~~`DEVICE_TYPE_OCTOBUS` and `src/devices/octobus/`~~ **DONE.** `$ND100X/src/devices/octobus/device_octobus.{c,h}`, registered in the device manager, with `$ND100X/tests/test_octobus.c` (21 checks) | done |
| 4.2 | **UNBLOCKED, and the probe half is done.** Open item #3 is answered from SINTRAN source, not a carve request - see below. The eight registers, the two probe sequences (`OCSTART` presence and clear, `CH5CPUPRESENT` data-ready spin) and the ident codes are implemented and tested. **What remains is the FRAME PATH**: nothing is handed to an octobus fabric yet, so a data-register read returns 0 rather than an invented frame | section 6.4 |
| 4.3 | ~~The ND-5000 station at 070B~~ **DONE.** `src/ndbus/ndbus_nd5000.{h,c}` plus `ndbus_multibyte.{h,c}`: SOMB/data/EOMB reassembly, OMD-3 routing to the ACCP, the guard table, Messack/Messnak, and a station number outside 70B..76B refused at init | `src/ndbus/` |
| 4.4 | ~~ACCP command layer~~ **DONE.** `src/ndbus/ndbus_accp.{h,c}`: all 42 command codes, the dispatcher arm range with its four holes, the measured guard table and order, the parameter-length table, and the Messnak codes. T123-T125 were read first and the frame, the Messack/Messnak shape and the error-code table are quoted in the header. **The numeric command bytes are NOT in the manual** - they are firmware-measured, and the manual's section order is not the command order (033B is RUNTST, not STARTMIC; the real start is 066B) | section 2.8 |
| 4.5 | **Doorbell DONE** - `ndbus_nd5000_sniff_write16()` keys on the 0xFFFF -> 0 transition, and a threshold of 2 or more logs the warning the moment it is set, because it cannot be met. The MAILBOX (the message area the doorbell announces) still needs its layout settled before it can be written | section 9.2 |
| 4.6 | **INI PARSER AND WRITER DONE**, `$ND100X/src/machine/machine_config.{c,h}` and `$ND100X/tests/test_machine_config.c` (93 checks). `[mfbus]`, `[mfbus.part.N]`, `[controller.octobus.0]` and `[nd5000.1..7]` parse, validate and round-trip; page and station numbers take octal with a trailing B; a station outside 070B..076B, a duplicate station, a duplicate slot, a slot above 7 and a malformed page are all errors; `[nd500]` is untouched and still works. **The JSON export at `machine_config_json.c` still needs the same fields** so the browser sees the schema | section 8 |
| 4.7 | **BOTH HALVES BUILT, NOT YET JOINED.** ND-100 side: `NdMemoryBank` now carries `read`/`write`/`ctx` and `mms_memory_bank_register_backed()`; the physical read and write paths consult a backed bank **before** the `g_nd_memsize` bounds test, so an MPM-5 window can sit above installed local RAM, and it takes no ECC latch because only LOCAL memory carries the error-correction network. ND-500 side: `src/ndbus/ndbus_window.{h,c}` is the word-to-byte and big-endian conversion in one place. **JOINED.** `$ND100X/src/machine/mfbus_bridge.{c,h}` allocates the pool, attaches an `NdbusWindow` over it and registers it as an `ND_MEM_MPM5` bank at `base_page * 1024` words. `$ND100X/tests/test_mfbus_bridge.c` (38 checks) drives it from both sides: bytes written to the pool read back as ND-100 words at the configured page and the reverse, MSB lands in the first byte of the pair, the window classifies MPM5 while local RAM stays LOCAL, and a base page overlapping installed RAM is refused loudly with nothing left attached. The bridge compiles to nothing without `ND100X_WITH_ND500`, so a machine with no ND-500 needs none of nd500x's headers.

The submodule exists at `$ND100X/external/nd500x` but is pinned at the last PUSHED nd500x commit, which does not contain `src/ndbus/`. Until the ndbus work is committed and pushed, nd100x builds against the sibling checkout `../nd500x` - the CMake default, deliberately, so a stale pinned commit is never compiled in silence | sections 7.2, 7.3 |
| 4.8 | Tests layers 5 and 6; export golden octobus and mailbox traces from RetroCore for layer 8 | section 11 |

**Wiring DONE.** `mc_apply_devices()` in `$ND100X/src/machine/machine_config_apply.c`
builds the machine from the `.ini`: the pool is attached first (both the card
and the stations depend on it), then the octobus card if `[controller.octobus.0]`
is enabled, then one station per enabled `[nd5000.N]`. `mfbus_add_nd5000()`
refuses an illegal or duplicate station and an eighth CPU. All of it is behind
`ND100X_WITH_ND500`, so a machine without the ND-500 is unchanged.

**Exit:** SINTRAN on nd100x reports one LOCAL bank and one MPM5 bank, and brings a
single ND-5000 up over the octobus. **The remaining step is the ND-500 CPU
itself**: the pool, the bus, the card and the stations are built from
configuration, but no `Nd500Machine` is attached to a station yet, so nothing
executes. That needs the native frontend to link `nd500_cpu`/`nd500_machine`,
which it does not today - only the WASM frontend does.

---

### Phase 5 - more CPUs (~3 weeks)

Only after phase 4 is green.

| # | Task | Notes |
|---|---|---|
| 5.1 | **REJECTION DONE**, `mc_validate()` in `$ND100X/src/machine/machine_config.c`: two CPUs with different `base_page`, or a per-CPU `base_page` disagreeing with `[mfbus]`, is refused with the reason; an ENABLED ND-5000 with no `[mfbus]` is refused too, because it has no private memory and nowhere to run. Settling open item #1 is what later RELAXES this check | section 4 |
| 5.2 | **The fabric already does this** - `ndbus_fabric_register()` takes any station 1..76B directly, and `test_ndbus.c` registers all seven ND-5000 slots and refuses an eighth. What remains is CREATING a station per configured CPU from the `.ini` | section 6.3 |
| 5.3 | One host thread per CPU in the **native frontend only**: create, stop, join, and a clean shutdown path | section 7.5 |
| 5.4 | ~~Enable the cross-CPU TLB shootdown~~ **DONE in task 3.2** - `nd500_tlb_signal_flush()` already sets `flush_pending` in every registered cache, and `test_tlb_shootdown.c` covers two caches. Nothing further is needed when more CPUs attach | section 7.5.6 |
| 5.5 | Test layer 7 at two CPUs, then at the full seven | section 11 |

**Exit:** two ND-5000s run concurrently against one shared pool. Target two first -
the carved L07 SINTRAN validates CPU number 0 or 1 (section 3.3) - while sizing the
fabric for the hardware's seven.

---

### Parallel track - the questions, not the code

These need a carve request or a manual, not an editor, and can run alongside any
phase. **None of them now gates a phase.**

**Open item #3 is CLOSED**, and it did not need a carve request. The ND-100
octobus card's registers, addresses and ident codes are all evidenced already:

- **Addresses, byte-verified.** The resident commoncode E-frame sender at 063247
  IOXTs the literal constants 100405 (write data) and 100406 (read status), and
  `s3vs-4.symb` puts `OOCT0` at 100400+4, so the input base 100400 follows from
  the +4 controller spacing. Four interfaces, eight registers each, 010 octal
  apart.
- **Register semantics, from NPL source.** `PH-P2-OPPSTART.NPL:4049` reads +2 to
  detect the card; `:4054` writes 20 octal to +3 (DCONT) to clear the input
  interface and `:4055` reaches +7 for the output one; `:3923` spins on output
  status **bit 3, data ready**; `:3931` writes the master clear to +5.
- **Ident codes, live-verified.** TPE OCTOBUS B00's `LIST-OCTOBUS-DEVICES`
  prints the hardware's own table: 40B/41B, 42B/43B, 44B/45B, 46B/47B, receive
  then transmit, all on level 13.

**Two plausible ident claims are refuted, and both are recorded in
`$ND100X/src/devices/octobus/device_octobus.h` because both look right:** the
formula `((devaddr - 100200) / 4) + 20`, which yields 60B, and the reading of the
L-VSX-500 L07 `ITB13+37B` / `+40B` slots as idents 37B/40B - with those, TPE gets
the level-13 interrupt but does not attribute the ident to the octobus and prints
"No Octobus interrupt detected". So the ITB13 slot index is not the ident code,
and how that table is indexed is still open - but it no longer blocks anything.

The **PCB 3109 identification** was the other half of item #3 and is not needed:
no software path reads a card identification, so nothing in the emulator depends
on it.

| Open item | Raise it | Needed by |
|---|---|---|
| #1 Does `ADRZERO` ever differ between CPUs? | during phase 2 or 3 | phase 5 |
| #2 N500DF offset for the stored `ADRZERO`: 60B or 41B? | with #1 | before any C header names it |
| #5 How did ND-5900 SINTRAN address more than one CPU? | during phase 4 | phase 5 design |
| #6 SAMSON's second multiport channel and second octobus interface | any time | nothing yet |
| #7 `C5DF(0:15)` versus the L07 0-or-1 validation | with #5 | phase 5 sizing |

---

## 11. Test plan

### 11.0 The staging, learned from RetroCore

RetroCore's `$RETROCORE/Emulated.Tests.ND100/ControllerOctobus/` (29 files,
8,191 lines) is a LADDER, and the order is the lesson: **nothing needs SINTRAN
until the last rung, and nothing needs an ND-100 CPU until the middle**. Each
rung is a harness that can be stood up on its own, and a failure at rung N is a
failure of rung N, not of the machine above it.

#### Rung 1 - the bus alone. No ND-100, no CPU, no SINTRAN.

A fabric, a station and a **do-nothing CPU** whose entire job is to let the
station register so its replies have somewhere to go
(`ConformanceMockCpu.cs`: "Do-nothing IND500Cpu whose only job is to let an
OctobusND5000Station register on the octobus fabric"). Then:
frame in, frame out, the destination-to-source rewrite, the ACCP guard matrix.

*We have this:* `test_ndbus.c` layers 4, 5 and 6, with `NdbusCpuOps` carrying a
NULL CPU for exactly the same reason.

#### Rung 2 - shared memory, two ports, no bus traffic.

`MpmBackedMicroMemoryTests` - one backing array reached from both sides, which
is where a byte-order or window-base mistake shows up on its own rather than
disguised as a protocol fault.

*We have this:* the pool, window and bridge tests.

#### Rung 3 - the mailbox, seeded by hand. STILL no ND-100 and no SINTRAN.

`OctobusMailboxO1Tests` does not boot anything. It writes **XMSINIT's picture**
into the pool by hand, builds a message block, and calls the servicer directly.
That is the whole trick: the mailbox is testable because its initial state is
writable.

*We have this as of the XMSINIT work:* `ndbus_mailbox_init_xmsinit()`.

#### Rung 4 - the ND-100 CARD, driven directly. No CPU, no SINTRAN.

`OctobusControllerTests` mirrors **TPE's own octobus tests**, which is the
strongest available idea in this whole ladder - the card is tested the way the
real diagnostic tests it, so passing means the same thing:

| TPE test | What it checks |
|---|---|
| 1 | Check Ident - interface detection |
| 2 | Check data transmission |
| 3 | Check receive FIFO length - **16 words** |
| 4 | Check octobus configuration - station discovery |

`OctobusTpeConfigReproTests` goes further and sends an OMD-0 Test Protocol
message **byte for byte as TPE's `octobus_send_multibyte_message` does**: SOMB,
source-OMD byte, byte count, payload, EOMB, one write to register +5 per frame,
payload starting with the magic `0x71C7`.

**`[GAP]` We do not model the receive FIFO at all.** Our card has registers and
no queue, so TPE test 3 would fail against it today. This is the clearest
single thing to build next on the ND-100 side.

#### Rung 5 - execution in the pool, bare harness.

`OctobusPhase3ExecBringupTests` places a **SAMSON context block** - P at +0x00
= the macro entry point, B at +0x08 = a valid local data base - loads a
hand-assembled ND-500 program into MPM, and runs it. 32-bit values are written
big-endian, high halfword first.

*We have the loading half* (`mfbus_load_nd5000`), *not the context block.*

#### Rung 6 - threading, still bare.

`OctobusPhase3ThreadedTests` has a named **canary**: kick plus X5ACT, and the
GIVEINT answer must arrive **exactly once**. It also records a harness fact
worth stealing: in a bare harness **no machine is clocking the card**, so the
test must pump the device clock itself before draining the FIFO.

#### Rung 7 - the whole machine: ND-100 + SINTRAN + ND-5000.

`OctobusPhase3MonBringupTests`, `...RestartTests`, `...TrapTests`. Only here
does SINTRAN appear.

### 11.1 What a full ND-100 + SINTRAN + ND-5000 scenario needs

In the order a failure would be diagnosed, so the first thing that breaks is the
first thing that was built:

1. **SINTRAN sees the memory.** `MEMORY-CONFIGURATION` reports one LOCAL bank
   and one MPM5 bank. This is task 1.9 and needs nothing from the octobus - if
   it fails, nothing above it can be believed.
2. **SINTRAN finds the card.** `OCSTART` reads register +2 and does not take the
   IOX-error path, then writes 20 octal to +3 and +7. Rung 4.
3. **`CH5CPUPRESENT` finds a CPU.** It spins on output status bit 3, then writes
   `CMMACLE` to +5. Our card sets data-ready from reset precisely so this
   terminates.
4. **`DEFINE-MEMORY-CONFIGURATION`** is given the base page the `.ini` used, and
   `MEMORY-CONFIGURATION` reports the ND-500 address zero back. A mismatch here
   is the ADRZERO question of section 4, not a bug.
5. **The ACCP bring-up sequence** runs over the octobus: ECHO, LSYSPAR, LPARP,
   VPARP, STARTMIC. VPARP is the one that proves shared memory agrees, because
   it reads back a word SINTRAN wrote.
6. **XMSINIT** seeds the mailbox, and the ND-5000's IDLE loop polls X5ACT.
7. **A message round trip**: SINTRAN queues a block on X5BEX, rings X5ACT, the
   ND-5000 services it and answers - the GIVEINT-arrives-exactly-once canary of
   rung 6, now with a real SINTRAN at one end.

**Build the harness for step 7 BEFORE step 1.** Every rung above is reachable
without SINTRAN, and a bug found at rung 3 with a hand-seeded mailbox takes
minutes to understand; the same bug found at step 7 costs a boot per attempt.



Layers 1-5 need neither emulator. That is the point, and it is the structure
RetroCore already proves with
`$RETROCORE/Emulated.Tests.ND100/ControllerOctobus/` (29 test files).

| Layer | What it tests | Model to copy |
|---|---|---|
| **1. Bank table** | LOCAL vs MPM5 at every boundary; overlapping-bank rejection; a bank above installed RAM; **the word-not-byte rule as a named regression test** | `$RETROCORE/Emulated.Tests/ND100/ND100_SystemBusTests.cs:310` |
| **2. ECC probe** | arm `SimBit0 + DisableECC`, write, clear bit 3, read, **level-14 fires means LOCAL**; same sequence on an MPM5 bank stays silent; the latch is per word and an unrelated fetch between write and read must not consume it | `$RETROCORE/Emulated.HW/ND/CPU/ND100/CpuND100.MMS.cs:495-565` |
| **3. Shared pool** | big-endian readback from the ND-5000 side after an ND-100 word write and the reverse; out-of-pool read returns 0 and write is refused; test-and-set atomicity under concurrent threads | `MpmWindow.cs`, `MpmBackedMicroMemoryTests.cs` |
| **4. Octobus protocol** | destination-to-source rewrite; unknown station times out rather than hanging; stations 070B-076B; stations 0 and 77B refused; MASTER assignment to the lowest station; priority increment on giving up | `OctobusFabric.cs`, `OctobusControllerTests.cs` |
| **5. ACCP command layer** | each of the commands used in phase 4, with its reply shape; rejection of out-of-range parameters | `AccpCommandGuardsTests.cs` |
| **6. Mailbox / doorbell** | the X5ACT transition, **including the one-transition-per-XMSINIT fact of section 9.2** | `OctobusMailboxO1Tests.cs` |
| **7. Two mock CPUs** | full handshake both directions with fake read8/write8; deterministic with zero threads and with several | `ConformanceMockCpu.cs` |
| **8. Golden trace replay** | export octobus frame and mailbox traces from RetroCore as JSON, replay in C, diff | **the existing `test/nd500-conformance.json` pattern, 39,598 cases, already working in both repositories** |
| **9. End to end** | SINTRAN on nd100x brings up one ND-5000; browser NDIX still boots unchanged | `OctobusPhase3ExecBringupTests.cs` |

Layer 8 is the strongest available check, because it does not depend on either
implementation being right - only on the two agreeing on bytes, and a
disagreement names the exact frame.

**Explicitly out of scope:** `AccpRealFirmwareConformanceTests.cs` runs the real
ND-324716 MC68000 ACCP firmware through `AccpMachine`. There is no MC68000 in the
C tree. **Real-ACCP conformance stays in RetroCore**; the C side tests against its
recorded traces through layer 8.

---

## 12. Open items

| # | Item | Grade | How to close it |
|---|---|---|---|
| 1 | Is `ADRZERO` ever different between two CPUs? | `[U]` | Dump offset 60B of both CPU datafields on a two-CPU SINTRAN, or carve request: is `DEFMC` the only writer? (section 4) |
| 2 | N500DF offset for the stored `ADRZERO`: 60B or 41B? | `[U]` | Carve request. Do not put either number in a C header first. (section 4) |
| 3 | ND-100-side octobus card register semantics and the PCB 3109 identification | `[U]` | Locate the ND-100 octobus interface manual, or carve the SINTRAN driver. (section 6.4) |
| 5 | How did SINTRAN on an ND-5900 address more than one CPU at the command level? | `[U]` | `DEFINE-MEMORY-CONFIGURATION` has no CPU parameter; `SET-CPU-STATUS` does. Carve request or ND-5900 documentation. (section 3.3) |
| 6 | What are SAMSON's second multiport channel and second octobus interface for? | `[U]` | ND-SAMSON-1 T3 lists them; the document is unfinished. Check ND-05.022.1. (section 2.9) |
| 7 | `C5DF(0:15)` (16 CPU slots) versus the L07 validation of CPU number 0 or 1 | `[U]` | Establish which SINTRAN version each belongs to. (section 3.3) |

None of items 1, 2, 5, 6 or 7 blocks phase 4. **Item 3 is the only one that does**, and it is the only item on this plan that cannot be closed by editing code.

---

## 13. One-paragraph summary

There is **one** shared multifunction-bus memory. Every ND-5000 CPU runs out of
it and has no memory of its own; the ND-100's own local memory is separate and
the ND-5000 cannot reach it. A CPU does not know where its address zero is - the
MPM port's window and base registers decide, and they are set from the memory
cabinet's service console before software runs, which is why SINTRAN's
`DEFINE-MEMORY-CONFIGURATION` issues no IOX and only records the resulting page
offset. Up to seven ND-5000s occupy octobus stations 70B-76B alongside the ND-100
at station 1B, and they coordinate by short octobus messages plus test-and-set
semaphores in the shared memory. In C that becomes one shared byte pool with
big-endian accessors and one lock, an octobus fabric, an ND-5000 station with its
mailbox, an ACCP command layer, and a memory bank table in nd100x that finally
gives the already-correct LOCAL/MPM5 ECC probe something real to classify.
