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
#define T 0x1802A1B0u
/* free wrapper 0x0802CEB3: size arg in b.0x18. Capture size at free of T, and
   GETB alloc-return of T with its requested size (arg passed to 0x802CF08). */
static uint32_t rd(Nd500Cpu*c,uint32_t v){ if(c->machine&&c->machine->mmu_enabled){uint32_t p=nd500_mmu_translate(c,v,0,0); if(nd500_trap_occurred())return 0xDEAD; if(p+4>=MEMSZ)return 0xBAD; return nd500_bus_read32(c->machine,p);} return nd500_bus_read32(c->machine,v);}
int main(int argc,char**argv){
    const char* dom="/mnt/d/ND/500/FraTor/nc/nc-a06.dom"; const char* cmd=(argc>1)?argv[1]:"CHECK B,B,B\r";
    if(ndlib_load_dom_header(dom)||ndlib_load_dom_segments())return 2;
    Nd500Machine m; Nd500Cpu c; nd500_machine_init(&m,MEMSZ); nd500_cpu_init(&c,&m); nd500_cpu_reset(&c);
    nd500_mmu_init(&c); nd500_domain_init(&c); mon_init();
    char in[128]; size_t j=0; for(size_t i=0;cmd[i]&&j<126;i++){ if(cmd[i]=='\\'&&cmd[i+1]=='r'){in[j++]='\r';i++;} else if(cmd[i]==';'&&cmd[i+1]==';'){in[j++]='\r';i++;} else in[j++]=cmd[i]; } in[j]=0; mon_queue_console_input(in);
    uint32_t sa=0; int dm=0; if(ndlib_dom_load_to_machine(&m,&c,-1,NULL,NULL,&sa,&dm))return 2; m.run_flag=1; m.stop_reason=STOP_NONE;
    uint32_t lastGETBsize=0;
    for(long s=0;s<3000000&&m.run_flag;s++){
        uint32_t pc=c.PC; unsigned long long ic=c.instruction_count;
        /* free wrapper entry 0x0802CEB3: node ptr in arg (b.0x14 after ents); size in b.0x18.
           But easier: freelist push 0x0802CEFE frees node=I[1]; the free-wrapper computed bucket.
           Capture at 0x0802CEB3 entry: arg node = mem[caller]; approximate via later. */
        if(pc==0x0802CF08u){ lastGETBsize=c.I[0]; } /* GETB called: size hint often in I0 or arg */
        if(pc==0x0802CF07u && c.I[0]==T){ /* GETB returned T */
            printf("%llu ALLOC T=0x1802A1B0 (GETB) prevSizeHint=0x%X\n", ic, lastGETBsize); }
        if(pc==0x0802CEFEu && c.I[1]==T){ /* free push of T; bucket head slot in b.0x24 */
            printf("%llu FREE  T=0x1802A1B0 (push)  headSlot(w1=@b.0x24 old head)=captured-below\n", ic); }
        nd500_cpu_step(&c);
        if(m.run_flag==0&&m.stop_reason!=STOP_NONE)break;
    }
    printf("STOP=%s\n",nd500_stop_reason_str(m.stop_reason)); return 0;
}
