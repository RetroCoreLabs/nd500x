/* Probe: does 0x54312D42 ever appear in ANY user register during the NC run,
 * and what is the ACTUAL crash state?  Driving copied from diag_nc_checkpoints.c. */
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
#include "../src/disasm/nd500_disasm.h"

#define MEMORY_SIZE (16u*1024u*1024u)
#define TARGET 0x54312D42u

int main(void){
    const char* DOM="/mnt/d/ND/500/FraTor/nc/nc-a06.dom";
    if (ndlib_load_dom_header(DOM)||ndlib_load_dom_segments()){fprintf(stderr,"load fail\n");return 2;}
    Nd500Machine m; Nd500Cpu c;
    nd500_machine_init(&m,MEMORY_SIZE);
    nd500_cpu_init(&c,&m); nd500_cpu_reset(&c); nd500_mmu_init(&c); nd500_domain_init(&c); mon_init();
    mon_queue_console_input("COMPILE A,A,A\r");
    uint32_t sa=0; int dom=0;
    ndlib_dom_load_to_machine(&m,&c,-1,NULL,NULL,&sa,&dom);
    m.run_flag=1; m.stop_reason=STOP_NONE;
    long firsthit_step=-1; unsigned long long firsthit_ic=0; uint32_t firsthit_pc=0; char firsthit_where[64]="";
    for(long s=0;s<2000000L && m.run_flag;s++){
        uint32_t pc=c.PC; unsigned long long ic=c.instruction_count;
        nd500_cpu_step(&c);
        if(firsthit_step<0){
            const char* w=NULL;
            for(int i=0;i<4;i++){ if(c.I[i]==TARGET){static char b[8];snprintf(b,8,"I[%d]",i);w=b;break;} }
            if(!w) for(int i=0;i<4;i++){ if(c.A[i]==TARGET){static char b[8];snprintf(b,8,"A[%d]",i);w=b;break;} }
            if(!w) for(int i=0;i<4;i++){ if(c.E[i]==TARGET){static char b[8];snprintf(b,8,"E[%d]",i);w=b;break;} }
            if(!w){ if(c.R==TARGET)w="R"; else if(c.B==TARGET)w="B"; else if(c.L==TARGET)w="L"; else if(c.TOS==TARGET)w="TOS"; }
            if(w){ firsthit_step=s; firsthit_ic=ic; firsthit_pc=pc; strncpy(firsthit_where,w,63); }
        }
        if(m.run_flag==0 && m.stop_reason!=STOP_NONE) break;
    }
    printf("=== 0x54312D42 register scan ===\n");
    if(firsthit_step<0) printf("NEVER appeared in any of I/A/E/R/B/L/TOS during whole run.\n");
    else printf("first appeared instr#=%llu PC=0x%08X in %s\n",firsthit_ic,firsthit_pc,firsthit_where);

    char dis[256]={0};
    nd500_disasm_format_range(&m,c.PC,4,dis,sizeof(dis));
    printf("\n=== ACTUAL crash state ===\n");
    printf("stop=%s\n", nd500_stop_reason_str(m.stop_reason));
    printf("instr#=%llu PC=0x%08X\n disasm: %s\n", c.instruction_count, c.PC, dis);
    printf("B=0x%08X R=0x%08X L=0x%08X TOS=0x%08X\n",c.B,c.R,c.L,c.TOS);
    printf("I1=%08X I2=%08X I3=%08X I4=%08X\n",c.I[0],c.I[1],c.I[2],c.I[3]);
    printf("A1=%08X A2=%08X A3=%08X A4=%08X\n",c.A[0],c.A[1],c.A[2],c.A[3]);
    printf("CED=%u CAD=%u\n",c.CED,c.CAD);
    /* the faulting instruction C5 FF 73 = @b.disp indirect: pointer read from B+disp */
    Nd500FetchedInstruction fi; memset(&fi,0,sizeof fi);
    nd500_decode_at(&m,c.PC,&fi);
    for(int i=0;i<fi.operand_count;i++)
        printf(" operand[%d] mode=%d ea=0x%08X\n",i,fi.operands[i].mode,fi.operands[i].effective_address);
    return 0;
}
