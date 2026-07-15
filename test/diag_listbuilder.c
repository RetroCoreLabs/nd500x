/* Log NC list-builder (fn 0x08023F49) invocations: count (I1 at 0x08023F70,
 * after h1:=r1.0 count load) and allocBase (I1 at 0x08023F80). CHECK B,B,B. */
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
    if(ndlib_load_dom_header(dom)||ndlib_load_dom_segments()){fprintf(stderr,"load\n");return 2;}
    Nd500Machine m; Nd500Cpu c;
    nd500_machine_init(&m,MEMSZ); nd500_cpu_init(&c,&m); nd500_cpu_reset(&c);
    nd500_mmu_init(&c); nd500_domain_init(&c); mon_init();
    char in[128]; size_t j=0;
    for(size_t i=0;cmd[i]&&j<126;i++){ if(cmd[i]=='\\'&&cmd[i+1]=='r'){in[j++]='\r';i++;} else if(cmd[i]==';'&&cmd[i+1]==';'){in[j++]='\r';i++;} else in[j++]=cmd[i]; }
    in[j]=0; mon_queue_console_input(in);
    uint32_t sa=0; int dm=0;
    if(ndlib_dom_load_to_machine(&m,&c,-1,NULL,NULL,&sa,&dm)){fprintf(stderr,"load2\n");return 2;}
    m.run_flag=1; m.stop_reason=STOP_NONE;
    uint32_t count=0; unsigned long long cic=0;
    for(long s=0;s<3000000&&m.run_flag;s++){
        if(c.PC==0x08023F70u){ count=c.I[0]&0xFFFF; cic=c.instruction_count; }
        if(c.PC==0x08023F80u){
            printf("instr %llu  count=0x%X (%u)  fills %d  allocBase=0x%08X\n",
                   cic, count, count, (int)count-1, c.I[0]);
        }
        nd500_cpu_step(&c);
        if(m.run_flag==0&&m.stop_reason!=STOP_NONE)break;
    }
    printf("STOP=%s instr=%llu PC=%08X\n",nd500_stop_reason_str(m.stop_reason),
           (unsigned long long)c.instruction_count,c.PC);
    return 0;
}
