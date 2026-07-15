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
static uint8_t rd8(Nd500Cpu*c,uint32_t v){ if(c->machine&&c->machine->mmu_enabled){uint32_t p=nd500_mmu_translate(c,v,0,1); if(nd500_trap_occurred())return 0; return nd500_bus_read8(c->machine,p);} return nd500_bus_read8(c->machine,v);}
int main(int argc,char**argv){
    const char* dom="/mnt/d/ND/500/FraTor/nc/nc-a06.dom";
    const char* cmd=(argc>1)?argv[1]:"CHECK B,B,B\r";
    unsigned long long lo=(argc>2)?strtoull(argv[2],0,0):1110300ull;
    unsigned long long hi=(argc>3)?strtoull(argv[3],0,0):1110470ull;
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
        uint8_t op = (ic>=lo&&ic<=hi)? rd8(&c,pc):0;
        /* ND-500 conditional branch first-bytes: C5/C6/C7 (=,><), D2/D3 (k), D6 (>=), D9 (<<), D0/D1 */
        int isbr = (op==0xC5||op==0xC6||op==0xC7||op==0xD2||op==0xD3||op==0xD6||op==0xD9);
        uint32_t fl=c.ST1;
        nd500_cpu_step(&c);
        if(isbr){
            int taken = (c.PC != pc+3) && (c.PC != pc+2); /* rough: branched away */
            printf("%llu PC=%08X op=%02X FLAGS=%08X Z=%d C=%d S=%d O=%d -> PC=%08X %s\n",
                   ic,pc,op,fl,(fl>>5)&1,(fl>>6)&1,(fl>>7)&1,(fl>>9)&1,c.PC, taken?"TAKEN":"not");
        }
        if(m.run_flag==0&&m.stop_reason!=STOP_NONE)break;
    }
    printf("STOP=%s instr=%llu\n",nd500_stop_reason_str(m.stop_reason),(unsigned long long)c.instruction_count);
    return 0;
}
