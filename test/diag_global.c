/* Poll global word @0x08000200; log every change (PC, instr#) to find who writes 0x1802A1B0. */
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
#define G 0x08000200u
static uint32_t rd(Nd500Cpu*c,uint32_t v){ if(c->machine&&c->machine->mmu_enabled){uint32_t p=nd500_mmu_translate(c,v,0,0); if(nd500_trap_occurred())return 0xDEAD; if(p+4>=MEMSZ)return 0xBAD; return nd500_bus_read32(c->machine,p);} return nd500_bus_read32(c->machine,v);}
int main(int argc,char**argv){
    const char* dom=nd500_testdata("FraTor/nc/nc-a06.dom");
    const char* cmd=(argc>1)?argv[1]:"CHECK B,B,B\r";
    if(ndlib_load_dom_header(dom)||ndlib_load_dom_segments())return 2;
    Nd500Machine m; Nd500Cpu c;
    nd500_machine_init(&m,MEMSZ); nd500_cpu_init(&c,&m); nd500_cpu_reset(&c);
    nd500_mmu_init(&c); nd500_domain_init(&c); mon_init();
    char in[128]; size_t j=0;
    for(size_t i=0;cmd[i]&&j<126;i++){ if(cmd[i]=='\\'&&cmd[i+1]=='r'){in[j++]='\r';i++;} else if(cmd[i]==';'&&cmd[i+1]==';'){in[j++]='\r';i++;} else in[j++]=cmd[i]; }
    in[j]=0; mon_queue_console_input(in);
    uint32_t sa=0; int dm=0;
    if(ndlib_dom_load_to_machine(&m,&c,-1,NULL,NULL,&sa,&dm))return 2;
    m.run_flag=1; m.stop_reason=STOP_NONE;
    uint32_t prev=0; int have=0;
    for(long s=0;s<3000000&&m.run_flag;s++){
        uint32_t pc=c.PC;
        nd500_cpu_step(&c);
        if(c.machine->mmu_enabled){
            uint32_t v=rd(&c,G);
            if(!have){prev=v;have=1;}
            else if(v!=prev){ printf("%llu  PC=%08X  mem[0x08000200]: %08X -> %08X\n",(unsigned long long)c.instruction_count,pc,prev,v); prev=v; }
        }
        if(m.run_flag==0&&m.stop_reason!=STOP_NONE)break;
    }
    printf("STOP=%s instr=%llu\n",nd500_stop_reason_str(m.stop_reason),(unsigned long long)c.instruction_count);
    return 0;
}
