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
#include "testdata.h"
#define MEMSZ (16u*1024u*1024u)
static uint32_t rd(Nd500Cpu*c,uint32_t v){
    if(c->machine&&c->machine->mmu_enabled){uint32_t p=nd500_mmu_translate(c,v,0,0);
        if(nd500_trap_occurred()){nd500_trap_clear();return 0xDEADDEAD;} if(p+4>=MEMSZ)return 0xBADBAD;
        return nd500_bus_read32(c->machine,p);}
    return nd500_bus_read32(c->machine,v);
}
int main(int argc,char**argv){
    const char* dom=nd500_testdata("FraTor/nc/nc-a06.dom"); const char* cmd=(argc>1)?argv[1]:"CHECK B,B,B\r";
    if(ndlib_load_dom_header(dom)||ndlib_load_dom_segments())return 2;
    Nd500Machine m; Nd500Cpu c; nd500_machine_init(&m,MEMSZ); nd500_cpu_init(&c,&m); nd500_cpu_reset(&c);
    nd500_mmu_init(&c); nd500_domain_init(&c); mon_init();
    char in[128]; size_t j=0; for(size_t i=0;cmd[i]&&j<126;i++){ if(cmd[i]=='\\'&&cmd[i+1]=='r'){in[j++]='\r';i++;} else if(cmd[i]==';'&&cmd[i+1]==';'){in[j++]='\r';i++;} else in[j++]=cmd[i]; } in[j]=0; mon_queue_console_input(in);
    uint32_t sa=0; int dm=0; if(ndlib_dom_load_to_machine(&m,&c,-1,NULL,NULL,&sa,&dm))return 2; m.run_flag=1; m.stop_reason=STOP_NONE;
    int done=0;
    for(long s=0;s<3000000&&m.run_flag;s++){
        uint32_t pc=c.PC;
        /* the free store is 0x0802CEFE `w1 =: r2.0`, r2=I[1] is the cell being freed */
        if(pc==0x0802CEFEu && c.I[1]==0x1802A1B0u && c.I[0]==0x18038000u && !done){
            done=1;
            printf("[FREE] instr=%llu cell=0x1802A1B0 link_val(w1/I0)=%08X\n",
                   (unsigned long long)c.instruction_count, c.I[0]);
            printf("  regs: L=%08X B=%08X R=%08X\n", c.L, c.B, c.R);
            /* walk frame chain: PREVB at B+0, RETA at B+4 (manual: PREVB=0,RETA=4) */
            uint32_t B=c.B;
            printf("  L (return addr of free helper) = %08X\n", c.L);
            for(int f=0; f<12 && B>=0x10000000u && B<0x18000000u; f++){
                uint32_t reta = rd(&c, B+4);
                uint32_t prevB = rd(&c, B+0);
                printf("  frame %d: B=%08X  RETA=%08X  PREVB=%08X\n", f, B, reta, prevB);
                if(prevB==B || prevB==0 || prevB==0xDEADDEAD) break;
                B=prevB;
            }
            break;
        }
        nd500_cpu_step(&c);
        if(m.run_flag==0&&m.stop_reason!=STOP_NONE)break;
    }
    printf("STOP=%s instr=%llu done=%d\n",nd500_stop_reason_str(m.stop_reason),(unsigned long long)c.instruction_count,done);
    return 0;
}
