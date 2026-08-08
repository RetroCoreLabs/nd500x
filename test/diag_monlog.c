/* Log every MON call NC makes up to the crash (CHECK B,B,B). Run from build/nc_sandbox. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/nd500_mmu.h"
#include "../src/cpu/nd500_domain.h"
#include "../src/ndlib/ndlib.h"
#include <ndmon/mon.h>
#include <ndmon/mon_log.h>
#include <ndmon/mon_file_table.h>
#include <ndmon/mon_clock.h>
#include "testdata.h"
#define MEMSZ (16u*1024u*1024u)
static const char* DOM; /* assigned in main - nd500_testdata() is a call, not a constant */
int main(int argc,char**argv){
    DOM = nd500_testdata("FraTor/nc/nc-a06.dom");
    const char* cmd=(argc>1)?argv[1]:"CHECK B,B,B\r";
    if(ndlib_load_dom_header(DOM)||ndlib_load_dom_segments()){fprintf(stderr,"load fail\n");return 2;}
    Nd500Machine m; Nd500Cpu c;
    nd500_machine_init(&m,MEMSZ); nd500_cpu_init(&c,&m); nd500_cpu_reset(&c);
    nd500_mmu_init(&c); nd500_domain_init(&c); mon_init();
    mon_log_enable(1); mon_log_set_level(MON_LOG_DEBUG);
    char inbuf[128]; size_t j=0;
    for(size_t i=0;cmd[i]&&j<126;i++){ if(cmd[i]=='\\'&&cmd[i+1]=='r'){inbuf[j++]='\r';i++;} else if(cmd[i]==';'&&cmd[i+1]==';'){inbuf[j++]='\r';i++;} else inbuf[j++]=cmd[i]; }
    inbuf[j]=0; mon_queue_console_input(inbuf);
    uint32_t sa=0; int dom=0;
    if(ndlib_dom_load_to_machine(&m,&c,-1,NULL,NULL,&sa,&dom)){fprintf(stderr,"load2 fail\n");return 2;}
    m.run_flag=1; m.stop_reason=STOP_NONE;
    for(long s=0;s<2000000&&m.run_flag;s++){ nd500_cpu_step(&c); if(m.run_flag==0&&m.stop_reason!=STOP_NONE)break; }
    fprintf(stderr,"STOP=%s instr=%llu PC=%08X\n",nd500_stop_reason_str(m.stop_reason),(unsigned long long)c.instruction_count,c.PC);
    return 0;
}
