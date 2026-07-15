/* Capture NC alloc routine 0x08024829 count computation: h1=mem16[*b.0x14],
 * h2=mem16[*b.0x18], sum, and the stored header. Find the 0x0302 (770) one. */
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
    uint32_t h1=0,h2=0; unsigned long long ic=0;
    for(long s=0;s<3000000&&m.run_flag;s++){
        uint32_t pc=c.PC;
        /* 0802483A: w1 + r2  -> at top of it I[0]=h1 (mem16[*b.14]), I[1]=h2 (mem16[*b.18]) */
        if(pc==0x0802483Au){ h1=c.I[0]&0xFFFF; h2=c.I[1]&0xFFFF; ic=c.instruction_count; }
        if(pc==0x0802484Fu){ /* about to store header I[0] */
            uint32_t hdr=c.I[0];
            printf("instr %llu  h1=0x%X h2=0x%X  sum=0x%X  storedHdr=0x%08X\n",
                   ic, h1, h2, h1+h2, hdr);
        }
        nd500_cpu_step(&c);
        if(m.run_flag==0&&m.stop_reason!=STOP_NONE)break;
    }
    printf("STOP=%s instr=%llu PC=%08X\n",nd500_stop_reason_str(m.stop_reason),(unsigned long long)c.instruction_count,c.PC);
    return 0;
}
