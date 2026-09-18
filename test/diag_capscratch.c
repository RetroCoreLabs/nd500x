/* diag_capscratch.c - Run NC (compile B,B,BOUT) and CAPTURE the CAT intermediate
 * that NC writes to the always-open scratch file (file 0100 octal = SCRATCHnn:DATA,
 * backed by ./SCRATCH/SCRATCH64.DATA) BEFORE the atexit cleanup deletes it.
 * This is step 1 of the manual NC->CAT-500 chain proof.
 * Run FROM build/nc_sandbox.
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
#include <ndmon/mon_file_table.h>
#include <ndmon/mon_clock.h>
#include "testdata.h"
#define MEMSZ (16u*1024u*1024u)
static const char* DOM; /* assigned in main - nd500_testdata() is a call, not a constant */

static long filesize(const char* p){ FILE* f=fopen(p,"rb"); if(!f)return -1; fseek(f,0,SEEK_END); long n=ftell(f); fclose(f); return n; }

int main(int argc,char**argv){
    DOM = nd500_testdata("FraTor/nc/nc-a06.dom");
    const char* cmd=(argc>1)?argv[1]:"COMPILE B,B,BOUT\rEXIT\r";
    if(ndlib_load_dom_header(DOM)||ndlib_load_dom_segments()){fprintf(stderr,"load fail\n");return 2;}
    Nd500Machine m; Nd500Cpu c;
    nd500_machine_init(&m,MEMSZ); nd500_cpu_init(&c,&m); nd500_cpu_reset(&c);
    nd500_mmu_init(&c); nd500_domain_init(&c); mon_init();
    char in[128]; size_t j=0;
    for(size_t i=0;cmd[i]&&j<126;i++){ if(cmd[i]=='\\'&&cmd[i+1]=='r'){in[j++]='\r';i++;} else in[j++]=cmd[i]; } in[j]=0;
    mon_queue_console_input(in);
    uint32_t sa=0; int dm=0;
    if(ndlib_dom_load_to_machine(&m,&c,-1,NULL,NULL,&sa,&dm)){fprintf(stderr,"load2 fail\n");return 2;}
    m.run_flag=1; m.stop_reason=STOP_NONE;
    for(long s=0;s<8000000&&m.run_flag;s++){ nd500_cpu_step(&c); if(m.run_flag==0&&m.stop_reason!=STOP_NONE)break; }
    printf("NC STOP=%s instr=%llu\n",nd500_stop_reason_str(m.stop_reason),(unsigned long long)c.instruction_count);

    /* Capture the scratch BEFORE process exit (atexit deletes it). cwd = build/nc_sandbox */
    const char* scr="./SCRATCH/SCRATCH64.DATA";
    long n=filesize(scr);
    printf("\n=== CAT intermediate ./SCRATCH/SCRATCH64.DATA : size=%ld ===\n", n);
    if(n>0){
        FILE* f=fopen(scr,"rb"); FILE* o=fopen("./cat_intermediate.dat","wb");
        if(f&&o){ char buf[8192]; size_t r; while((r=fread(buf,1,sizeof(buf),f))>0) fwrite(buf,1,r,o); }
        if(f) { fclose(f); }
        if(o) { fclose(o); }
        printf("copied to ./cat_intermediate.dat\n");
        /* hexdump head */
        FILE* h=fopen(scr,"rb"); if(h){ unsigned char b[64]; size_t r=fread(b,1,64,h); fclose(h);
            printf("head:"); for(size_t i=0;i<r;i++){ if(i%16==0)printf("\n  "); printf("%02X ",b[i]); } printf("\n"); }
    } else {
        printf("(scratch not present or empty - check auto-scratch-64 / file name)\n");
    }
    /* Also list what else is in SCRATCH/ and GUEST/ */
    printf("\n=== SCRATCH/ + GUEST NRF ===\n"); fflush(stdout);
    system("ls -la SCRATCH/ GUEST/*.NRF 2>/dev/null");
    return 0;
}
