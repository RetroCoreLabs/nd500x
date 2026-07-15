/* diag_domload.c - Generic DOM load+run smoke test.
 *   argv[1] = DOM path   argv[2] = console input (optional)   argv[3] = maxsteps
 * Reports the first MON calls it makes and where it stops. Used to confirm
 * cat-cat5-b06.dom (CAT-500 code generator) loads and runs under nd500x.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/nd500_mmu.h"
#include "../src/cpu/nd500_domain.h"
#include "../src/ndlib/ndlib.h"
#include "../src/libmon/mon.h"
#include "../src/libmon/mon_log.h"
#include "../src/libmon/mon_file_table.h"
#include "../src/libmon/mon_clock.h"
#define MEMSZ (16u*1024u*1024u)

static int mon_seen=0;
static void mon_cb(MonLogLevel l,const char* m){ (void)l; if(strstr(m,"CALL ")&&mon_seen<60){ printf("  %s\n",m); mon_seen++; } }

int main(int argc,char**argv){
    const char* dom=(argc>1)?argv[1]:"/mnt/d/ND/500/CAT5-CAT/cat-cat5-b06.dom";
    const char* cmd=(argc>2)?argv[2]:"";
    long maxsteps=(argc>3)?atol(argv[3]):3000000;
    if(ndlib_load_dom_header(dom)||ndlib_load_dom_segments()){fprintf(stderr,"load fail: %s\n",dom);return 2;}
    Nd500Machine m; Nd500Cpu c;
    nd500_machine_init(&m,MEMSZ); nd500_cpu_init(&c,&m); nd500_cpu_reset(&c);
    nd500_mmu_init(&c); nd500_domain_init(&c); mon_init();
    mon_log_set_callback(mon_cb); mon_log_enable(1); mon_log_set_level(MON_LOG_INFO);
    if(cmd[0]){ char in[128]; size_t j=0; for(size_t i=0;cmd[i]&&j<126;i++){ if(cmd[i]=='\\'&&cmd[i+1]=='r'){in[j++]='\r';i++;} else in[j++]=cmd[i]; } in[j]=0; mon_queue_console_input(in); }
    uint32_t sa=0; int dm=0;
    if(ndlib_dom_load_to_machine(&m,&c,-1,NULL,NULL,&sa,&dm)){fprintf(stderr,"load2 fail\n");return 2;}
    printf("Loaded '%s' entry=0x%08X\n",dom,c.PC);
    printf("First MON calls it issues:\n");
    m.run_flag=1; m.stop_reason=STOP_NONE;
    for(long s=0;s<maxsteps&&m.run_flag;s++){ nd500_cpu_step(&c); if(m.run_flag==0&&m.stop_reason!=STOP_NONE)break; }
    printf("STOP=%s instr=%llu PC=%08X\n",nd500_stop_reason_str(m.stop_reason),
           (unsigned long long)c.instruction_count,c.PC);
    return 0;
}
