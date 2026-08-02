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
#include "testdata.h"
#define MEMSZ (16u*1024u*1024u)
static uint8_t seen[65536];
int main(int argc,char**argv){
    const char* dom=nd500_testdata("FraTor/nc/nc-a06.dom"); const char* cmd=(argc>1)?argv[1]:"CHECK B,B,B\r";
    if(ndlib_load_dom_header(dom)||ndlib_load_dom_segments())return 2;
    Nd500Machine m; Nd500Cpu c; nd500_machine_init(&m,MEMSZ); nd500_cpu_init(&c,&m); nd500_cpu_reset(&c);
    nd500_mmu_init(&c); nd500_domain_init(&c); mon_init();
    char in[128]; size_t j=0; for(size_t i=0;cmd[i]&&j<126;i++){ if(cmd[i]=='\\'&&cmd[i+1]=='r'){in[j++]='\r';i++;} else if(cmd[i]==';'&&cmd[i+1]==';'){in[j++]='\r';i++;} else in[j++]=cmd[i]; } in[j]=0; mon_queue_console_input(in);
    uint32_t sa=0; int dm=0; if(ndlib_dom_load_to_machine(&m,&c,-1,NULL,NULL,&sa,&dm))return 2; m.run_flag=1; m.stop_reason=STOP_NONE;
    long uniq=0;
    for(long s=0;s<3000000&&m.run_flag;s++){
        Nd500FetchedInstruction fi; memset(&fi,0,sizeof(fi));
        nd500_decode_at(&m,c.PC,&fi);
        uint16_t op=(uint16_t)fi.opcode;
        if(!seen[op]){seen[op]=1;uniq++;}
        nd500_cpu_step(&c);
        if(m.run_flag==0&&m.stop_reason!=STOP_NONE)break;
    }
    printf("Unique instruction opcodes executed before crash: %ld\n",uniq);
    /* dump the mnemonic set */
    for(int op=0;op<65536;op++) if(seen[op]) printf("  0x%04X\n",op);
    printf("STOP=%s instr=%llu\n",nd500_stop_reason_str(m.stop_reason),(unsigned long long)c.instruction_count);
    return 0;
}
