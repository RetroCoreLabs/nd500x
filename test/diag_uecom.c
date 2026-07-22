/* diag_uecom.c - Capture NC's LIVE, runtime-built 317B UECOM command (and 54B MDLFI
 * filenames) at GENERATE-CODE end, by decoding the [Length:4][Pointer:4] descriptor
 * at the arg address the MON layer reports. Non-invasive: does NOT modify any handler.
 *
 * Build (repo root):
 *   gcc -O2 -o build/bin/diag_uecom test/diag_uecom.c -Wl,--start-group \
 *     build/lib/libnd500_debugger.a build/lib/libnd500_cpu.a build/lib/libnd500_machine.a \
 *     build/lib/libmon.a build/lib/libnd500_ndlib.a build/lib/libnd500_disasm.a \
 *     -Wl,--end-group -lpthread -lm
 * Run FROM build/nc_sandbox:
 *   cd build/nc_sandbox && ND500X_PIN_CLOCK=1 ../bin/diag_uecom
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
#include <ndmon/mon_clock.h>

#define MEMSZ (16u*1024u*1024u)
static const char* DOM="/mnt/d/ND/500/FraTor/nc/nc-a06.dom";

static Nd500Cpu* g_cpu=NULL;

/* Replicate the MON layer's MMU-aware data reads (big-endian word). */
static uint8_t rb(uint32_t a){
    uint32_t p=a;
    if(g_cpu->machine->mmu_enabled) p=nd500_mmu_translate(g_cpu,a,0,0);
    return nd500_bus_read8(g_cpu->machine,p);
}
static uint32_t rw(uint32_t a){
    return ((uint32_t)rb(a)<<24)|((uint32_t)rb(a+1)<<16)|((uint32_t)rb(a+2)<<8)|rb(a+3);
}

/* Dump `n` raw bytes at vaddr as hex + ASCII (does NOT stop at terminators). */
static void raw_dump(const char* label,uint32_t v,int n){
    printf("    %s @0x%08X: ", label, v);
    for(int i=0;i<n;i++) printf("%02X ", rb(v+i));
    printf("| ");
    for(int i=0;i<n;i++){ uint8_t c=rb(v+i); putchar((c>=0x20&&c<0x7F)?c:'.'); }
    printf("\n");
}

static int template_dumped=0;
/* Decode + print a [len][ptr] descriptor at arg address `ea`. */
static void dump_desc(const char* tag,uint32_t ea){
    if(!template_dumped){
        printf("\n=== UECOM command TEMPLATE @0x0801DE98 (loaded by w1:=$0x801DE98 at 0x0802E11A) ===\n");
        raw_dump("template", 0x0801DE98u, 48);
        template_dumped=1;
    }
    uint32_t len=rw(ea), ptr=rw(ea+4);
    printf("\n>>> %s descriptor @0x%08X: raw8=", tag, ea);
    for(int i=0;i<8;i++) printf("%02X ", rb(ea+i));
    printf("\n    [Length=%u (0x%X), Pointer=0x%08X]\n", len, len, ptr);
    if(ptr && len && len<=4096){
        char s[4097]; uint32_t n=len<4096?len:4096; uint32_t j=0;
        for(uint32_t i=0;i<n;i++){ uint8_t c=rb(ptr+i); if(c==0x00||c==0x27||c==0xFF)break; s[j++]=(char)c; }
        s[j]=0;
        printf("    COMMAND (to terminator) = \"%s\"\n", s);
        raw_dump("full buffer", ptr, 72);   /* show any post-apostrophe params */
    } else {
        printf("    (descriptor length/pointer out of range - not read)\n");
    }
}

static uint32_t pend_ea=0; static const char* pend_tag=NULL;
static void mon_cb(MonLogLevel lvl,const char* msg){
    (void)lvl;
    const char* p=NULL; const char* tag=NULL;
    if((p=strstr(msg,"317B (UECOM) arg[0]"))) tag="317B UECOM";
    else if((p=strstr(msg,"54B (MDLFI) arg[0]"))) tag="54B MDLFI";
    if(p){
        const char* e=strstr(msg,"ea=0x");
        if(e){ pend_ea=(uint32_t)strtoul(e+5,NULL,16); pend_tag=tag; }
    }
}

int main(int argc,char**argv){
    const char* cmd=(argc>1)?argv[1]:"CHECK B,B,B\rGENERATE-CODE B,BOUT\rEXIT\r";
    if(ndlib_load_dom_header(DOM)||ndlib_load_dom_segments()){fprintf(stderr,"load fail\n");return 2;}
    Nd500Machine m; Nd500Cpu c;
    nd500_machine_init(&m,MEMSZ); nd500_cpu_init(&c,&m); nd500_cpu_reset(&c);
    nd500_mmu_init(&c); nd500_domain_init(&c); mon_init();
    g_cpu=&c;
    mon_log_set_callback(mon_cb); mon_log_enable(1); mon_log_set_level(MON_LOG_DEBUG);
    char in[128]; size_t j=0;
    for(size_t i=0;cmd[i]&&j<126;i++){ if(cmd[i]=='\\'&&cmd[i+1]=='r'){in[j++]='\r';i++;} else if(cmd[i]==';'&&cmd[i+1]==';'){in[j++]='\r';i++;} else in[j++]=cmd[i]; }
    in[j]=0; mon_queue_console_input(in);
    uint32_t sa=0; int dm=0;
    if(ndlib_dom_load_to_machine(&m,&c,-1,NULL,NULL,&sa,&dm)){fprintf(stderr,"load2 fail\n");return 2;}
    m.run_flag=1; m.stop_reason=STOP_NONE;
    for(long s=0;s<8000000&&m.run_flag;s++){
        nd500_cpu_step(&c);
        if(pend_ea){ dump_desc(pend_tag,pend_ea); pend_ea=0; pend_tag=NULL; }
        if(m.run_flag==0&&m.stop_reason!=STOP_NONE)break;
    }
    printf("\nSTOP=%s instr=%llu PC=%08X\n",nd500_stop_reason_str(m.stop_reason),
           (unsigned long long)c.instruction_count,c.PC);
    return 0;
}
