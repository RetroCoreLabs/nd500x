#include "cpu_protos.h"
#include "machine_protos.h"
#include <stdio.h>

/**
 * HlGet instruction - MOVE class
 * 
 * Mnemonic: hl=:
 * Operands: 1
 * Opcode: 0xFDC1
 */
void nd500_instr_HlGet(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* TODO: Implement HlGet instruction
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
        printf("[STUB] HlGet instruction not implemented (mnemonic: %s, opcode: 0x%04X)\n", 
               fi->mnemonic, fi->opcode);
        warned = 1;
    }
    
    /* Stub does nothing - PC will be advanced by cpu_step() */
}
