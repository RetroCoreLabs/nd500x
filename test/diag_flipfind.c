/* Flip-finder: for each conditional-branch SITE executed before the crash, run a
 * full sim forcing that branch to the OPPOSITE outcome at every execution, and
 * record where it stops. A flip that avoids the 0x080241FC crash (gets past the
 * normal crash instr 1,230,739, or stops elsewhere) implicates that branch's flag.
 *
 * Pass 1: collect branch PCs and, per PC, the two next-PCs (fallthrough + target).
 * Pass 2: per candidate PC, replay forcing the opposite next-PC.
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
#include <ndmon/mon_file_table.h>
#include <ndmon/mon_clock.h>
#define MEMSZ (16u*1024u*1024u)
#define BASE 0x08000000u
#define SPAN 0x00040000u
#define CAP 1350000L
static uint8_t rd8(Nd500Cpu*c,uint32_t v){ if(c->machine&&c->machine->mmu_enabled){uint32_t p=nd500_mmu_translate(c,v,0,1); if(nd500_trap_occurred())return 0; return nd500_bus_read8(c->machine,p);} return nd500_bus_read8(c->machine,v);}
static uint32_t nxtA[SPAN]; static uint32_t nxtB[SPAN]; static uint8_t isbrPC[SPAN];
static int is_branch(uint8_t op){ return op==0xC5||op==0xC6||op==0xC7||op==0xD2||op==0xD3||op==0xD6||op==0xD8||op==0xD9||op==0xDA||op==0xDB; }
static void setup(Nd500Machine*m,Nd500Cpu*c,const char*cmd){
    nd500_machine_init(m,MEMSZ); nd500_cpu_init(c,m); nd500_cpu_reset(c);
    nd500_mmu_init(c); nd500_domain_init(c); mon_init();
    char in[128]; size_t j=0;
    for(size_t i=0;cmd[i]&&j<126;i++){ if(cmd[i]=='\\'&&cmd[i+1]=='r'){in[j++]='\r';i++;} else if(cmd[i]==';'&&cmd[i+1]==';'){in[j++]='\r';i++;} else in[j++]=cmd[i]; }
    in[j]=0; mon_queue_console_input(in);
    uint32_t sa=0; int dm=0; ndlib_dom_load_to_machine(m,c,-1,NULL,NULL,&sa,&dm);
    m->run_flag=1; m->stop_reason=STOP_NONE;
}
int main(int argc,char**argv){
    const char* dom="/mnt/d/ND/500/FraTor/nc/nc-a06.dom";
    const char* cmd=(argc>1)?argv[1]:"CHECK B,B,B\r";
    if(ndlib_load_dom_header(dom)||ndlib_load_dom_segments())return 2;
    /* ---- Pass 1: collect branch sites + their two next-PCs ---- */
    { Nd500Machine m; Nd500Cpu c; setup(&m,&c,cmd);
      uint32_t prevpc=0; uint8_t prevbr=0;
      for(long s=0;s<3000000&&m.run_flag;s++){
        if(prevbr){ uint32_t i=prevpc-BASE; uint32_t np=c.PC;
            if(nxtA[i]==0){nxtA[i]=np;} else if(nxtA[i]!=np && nxtB[i]==0){nxtB[i]=np;} }
        uint32_t pc=c.PC; prevbr=0;
        if(pc>=BASE&&pc<BASE+SPAN){ uint8_t op=rd8(&c,pc); if(is_branch(op)){isbrPC[pc-BASE]=1;prevbr=1;prevpc=pc;} }
        nd500_cpu_step(&c);
        if(m.run_flag==0&&m.stop_reason!=STOP_NONE)break;
      }
      nd500_machine_free(&m);
    }
    /* candidate list = branch PCs with BOTH next-PCs known */
    static uint32_t cand[4096]; int nc=0;
    for(uint32_t i=0;i<SPAN;i++) if(isbrPC[i]&&nxtA[i]&&nxtB[i]&&nc<4096) cand[nc++]=BASE+i;
    fprintf(stderr,"flippable candidates: %d\n",nc);
    /* ---- Single-target mode: argv[2]=PC ---- */
    if(argc>2){
        uint32_t tgt=(uint32_t)strtoul(argv[2],0,0); uint32_t A=nxtA[tgt-BASE],B=nxtB[tgt-BASE];
        Nd500Machine m; Nd500Cpu c; setup(&m,&c,cmd);
        long CAP2=2500000L; uint32_t lastpc=0; long flips=0;
        for(long step=0;step<CAP2&&m.run_flag;step++){
            uint32_t pc=c.PC; lastpc=pc; int hit=(pc==tgt);
            nd500_cpu_step(&c);
            if(hit){ if(c.PC==A){c.PC=B;flips++;} else if(c.PC==B){c.PC=A;flips++;} }
            if(m.run_flag==0&&m.stop_reason!=STOP_NONE)break;
        }
        printf("SINGLE FLIP PC=%08X A=%08X B=%08X flips=%ld -> stop=%s instr=%llu lastPC=%08X\n",
               tgt,A,B,flips,nd500_stop_reason_str(m.stop_reason),(unsigned long long)c.instruction_count,lastpc);
        return 0;
    }
    /* ---- Pass 2: flip each candidate ---- */
    for(int k=0;k<nc;k++){
        uint32_t tgt=cand[k]; uint32_t A=nxtA[tgt-BASE],B=nxtB[tgt-BASE];
        Nd500Machine m; Nd500Cpu c; setup(&m,&c,cmd);
        long step=0; uint32_t lastpc=0;
        for(;step<CAP&&m.run_flag;step++){
            uint32_t pc=c.PC; lastpc=pc;
            int hit=(pc==tgt);
            nd500_cpu_step(&c);
            if(hit){ if(c.PC==A)c.PC=B; else if(c.PC==B)c.PC=A; }
            if(m.run_flag==0&&m.stop_reason!=STOP_NONE)break;
        }
        int crashed_here = (m.stop_reason!=STOP_NONE && (lastpc==0x080241F9u||lastpc==0x080241FCu));
        /* interesting = did NOT crash at the usual site, and got reasonably far */
        if(!crashed_here){
            printf("FLIP PC=%08X -> stop=%s instr=%llu lastPC=%08X  (A=%08X B=%08X)\n",
                   tgt, nd500_stop_reason_str(m.stop_reason),
                   (unsigned long long)c.instruction_count, lastpc, A, B);
            fflush(stdout);
        }
        nd500_machine_free(&m);
    }
    printf("DONE sweep of %d branches\n",nc);
    return 0;
}
