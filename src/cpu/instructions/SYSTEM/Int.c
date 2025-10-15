#include "cpu_protos.h"
#include "machine_protos.h"
#include <stdio.h>

/**
 * Int instruction - SYSTEM class
 * 
 * Variants: 2
 * Mnemonics: int int
 * Operands: 1
 * 
 * Opcodes:
 *   0xFE60 (int)
 *   0xFE64 (int)
 */
void nd500_instr_Int(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* TODO: Implement Int instruction
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
        printf("[STUB] Int instruction not implemented (mnemonic: %s, opcode: 0x%04X)\n", 
               fi->mnemonic, fi->opcode);
        warned = 1;
    }
    
    /* Stub does nothing - PC will be advanced by cpu_step() */
}
