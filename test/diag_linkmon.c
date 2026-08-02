/*
 * Diagnostic: MON trace for the ND LINKER (or any DOM), for Phase 2.
 * Unlike diag_monlog (which hardcodes the NC dom), this takes the DOM path and
 * command string on argv so the linker can be driven with a known-good NRF.
 *
 * argv: <dom-path> <cmd>  (cmd: ";;" or "\r" -> CR)
 * Run PINNED from a sandbox dir:  ND500X_PIN_CLOCK=1 ../bin/diag_linkmon <dom> "<cmd>"
 * Build: gcc -O2 -o build/bin/diag_linkmon test/diag_linkmon.c \
 *   -Wl,--start-group build/lib/libnd500_debugger.a build/lib/libnd500_cpu.a \
 *   build/lib/libnd500_machine.a build/lib/libmon.a build/lib/libnd500_ndlib.a \
 *   build/lib/libnd500_disasm.a -Wl,--end-group -lpthread -lm
 */
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
#include "testdata.h"

#define MEMSZ (16u*1024u*1024u)

int main(int argc,char**argv){
    const char* dom = (argc>1)?argv[1]:nd500_testdata("nd-linker/linker-b01.dom");
    const char* cmd = (argc>2)?argv[2]:"EXIT;;";
    long max_steps  = (argc>3)?strtol(argv[3],NULL,0):3000000L;

    if(ndlib_load_dom_header(dom)||ndlib_load_dom_segments()){fprintf(stderr,"load fail\n");return 2;}
    Nd500Machine m; Nd500Cpu c;
    nd500_machine_init(&m,MEMSZ); nd500_cpu_init(&c,&m); nd500_cpu_reset(&c);
    nd500_mmu_init(&c); nd500_domain_init(&c); mon_init();
    if (!getenv("ND500X_NOLOG")) { mon_log_enable(1); mon_log_set_level(MON_LOG_DEBUG); }
    char inbuf[256]; size_t j=0;
    for(size_t i=0;cmd[i]&&j<254;i++){ if(cmd[i]=='\\'&&cmd[i+1]=='r'){inbuf[j++]='\r';i++;} else if(cmd[i]==';'&&cmd[i+1]==';'){inbuf[j++]='\r';i++;} else inbuf[j++]=cmd[i]; }
    inbuf[j]=0; mon_queue_console_input(inbuf);
    /* The ND LINKER reads commands via 1B INBT device 0 (SINTRAN command buffer)
     * AND the terminal (503B). Populating the command buffer puts it in BATCH mode
     * (which triggers a "Batch abortion" prompt at end-of-buffer). Gate it so we can
     * test interactive-only (console/503B) vs batch (command buffer). */
    if (getenv("ND500X_LINK_CMDBUF")) mon_set_command_buffer(inbuf);
    uint32_t sa=0; int dm=0;
    if(ndlib_dom_load_to_machine(&m,&c,-1,NULL,NULL,&sa,&dm)){fprintf(stderr,"load2 fail\n");return 2;}
    m.run_flag=1; m.stop_reason=STOP_NONE;
    int peek_tick = (getenv("ND500X_PEEK_TICK") != NULL);
    for(long s=0;s<max_steps&&m.run_flag;s++){
        if (peek_tick) { uint32_t _p=nd500_mmu_peek(&c,0xB0030DE8u); (void)_p; }
        nd500_cpu_step(&c);
        if(m.run_flag==0&&m.stop_reason!=STOP_NONE)break;
    }
    fprintf(stderr,"STOP=%s instr=%llu PC=%08X\n",nd500_stop_reason_str(m.stop_reason),(unsigned long long)c.instruction_count,c.PC);
    { const char* co=mon_get_console_output(); size_t cl=mon_get_console_output_len();
      fprintf(stderr,"=== LINKER CONSOLE (%zu bytes) ===\n",cl);
      if(co) fwrite(co,1,cl,stderr); fprintf(stderr,"\n=== end ===\n"); }
    return 0;
}
