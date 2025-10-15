#include "cpu_protos.h"
#include "machine_protos.h"
#include <stdio.h>

/**
 * Mul2 instruction - ARITHMETIC class
 * 
 * Variants: 5
 * Mnemonics: mul2 mul2 mul2 mul2 mul2
 * Operands: 2
 * 
 * Opcodes:
 *   0xFC5D (mul2)
 *   0xFC5E (mul2)
 *   0xFC5F (mul2)
 *   0xFC60 (mul2)
 *   0xFC61 (mul2)
 */
void nd500_instr_Mul2(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* TODO: Implement Mul2 instruction
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
        printf("[STUB] Mul2 instruction not implemented (mnemonic: %s, opcode: 0x%04X)\n", 
               fi->mnemonic, fi->opcode);
        warned = 1;
    }
    
    /* Stub does nothing - PC will be advanced by cpu_step() */
}
