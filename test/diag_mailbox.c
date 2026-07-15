#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/nd500_mmu.h"
#include "../src/cpu/nd500_domain.h"
#include "../src/ndlib/ndlib.h"
#include "../src/libmon/mon.h"
#include "../src/libmon/mon_file_table.h"
#include "../src/libmon/mon_clock.h"
#define MEMSZ (16u*1024u*1024u)
#define MAILBOX 0x08000200u
#define WLO 1300000ULL
#define WHI 1410000ULL

static uint32_t rd(Nd500Cpu*c,uint32_t v){
    if(c->machine&&c->machine->mmu_enabled){
        uint32_t p=nd500_mmu_translate(c,v,0,0);
        if(nd500_trap_occurred())return 0xDEADDEAD;
        if(p+4>=MEMSZ)return 0xBADBAD;
        return nd500_bus_read32(c->machine,p);
    }
    return nd500_bus_read32(c->machine,v);
}
int main(int argc,char**argv){
    const char* dom="/mnt/d/ND/500/FraTor/nc/nc-a06.dom"; const char* cmd=(argc>1)?argv[1]:"CHECK B,B,B\r";
    if(ndlib_load_dom_header(dom)||ndlib_load_dom_segments())return 2;
    Nd500Machine m; Nd500Cpu c; nd500_machine_init(&m,MEMSZ); nd500_cpu_init(&c,&m); nd500_cpu_reset(&c);
    nd500_mmu_init(&c); nd500_domain_init(&c); mon_init();
    char in[128]; size_t j=0; for(size_t i=0;cmd[i]&&j<126;i++){ if(cmd[i]=='\\'&&cmd[i+1]=='r'){in[j++]='\r';i++;} else if(cmd[i]==';'&&cmd[i+1]==';'){in[j++]='\r';i++;} else in[j++]=cmd[i]; } in[j]=0; mon_queue_console_input(in);
    uint32_t sa=0; int dm=0; if(ndlib_dom_load_to_machine(&m,&c,-1,NULL,NULL,&sa,&dm))return 2; m.run_flag=1; m.stop_reason=STOP_NONE;
    uint32_t prev=0; int have=0; long nchg=0;
    for(long s=0;s<3000000&&m.run_flag;s++){
        uint32_t pc=c.PC; unsigned long long ic=c.instruction_count;
        nd500_cpu_step(&c);
        if(c.machine->mmu_enabled && ic>=WLO && ic<=WHI){
            uint32_t g=rd(&c,MAILBOX);
            if(!have){prev=g;have=1;printf("%llu (baseline) mailbox=%08X\n",ic,g);}
            else if(g!=prev){printf("%llu PC=%08X mailbox: %08X -> %08X\n",ic,pc,prev,g);prev=g;nchg++;}
        }
    }
    printf("changes=%ld STOP=%s instr=%llu\n",nchg,nd500_stop_reason_str(m.stop_reason),(unsigned long long)c.instruction_count);
    return 0;
}
