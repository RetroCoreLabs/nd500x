#include "cpu_protos.h"
#include "machine_protos.h"
#include <stdio.h>

/**
 * Scomp instruction - COMPARE class
 * 
 * Mnemonic: scomp
 * Operands: 2
 * Opcode: 0xFDAC
 */
void nd500_instr_Scomp(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* TODO: Implement Scomp instruction
     * 
     * Implementation notes:
     * - Operand count: 2
     * - Access operands via: fi->operands[0..1]
     * - Use read_operand_w() / write_operand_w() helpers from cpu_instr.c
     * - Update CPU registers and FLAGS as needed
     * - PC will be advanced automatically by cpu_step()
     * 
     * Current status: STUB - Not implemented
     */
    
    static int warned = 0;
    if (!warned) {
        printf("[STUB] Scomp instruction not implemented (mnemonic: %s, opcode: 0x%04X)\n", 
               fi->mnemonic, fi->opcode);
        warned = 1;
    }
    
    /* Stub does nothing - PC will be advanced by cpu_step() */
}
