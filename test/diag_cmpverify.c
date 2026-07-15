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
static uint32_t rd(Nd500Cpu*c,uint32_t v){ if(c->machine&&c->machine->mmu_enabled){uint32_t p=nd500_mmu_translate(c,v,0,0); if(nd500_trap_occurred())return 0xDEAD; if(p+4>=MEMSZ)return 0xBAD; return nd500_bus_read32(c->machine,p);} return nd500_bus_read32(c->machine,v);}
int main(int argc,char**argv){
    const char* dom="/mnt/d/ND/500/FraTor/nc/nc-a06.dom";
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
        uint32_t pc=c.PC; unsigned long long ic=c.instruction_count;
        if(ic>1110350 && ic<1110410){
            if(pc==0x080248D0u){ /* just after h comp2 b.0x26,b.0x2E */
                uint32_t w26=rd(&c,c.B+0x26), w2e=rd(&c,c.B+0x2E);
                uint16_t a=(w26>>16)&0xFFFF, b=(w2e>>16)&0xFFFF; /* big-endian halfword @off */
                printf("%llu 0x080248D0 h comp2 b.26,b.2E: b26=0x%04X b2E=0x%04X  ST1=%08X Z=%d  (true a==b? %d)\n",
                       ic, a, b, c.ST1, (c.ST1>>5)&1, a==b);
            }
            if(pc==0x0802485Du){
                uint32_t w2e=rd(&c,c.B+0x2E); uint16_t a=(w2e>>16)&0xFFFF;
                printf("%llu 0x0802485D if<< after comp2 b.2E,$1: b2E=0x%04X  ST1=%08X S=%d  (true a<1? %d signed)\n",
                       ic, a, c.ST1, (c.ST1>>7)&1, ((int16_t)a)<1);
            }
        }
        nd500_cpu_step(&c);
        if(m.run_flag==0&&m.stop_reason!=STOP_NONE)break;
    }
    printf("STOP=%s instr=%llu\n",nd500_stop_reason_str(m.stop_reason),(unsigned long long)c.instruction_count);
    return 0;
}
