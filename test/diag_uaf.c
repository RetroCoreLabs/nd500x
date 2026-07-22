/* Timeline: every freelist free (0x0802CEFE, node=I[1]) and alloc (0x0802CF08 ret)
 * of cell 0x1802A1B0, plus the fatal read at 1,196,912. Shows use-after-free order. */
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
#define TARGET 0x1802A1B0u
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
        uint32_t pc=c.PC;
        if(pc==0x0802CEFEu && c.I[1]==TARGET)
            printf("%llu  FREE   node=0x%08X (PC 0x0802CEFE)\n",(unsigned long long)c.instruction_count,c.I[1]);
        if(pc==0x0802CF60u && (c.I[0]==TARGET||c.I[1]==TARGET))
            printf("%llu  alloc? I0=%08X I1=%08X (PC 0x0802CF60)\n",(unsigned long long)c.instruction_count,c.I[0],c.I[1]);
        nd500_cpu_step(&c);
        /* after alloc returns, I[0] may hold the allocBase */
        if(pc==0x0802CF07u && c.I[0]==TARGET)
            printf("%llu  ALLOC  ret=0x%08X (freelist pop)\n",(unsigned long long)c.instruction_count,c.I[0]);
        if(m.run_flag==0&&m.stop_reason!=STOP_NONE)break;
    }
    printf("STOP=%s instr=%llu\n",nd500_stop_reason_str(m.stop_reason),(unsigned long long)c.instruction_count);
    return 0;
}
