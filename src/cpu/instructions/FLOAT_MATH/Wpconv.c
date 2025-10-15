#include "cpu_protos.h"
#include "machine_protos.h"
#include <stdio.h>

/**
 * Wpconv instruction - FLOAT_MATH class
 * 
 * Mnemonic: wpconv
 * Operands: 1
 * Opcode: 0xFEB8
 */
void nd500_instr_Wpconv(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* TODO: Implement Wpconv instruction
     * 
     * Implementation notes:
     * - Operand count: 1
     * - Access operands via: fi->operands[0..0]
     * - Use read_operand_w() / write_operand_w() helpers from cpu_instr.c
     * - Update CPU registers and FLAGS as needed
     * - PC will be advanced automatically by cpu_step()
     * 
     * Current status: STUB - Not implemented
     */
    
    static int warned = 0;
    if (!warned) {
        printf("[STUB] Wpconv instruction not implemented (mnemonic: %s, opcode: 0x%04X)\n", 
               fi->mnemonic, fi->opcode);
        warned = 1;
    }
    
    /* Stub does nothing - PC will be advanced by cpu_step() */
}
