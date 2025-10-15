#include "cpu_protos.h"
#include "machine_protos.h"
#include <stdio.h>

/**
 * Umul instruction - ARITHMETIC class
 * 
 * Mnemonic: umul
 * Operands: 3
 * Opcode: 0xFC80
 */
void nd500_instr_Umul(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* TODO: Implement Umul instruction
     * 
     * Implementation notes:
     * - Operand count: 3
     * - Access operands via: fi->operands[0..2]
     * - Use read_operand_w() / write_operand_w() helpers from cpu_instr.c
     * - Update CPU registers and FLAGS as needed
     * - PC will be advanced automatically by cpu_step()
     * 
     * Current status: STUB - Not implemented
     */
    
    static int warned = 0;
    if (!warned) {
        printf("[STUB] Umul instruction not implemented (mnemonic: %s, opcode: 0x%04X)\n", 
               fi->mnemonic, fi->opcode);
        warned = 1;
    }
    
    /* Stub does nothing - PC will be advanced by cpu_step() */
}
