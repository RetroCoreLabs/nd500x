#include "cpu_protos.h"
#include "machine_protos.h"
#include <stdio.h>

/**
 * Loop instruction - BRANCH class
 * 
 * Variants: 10
 * Mnemonics: loop loop loop loop loop loop ...
 * Operands: 4
 * 
 * Opcodes:
 *   0xFD2D (loop)
 *   0xFD32 (loop)
 *   0xFD2E (loop)
 *   0xFD33 (loop)
 *   0xFD2F (loop)
 *   0xFD34 (loop)
 *   0xFD30 (loop)
 *   0xFD35 (loop)
 *   0xFD31 (loop)
 *   0xFD36 (loop)
 */
void nd500_instr_Loop(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* TODO: Implement Loop instruction
     * 
     * Implementation notes:
     * - Operand count: 4
     * - Access operands via: fi->operands[0..3]
     * - Use read_operand_w() / write_operand_w() helpers from cpu_instr.c
     * - Update CPU registers and FLAGS as needed
     * - PC will be advanced automatically by cpu_step()
     * 
     * Current status: STUB - Not implemented
     */
    
    static int warned = 0;
    if (!warned) {
        printf("[STUB] Loop instruction not implemented (mnemonic: %s, opcode: 0x%04X)\n", 
               fi->mnemonic, fi->opcode);
        warned = 1;
    }
    
    /* Stub does nothing - PC will be advanced by cpu_step() */
}
