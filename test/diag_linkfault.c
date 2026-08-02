/*
 * Diagnostic: capture the linker startup SCOPA operands at the PV (Phase 2).
 * Stops at PC=0xB0041CF2 (by scopa b.1648,b.1656,$0) and dumps, via the trap-free
 * nd500_mmu_peek, the two 8-byte string descriptors [count,base] and the record R
 * fields (r.2/r.6/r.10/r.20/r.24/r.28) that feed them, to decide null-base vs
 * count-underflow, and whether R is bogus.
 *
 * Build: gcc -O2 -o build/bin/diag_linkfault test/diag_linkfault.c \
 *   -Wl,--start-group build/lib/libnd500_debugger.a build/lib/libnd500_cpu.a \
 *   build/lib/libnd500_machine.a build/lib/libmon.a build/lib/libnd500_ndlib.a \
 *   build/lib/libnd500_disasm.a -Wl,--end-group -lpthread -lm
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/nd500_mmu.h"
#include "../src/cpu/nd500_domain.h"
#include "../src/ndlib/ndlib.h"
#include <ndmon/mon.h>
#include <ndmon/mon_log.h>
#include "testdata.h"

#define MEMSZ (16u*1024u*1024u)
#define SCOPA_PC 0xB0041CF2u

int main(int argc,char**argv){
    const char* dom = (argc>1)?argv[1]:nd500_testdata("nd-linker/linker-b01.dom");
    const char* cmd = (argc>2)?argv[2]:"EXIT;;";
    if(ndlib_load_dom_header(dom)||ndlib_load_dom_segments()){fprintf(stderr,"load fail\n");return 2;}
    Nd500Machine m; Nd500Cpu c;
    nd500_machine_init(&m,MEMSZ); nd500_cpu_init(&c,&m); nd500_cpu_reset(&c);
    nd500_mmu_init(&c); nd500_domain_init(&c); mon_init();
    char inbuf[128]; size_t j=0;
    for(size_t i=0;cmd[i]&&j<126;i++){ if(cmd[i]==';'&&cmd[i+1]==';'){inbuf[j++]='\r';i++;} else inbuf[j++]=cmd[i]; }
    inbuf[j]=0; mon_queue_console_input(inbuf);
    uint32_t sa=0; int dm=0;
    if(ndlib_dom_load_to_machine(&m,&c,-1,NULL,NULL,&sa,&dm)){fprintf(stderr,"load2 fail\n");return 2;}
    m.run_flag=1; m.stop_reason=STOP_NONE;

    #define PK32(va) ({ uint32_t _p=nd500_mmu_peek(&c,(va)); (_p==0xFFFFFFFFu)?0xDEAD0000u:nd500_bus_read32(&m,_p); })
    /* Target PC to dump at (default: the command-parser copy-loop at 0xB00391B3). */
    uint32_t target = (argc>3)?(uint32_t)strtoul(argv[3],NULL,0):0xB00391B3u;
    int dumped=0;
    for(long s=0;s<3000000&&m.run_flag;s++){
        if(c.PC==target && !dumped){
            uint32_t B=c.B, R=c.R;
            printf("HIT PC=%08X @instr=%llu  B=%08X R=%08X L=%08X\n",
                   target,(unsigned long long)c.instruction_count,B,R,c.L);
            printf("  I1=%08X I2=%08X I3=%08X I4=%08X\n",c.I[0],c.I[1],c.I[2],c.I[3]);
            printf("  b.36(count)=%08X  b.50(base)=%08X  b.306=%08X\n",
                   PK32(B+36),PK32(B+50),PK32(B+306));
            printf("  R fields r.48=%08X r.44=%08X r.52=%08X\n",PK32(R+48),PK32(R+44),PK32(R+52));
            dumped=1;
        }
        nd500_cpu_step(&c);
        if(m.run_flag==0&&m.stop_reason!=STOP_NONE)break;
    }
    fprintf(stderr,"STOP=%s instr=%llu PC=%08X dumped=%d\n",
            nd500_stop_reason_str(m.stop_reason),(unsigned long long)c.instruction_count,c.PC,dumped);
    return 0;
}
