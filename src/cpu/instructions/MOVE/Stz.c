#include "cpu_protos.h"
#include "machine_protos.h"
#include <stdio.h>

/**
 * Stz instruction - MOVE class
 * 
 * Variants: 6
 * Mnemonics: stz stz stz stz stz stz
 * Operands: 1
 * 
 * Opcodes:
 *   0xFC85 (stz)
 *   0x0048 (stz)
 *   0x0049 (stz)
 *   0x004A (stz)
 *   0x004B (stz)
 *   0x004C (stz)
 */
void nd500_instr_Stz(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* TODO: Implement Stz instruction
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
        printf("[STUB] Stz instruction not implemented (mnemonic: %s, opcode: 0x%04X)\n", 
               fi->mnemonic, fi->opcode);
        warned = 1;
    }
    
    /* Stub does nothing - PC will be advanced by cpu_step() */
}
