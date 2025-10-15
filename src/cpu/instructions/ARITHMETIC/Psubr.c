#include "cpu_protos.h"
#include "machine_protos.h"
#include <stdio.h>

/**
 * Psubr instruction - ARITHMETIC class
 * 
 * Mnemonic: psubr
 * Operands: 3
 * Opcode: 0xFE86
 */
void nd500_instr_Psubr(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* TODO: Implement Psubr instruction
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
        printf("[STUB] Psubr instruction not implemented (mnemonic: %s, opcode: 0x%04X)\n", 
               fi->mnemonic, fi->opcode);
        warned = 1;
    }
    
    /* Stub does nothing - PC will be advanced by cpu_step() */
}
