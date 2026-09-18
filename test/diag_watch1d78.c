/* Watch writes to a virtual address during a linker run: log instr#/PC/old/new
 * whenever the watched word changes. Used to trace where the command-parser's
 * scan-length field [R+48]=0xB0001D78 gets its bogus 0xF80000CB. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/nd500_mmu.h"
#include "../src/cpu/nd500_domain.h"
#include "../src/ndlib/ndlib.h"
#include <ndmon/mon.h>
#include <ndmon/mon_file_table.h>   /* mon_queue_console_input */
#include "testdata.h"
#define MEMSZ (16u*1024u*1024u)
int main(int argc,char**argv){
    const char* dom=(argc>1)?argv[1]:nd500_testdata("nd-linker/linker-b01.dom");
    const char* cmd=(argc>2)?argv[2]:"LIST-STATUS;;EXIT;;";
    uint32_t watch=(argc>3)?(uint32_t)strtoul(argv[3],NULL,0):0xB0001D78u;
    if(ndlib_load_dom_header(dom)||ndlib_load_dom_segments()){fprintf(stderr,"load fail\n");return 2;}
    Nd500Machine m; Nd500Cpu c;
    nd500_machine_init(&m,MEMSZ); nd500_cpu_init(&c,&m); nd500_cpu_reset(&c);
    nd500_mmu_init(&c); nd500_domain_init(&c); mon_init();
    char inbuf[128]; size_t j=0;
    for(size_t i=0;cmd[i]&&j<126;i++){ if(cmd[i]==';'&&cmd[i+1]==';'){inbuf[j++]='\r';i++;} else inbuf[j++]=cmd[i]; }
    inbuf[j]=0; mon_queue_console_input(inbuf);
    uint32_t sa=0; int dm=0;
    if(ndlib_dom_load_to_machine(&m,&c,-1,NULL,NULL,&sa,&dm)){fprintf(stderr,"load2 fail\n");return 2;}
    m.run_flag=1; m.stop_reason=STOP_NONE;
    #define PK32(va) ({ uint32_t _p=nd500_mmu_peek(&c,(va)); (_p==0xFFFFFFFFu)?0xDEAD0000u:nd500_bus_read32(&m,_p); })
    uint32_t prev=PK32(watch); uint32_t prevPC=0;
    for(long s=0;s<3000000&&m.run_flag;s++){
        uint32_t cur=PK32(watch);
        if(cur!=prev){
            printf("WRITE watch=%08X instr=%llu byPC=%08X  %08X -> %08X\n",
                   watch,(unsigned long long)c.instruction_count,prevPC,prev,cur);
            prev=cur;
        }
        prevPC=c.PC;
        nd500_cpu_step(&c);
        if(m.run_flag==0&&m.stop_reason!=STOP_NONE)break;
    }
    fprintf(stderr,"STOP=%s instr=%llu PC=%08X\n",nd500_stop_reason_str(m.stop_reason),(unsigned long long)c.instruction_count,c.PC);
    return 0;
}
