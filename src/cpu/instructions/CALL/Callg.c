#include "cpu_protos.h"
#include "machine_protos.h"
#include <stdio.h>

/**
 * Callg instruction - CALL class
 * 
 * Mnemonic: callg
 * Operands: 2
 * Opcode: 0x00B5
 */
void nd500_instr_Callg(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* TODO: Implement Callg instruction
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
        printf("[STUB] Callg instruction not implemented (mnemonic: %s, opcode: 0x%04X)\n", 
               fi->mnemonic, fi->opcode);
        warned = 1;
    }
    
    /* Stub does nothing - PC will be advanced by cpu_step() */
}
