#include "cpu_protos.h"
#include "machine_protos.h"
#include <stdio.h>

/**
 * Loopd instruction - BRANCH class
 * 
 * Variants: 10
 * Mnemonics: loopd loopd loopd loopd loopd loopd ...
 * Operands: 3
 * 
 * Opcodes:
 *   0xFD23 (loopd)
 *   0xFD28 (loopd)
 *   0xFD24 (loopd)
 *   0xFD29 (loopd)
 *   0xFD25 (loopd)
 *   0xFD2A (loopd)
 *   0xFD26 (loopd)
 *   0xFD2B (loopd)
 *   0xFD27 (loopd)
 *   0xFD2C (loopd)
 */
void nd500_instr_Loopd(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* TODO: Implement Loopd instruction
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
        printf("[STUB] Loopd instruction not implemented (mnemonic: %s, opcode: 0x%04X)\n", 
               fi->mnemonic, fi->opcode);
        warned = 1;
    }
    
    /* Stub does nothing - PC will be advanced by cpu_step() */
}
