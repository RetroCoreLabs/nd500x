#include "cpu_protos.h"
#include "machine_protos.h"
#include <stdio.h>

/**
 * Abs instruction - ARITHMETIC class
 * 
 * Variants: 5
 * Mnemonics: abs abs abs abs abs
 * Operands: 0
 * 
 * Opcodes:
 *   0xFF00 (abs)
 *   0xFF04 (abs)
 *   0xFF08 (abs)
 *   0xFF0C (abs)
 *   0xFF0C (abs)
 */
void nd500_instr_Abs(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* TODO: Implement Abs instruction
     * 
     * Implementation notes:
     * - Operand count: 0
     * - Access operands via: fi->operands[0..-1]
     * - Use read_operand_w() / write_operand_w() helpers from cpu_instr.c
     * - Update CPU registers and FLAGS as needed
     * - PC will be advanced automatically by cpu_step()
     * 
     * Current status: STUB - Not implemented
     */
    
    static int warned = 0;
    if (!warned) {
        printf("[STUB] Abs instruction not implemented (mnemonic: %s, opcode: 0x%04X)\n", 
               fi->mnemonic, fi->opcode);
        warned = 1;
    }
    
    /* Stub does nothing - PC will be advanced by cpu_step() */
}
