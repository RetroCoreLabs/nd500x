#include "cpu_protos.h"
#include "machine_protos.h"
#include <stdio.h>

/**
 * IfNotEqualGo instruction - BRANCH class
 * 
 * Variants: 2
 * Mnemonics: if><go if><go
 * Operands: 1
 * 
 * Opcodes:
 *   0x00C6 (if><go)
 *   0x00C7 (if><go)
 */
void nd500_instr_IfNotEqualGo(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* TODO: Implement IfNotEqualGo instruction
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
        printf("[STUB] IfNotEqualGo instruction not implemented (mnemonic: %s, opcode: 0x%04X)\n", 
               fi->mnemonic, fi->opcode);
        warned = 1;
    }
    
    /* Stub does nothing - PC will be advanced by cpu_step() */
}
