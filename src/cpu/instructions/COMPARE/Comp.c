#include "cpu_protos.h"
#include "machine_protos.h"
#include <stdio.h>

/**
 * Comp instruction - COMPARE class
 * 
 * Variants: 6
 * Mnemonics: comp comp comp comp comp comp
 * Operands: 1
 * 
 * Opcodes:
 *   0xFC18 (comp)
 *   0x0030 (comp)
 *   0xFC1C (comp)
 *   0x0034 (comp)
 *   0x0038 (comp)
 *   0x003C (comp)
 */
void nd500_instr_Comp(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* TODO: Implement Comp instruction
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
        printf("[STUB] Comp instruction not implemented (mnemonic: %s, opcode: 0x%04X)\n", 
               fi->mnemonic, fi->opcode);
        warned = 1;
    }
    
    /* Stub does nothing - PC will be advanced by cpu_step() */
}
