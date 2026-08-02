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
    for(long s=0;s<3000000&&m.run_flag;s++){
        if(c.PC==0x080050C9u){
            unsigned long long ic=c.instruction_count;
            if(ic>1196000&&ic<1197500){
                uint32_t L0=rd(&c,c.B+0x14);
                uint32_t obj=rd(&c,L0);
                uint32_t w0=rd(&c,obj);
                printf("%llu 0x080050C9 B=%08X  b.0x14=%08X  recordA=mem[b.14]=%08X  word0=%08X mem16=%04X\n",
                       ic, c.B, L0, obj, w0, (w0>>16)&0xFFFF);
            }
        }
        nd500_cpu_step(&c);
        if(m.run_flag==0&&m.stop_reason!=STOP_NONE)break;
    }
    printf("STOP=%s instr=%llu\n",nd500_stop_reason_str(m.stop_reason),(unsigned long long)c.instruction_count);
    return 0;
}
