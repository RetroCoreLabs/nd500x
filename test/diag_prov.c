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

    /* NON-perturbing: register compares only; dump at the fatal instant */
    int dumped=0;
    for(long s=0;s<3000000&&m.run_flag;s++){
        uint32_t pc=c.PC;
        if(pc==0x08024884u && c.I[0]==0xA1B8A1A8u && !dumped){
            dumped=1;
            uint32_t B=c.B;
            uint32_t src = rd(&c, B + 0x14);              /* b.20 = copy SOURCE record */
            uint32_t dst = rd(&c, B + 0x20);              /* b.32 = copy DEST base */
            printf("[FATAL COPY] ic=%llu B=%08X source(b.20)=%08X dest(b.32)=%08X\n",
                   (unsigned long long)c.instruction_count, B, src, dst);
            printf("SOURCE dump [-2..+28]: ");
            for(int o=-2;o<28;o+=4) printf("[%d]=%08X ", o, rd(&c, src+o));
            printf("\n");
            break;
        }
        nd500_cpu_step(&c);
        if(m.run_flag==0&&m.stop_reason!=STOP_NONE)break;
    }
    printf("STOP=%s instr=%llu dumped=%d\n",nd500_stop_reason_str(m.stop_reason),(unsigned long long)c.instruction_count,dumped);

    /* scan all mapped data for the literal 0x54312D42 to locate every copy */
    printf("=== scan seg 0x08000000..0x08060000 and 0x18000000..0x18050000 for 0x54312D42 ===\n");
    uint32_t ranges[][2]={{0x08000000u,0x08060000u},{0x18000000u,0x18050000u}};
    for(int r=0;r<2;r++){
        for(uint32_t a=ranges[r][0]; a<ranges[r][1]; a+=2){
            uint32_t v=rd(&c,a);
            if(v==0x54312D42u) printf("  0x54312D42 at %08X\n", a);
        }
    }
    return 0;
}
