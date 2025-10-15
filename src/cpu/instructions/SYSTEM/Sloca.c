#include "cpu_protos.h"
#include "machine_protos.h"
#include <stdio.h>

/**
 * Sloca instruction - SYSTEM class
 * 
 * Variants: 2
 * Mnemonics: sloca sloca
 * Operands: 2
 * 
 * Opcodes:
 *   0xFDAF (sloca)
 *   0xFDB0 (sloca)
 */
void nd500_instr_Sloca(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* TODO: Implement Sloca instruction
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
        printf("[STUB] Sloca instruction not implemented (mnemonic: %s, opcode: 0x%04X)\n", 
               fi->mnemonic, fi->opcode);
        warned = 1;
    }
    
    /* Stub does nothing - PC will be advanced by cpu_step() */
}
