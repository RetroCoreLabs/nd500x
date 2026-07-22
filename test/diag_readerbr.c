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
#include "../src/disasm/nd500_disasm.h"
#define MEMSZ (16u*1024u*1024u)

#define RLO 0x08005024u
#define RHI 0x08005130u
#define MAILBOX 0x08000200u

int main(int argc,char**argv){
    const char* dom="/mnt/d/ND/500/FraTor/nc/nc-a06.dom";
    const char* cmd=(argc>1)?argv[1]:"CHECK B,B,B\r";
    if(ndlib_load_dom_header(dom)||ndlib_load_dom_segments())return 2;
    Nd500Machine m; Nd500Cpu c; nd500_machine_init(&m,MEMSZ); nd500_cpu_init(&c,&m); nd500_cpu_reset(&c);
    nd500_mmu_init(&c); nd500_domain_init(&c); mon_init();
    char in[128]; size_t j=0; for(size_t i=0;cmd[i]&&j<126;i++){ if(cmd[i]=='\\'&&cmd[i+1]=='r'){in[j++]='\r';i++;} else if(cmd[i]==';'&&cmd[i+1]==';'){in[j++]='\r';i++;} else in[j++]=cmd[i]; } in[j]=0; mon_queue_console_input(in);
    uint32_t sa=0; int dm=0; if(ndlib_dom_load_to_machine(&m,&c,-1,NULL,NULL,&sa,&dm))return 2;

    /* 1) static disassembly of the reader routine */
    static char buf[16384];
    nd500_disasm_format_range(&m, RLO, RHI-RLO, buf, sizeof buf);
    printf("=== reader routine disasm 0x%08X..0x%08X ===\n%s\n", RLO, RHI, buf);

    m.run_flag=1; m.stop_reason=STOP_NONE;
    long entry=0;
    int trace=0;
    for(long s=0;s<3000000&&m.run_flag;s++){
        uint32_t pc=c.PC;
        unsigned long long ic=c.instruction_count;
        if(pc==RLO){
            entry++;
            /* start tracing only for invocations near the critical window */
            trace = (ic>1150000ULL && ic<1210000ULL);
            if(trace) printf("\n--- reader entry #%ld instr=%llu mailbox[0x200]=%08X ---\n",
                             entry, ic, nd500_bus_read32(&m, MAILBOX));
        }
        if(trace && pc>=RLO && pc<RHI){
            char one[256]; nd500_disasm_format_range(&m, pc, 8, one, sizeof one);
            /* strip newline */
            char*nl=strchr(one,'\n'); if(nl)*nl=0;
            printf("  ic=%llu PC=%08X ST1=%08X I1=%08X | %s\n",
                   ic, pc, c.ST1, c.I[0], one);
        }
        nd500_cpu_step(&c);
        if(m.run_flag==0&&m.stop_reason!=STOP_NONE)break;
    }
    printf("\nSTOP=%s instr=%llu entries=%ld\n",nd500_stop_reason_str(m.stop_reason),(unsigned long long)c.instruction_count,entry);
    return 0;
}
