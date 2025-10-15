#include "cpu_protos.h"
#include "machine_protos.h"
#include <stdio.h>

/**
 * Biconv instruction - FLOAT_MATH class
 * 
 * Variants: 5
 * Mnemonics: biconv biconv biconv biconv biconv
 * Operands: 2
 * 
 * Opcodes:
 *   0xFD49 (biconv)
 *   0xFD4E (biconv)
 *   0xFD53 (biconv)
 *   0xFD58 (biconv)
 *   0xFD5D (biconv)
 */
void nd500_instr_Biconv(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* TODO: Implement Biconv instruction
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
        printf("[STUB] Biconv instruction not implemented (mnemonic: %s, opcode: 0x%04X)\n", 
               fi->mnemonic, fi->opcode);
        warned = 1;
    }
    
    /* Stub does nothing - PC will be advanced by cpu_step() */
}
