#include "cpu_protos.h"
#include "machine_protos.h"
#include <stdio.h>

/**
 * Loopi instruction - BRANCH class
 * 
 * Variants: 10
 * Mnemonics: loopi loopi loopi loopi loopi loopi ...
 * Operands: 3
 * 
 * Opcodes:
 *   0xFCDE (loopi)
 *   0xFD1E (loopi)
 *   0xFCDF (loopi)
 *   0xFD1F (loopi)
 *   0x00BF (loopi)
 *   0x00E1 (loopi)
 *   0xFD1C (loopi)
 *   0xFD21 (loopi)
 *   0xFD1D (loopi)
 *   0xFD22 (loopi)
 */
void nd500_instr_Loopi(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* TODO: Implement Loopi instruction
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
        printf("[STUB] Loopi instruction not implemented (mnemonic: %s, opcode: 0x%04X)\n", 
               fi->mnemonic, fi->opcode);
        warned = 1;
    }
    
    /* Stub does nothing - PC will be advanced by cpu_step() */
}
