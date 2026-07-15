/* diag_cghist.c - Codegen-runaway analyzer for NC (nc-a06.dom).
 *
 * Answers the C#/RetroCore handoff questions:
 *   1. Loop back-edge (top of repeating cycle + the branch that should exit).
 *   2. Progress vs. re-processing the same node (register movement per iteration).
 *   3. Which MON call NC sits on inside the loop and what it returns (MON tail).
 *   4. Does codegen ever terminate (NRF emitted)?
 *
 * Build (from repo root) - see NC_CRASH_HANDOFF.md link line:
 *   gcc -O2 -o build/bin/diag_cghist test/diag_cghist.c -Wl,--start-group \
 *     build/lib/libnd500_debugger.a build/lib/libnd500_cpu.a build/lib/libnd500_machine.a \
 *     build/lib/libmon.a build/lib/libnd500_ndlib.a build/lib/libnd500_disasm.a \
 *     -Wl,--end-group -lpthread -lm
 * Run FROM build/nc_sandbox:
 *   cd build/nc_sandbox && ND500X_PIN_CLOCK=1 ../bin/diag_cghist "COMPILE B,B,B\r" 15000000
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/nd500_mmu.h"
#include "../src/cpu/nd500_domain.h"
#include "../src/ndlib/ndlib.h"
#include "../src/libmon/mon.h"
#include "../src/libmon/mon_log.h"
#include "../src/libmon/mon_file_table.h"
#include "../src/libmon/mon_clock.h"

#define MEMSZ (16u*1024u*1024u)
static const char* DOM="/mnt/d/ND/500/FraTor/nc/nc-a06.dom";

/* Codegen region per handoff: 0x0802B000 - 0x0802E000 */
#define CG_LO 0x0802B000u
#define CG_HI 0x0802E000u

/* PC histogram over 0x08000000..0x08040000 */
#define HBASE 0x08000000u
#define HSPAN 0x00040000u
static uint32_t *hist;               /* execution count per PC */

/* Back-edge table: transitions prevPC->PC with PC<prevPC (both in code seg). */
#define BE_MAX 512
static struct { uint32_t from, to; uint64_t count; } be[BE_MAX];
static int be_n=0;
static void be_add(uint32_t from,uint32_t to){
    for(int i=0;i<be_n;i++){ if(be[i].from==from&&be[i].to==to){be[i].count++;return;} }
    if(be_n<BE_MAX){ be[be_n].from=from; be[be_n].to=to; be[be_n].count=1; be_n++; }
}

/* MON call tail ring (last N log lines). */
#define MRING 160
static char mring[MRING][200];
static int mhead=0; static uint64_t mtotal=0;
static void mon_cb(MonLogLevel lvl,const char* msg){
    (void)lvl;
    strncpy(mring[mhead%MRING],msg,199); mring[mhead%MRING][199]=0;
    mhead++; mtotal++;
}

/* Per-iteration register sample ring during codegen (progress detector). */
#define SRING 8192
static struct { uint32_t pc,i0,i1,i2,i3,b,r,tos; } sr[SRING];
static int shead=0;

int main(int argc,char**argv){
    const char* cmd=(argc>1)?argv[1]:"COMPILE B,B,B\r";
    long maxsteps=(argc>2)?atol(argv[2]):15000000;
    hist=calloc(HSPAN,sizeof(uint32_t));
    if(!hist){fprintf(stderr,"calloc fail\n");return 3;}

    if(ndlib_load_dom_header(DOM)||ndlib_load_dom_segments()){fprintf(stderr,"load fail\n");return 2;}
    Nd500Machine m; Nd500Cpu c;
    nd500_machine_init(&m,MEMSZ); nd500_cpu_init(&c,&m); nd500_cpu_reset(&c);
    nd500_mmu_init(&c); nd500_domain_init(&c); mon_init();
    mon_log_set_callback(mon_cb); mon_log_enable(1); mon_log_set_level(MON_LOG_DEBUG);

    char in[128]; size_t j=0;
    for(size_t i=0;cmd[i]&&j<126;i++){ if(cmd[i]=='\\'&&cmd[i+1]=='r'){in[j++]='\r';i++;} else if(cmd[i]==';'&&cmd[i+1]==';'){in[j++]='\r';i++;} else in[j++]=cmd[i]; }
    in[j]=0; mon_queue_console_input(in);
    uint32_t sa=0; int dm=0;
    if(ndlib_dom_load_to_machine(&m,&c,-1,NULL,NULL,&sa,&dm)){fprintf(stderr,"load2 fail\n");return 2;}
    m.run_flag=1; m.stop_reason=STOP_NONE;

    int cg=0; uint64_t cg_start_instr=0; uint32_t prev=0;
    uint64_t cg_steps=0, cg_mon_at_entry=0;
    for(long s=0;s<maxsteps&&m.run_flag;s++){
        uint32_t pc=c.PC;
        nd500_cpu_step(&c);
        /* histogram */
        if(pc>=HBASE && pc<HBASE+HSPAN) hist[pc-HBASE]++;
        /* codegen entry */
        if(!cg && pc>=CG_LO && pc<CG_HI){ cg=1; cg_start_instr=c.instruction_count; cg_mon_at_entry=mtotal; }
        if(cg){
            cg_steps++;
            /* back-edge: taken backward branch inside code segment */
            if(c.PC<prev && prev>=0x08000000u && prev<HBASE+HSPAN && c.PC>=0x08000000u){
                if(prev-c.PC <= 0x8000u) be_add(prev,c.PC);
            }
            /* register sample ring */
            sr[shead%SRING].pc=c.PC; sr[shead%SRING].i0=c.I[0]; sr[shead%SRING].i1=c.I[1];
            sr[shead%SRING].i2=c.I[2]; sr[shead%SRING].i3=c.I[3];
            sr[shead%SRING].b=c.B; sr[shead%SRING].r=c.R; sr[shead%SRING].tos=c.TOS;
            shead++;
        }
        prev=c.PC;
        if(m.run_flag==0&&m.stop_reason!=STOP_NONE)break;
    }

    printf("=== RUN ===\n");
    printf("cmd=\"%s\" maxsteps=%ld\n",in,maxsteps);
    printf("STOP=%s total_instr=%llu PC=%08X\n",nd500_stop_reason_str(m.stop_reason),
           (unsigned long long)c.instruction_count,c.PC);
    printf("codegen_entered=%d at_instr=%llu cg_steps=%llu\n",cg,
           (unsigned long long)cg_start_instr,(unsigned long long)cg_steps);

    /* Top hot PCs */
    printf("\n=== TOP 25 HOT PCs (whole run) ===\n");
    for(int k=0;k<25;k++){
        uint32_t best=0; uint32_t bi=0;
        for(uint32_t i=0;i<HSPAN;i++){ if(hist[i]>best){best=hist[i];bi=i;} }
        if(!best)break;
        printf("  %2d  PC=%08X  count=%u\n",k+1,HBASE+bi,best);
        hist[bi]=0;
    }

    /* Top back-edges */
    printf("\n=== TOP BACK-EDGES (codegen window) ===\n");
    for(int k=0;k<20;k++){
        int bi=-1; uint64_t best=0;
        for(int i=0;i<be_n;i++){ if(be[i].count>best){best=be[i].count;bi=i;} }
        if(bi<0||best==0)break;
        printf("  %2d  %08X -> %08X   x%llu\n",k+1,be[bi].from,be[bi].to,(unsigned long long)be[bi].count);
        be[bi].count=0;
    }

    /* Register-progress ring: last 48 sampled states */
    printf("\n=== LAST 48 REGISTER SAMPLES (progress vs re-process) ===\n");
    printf("   PC        I1        I2        I3        I4        B         R         TOS\n");
    int n=shead<SRING?shead:SRING; int start=shead-48; if(start<shead-n)start=shead-n;
    for(int i=start;i<shead;i++){
        int q=i%SRING;
        printf("  %08X %08X %08X %08X %08X %08X %08X %08X\n",
            sr[q].pc,sr[q].i0,sr[q].i1,sr[q].i2,sr[q].i3,sr[q].b,sr[q].r,sr[q].tos);
    }

    /* MON tail */
    printf("\n=== MON TAIL (last %d of %llu log lines; %llu since codegen entry) ===\n",
           MRING,(unsigned long long)mtotal,(unsigned long long)(mtotal-cg_mon_at_entry));
    int mn = mtotal<MRING?(int)mtotal:MRING;
    for(int i=mhead-mn;i<mhead;i++){ printf("  %s\n",mring[((i%MRING)+MRING)%MRING]); }

    free(hist);
    return 0;
}
