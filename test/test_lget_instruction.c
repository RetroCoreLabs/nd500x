/*
 * Test LGet (l=:) instruction - L register store to memory
 *
 * Tests that the l=: instruction correctly writes the L register value
 * to the destination memory address.
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/cpu/nd500_instructions.h"
#include "../src/cpu/nd500_mmu.h"
#include "../src/machine/machine_protos.h"

/* External LGet function */
extern void nd500_instr_LGet(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi);

void test_lget_basic() {
    printf("=== Test 1: LGet Basic (Absolute Addressing) ===\n");

    /* Create machine with enough memory */
    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, 0x100000);  /* 1MB memory */
    nd500_cpu_init(&cpu, &m);

    /* Set L register to a known value */
    cpu.L = 0xDEADBEEF;
    printf("  L register = 0x%08X\n", cpu.L);

    /* Target address for write */
    uint32_t target_addr = 0x1000;

    /* Initialize target memory to zero */
    m.memory[target_addr + 0] = 0x00;
    m.memory[target_addr + 1] = 0x00;
    m.memory[target_addr + 2] = 0x00;
    m.memory[target_addr + 3] = 0x00;
    printf("  Target address = 0x%08X\n", target_addr);
    printf("  Memory before: 0x%02X%02X%02X%02X\n",
           m.memory[target_addr], m.memory[target_addr+1],
           m.memory[target_addr+2], m.memory[target_addr+3]);

    /* Construct LGet instruction: FD C0 C4 00 00 10 00
     * FD C0 = opcode for l=:
     * C4    = ABSOLUTE addressing mode (word)
     * 00 00 10 00 = absolute address 0x00001000 (big-endian)
     */
    uint8_t code[] = {
        0xFD, 0xC0,           /* opcode: l=: */
        0xC4,                 /* address code: ABSOLUTE word */
        0x00, 0x00, 0x10, 0x00  /* address: 0x00001000 (big-endian) */
    };
    memcpy(m.memory, code, sizeof(code));
    cpu.PC = 0;

    printf("  Instruction bytes: ");
    for (size_t i = 0; i < sizeof(code); i++) {
        printf("%02X ", code[i]);
    }
    printf("\n");

    /* Decode the instruction */
    Nd500FetchedInstruction fi;
    int decode_result = nd500_decode_at(&m, 0, &fi);
    printf("  Decode result: %d\n", decode_result);
    printf("  Mnemonic: %s\n", fi.mnemonic);
    printf("  Opcode: 0x%04X\n", fi.opcode);
    printf("  Operand count: %d\n", fi.operand_count);
    printf("  Total length: %d bytes\n", fi.total_len);

    if (fi.operand_count > 0) {
        printf("  Operand 0:\n");
        printf("    mode: %d\n", fi.operands[0].mode);
        printf("    address_code: 0x%02X\n", fi.operands[0].address_code);
        printf("    data_len: %d\n", fi.operands[0].data_len);
        printf("    data: ");
        for (int i = 0; i < fi.operands[0].data_len; i++) {
            printf("%02X ", fi.operands[0].data[i]);
        }
        printf("\n");
        printf("    effective_address: 0x%08X\n", fi.operands[0].effective_address);
    }

    /* Execute the instruction */
    printf("  Executing instruction...\n");

    /* Check if dispatch table has the LGet implementation */
    InstrExecFunc func = g_instr_exec_table[fi.opcode];
    printf("  Dispatch function: %p\n", (void*)func);

    if (func) {
        func(&cpu, &fi);
    } else {
        printf("  ERROR: No dispatch function for opcode 0x%04X!\n", fi.opcode);
        nd500_machine_free(&m);
        return;
    }

    /* Read memory at target address */
    uint32_t mem_value = ((uint32_t)m.memory[target_addr] << 24) |
                         ((uint32_t)m.memory[target_addr+1] << 16) |
                         ((uint32_t)m.memory[target_addr+2] << 8) |
                         (uint32_t)m.memory[target_addr+3];

    printf("  Memory after:  0x%02X%02X%02X%02X (0x%08X)\n",
           m.memory[target_addr], m.memory[target_addr+1],
           m.memory[target_addr+2], m.memory[target_addr+3],
           mem_value);
    printf("  Expected:      0x%08X\n", cpu.L);

    if (mem_value == cpu.L) {
        printf("  PASS: L register correctly written to memory\n");
    } else {
        printf("  FAIL: Memory value does not match L register!\n");
        printf("  Difference: expected 0x%08X, got 0x%08X\n", cpu.L, mem_value);
    }

    /* Cleanup */
    nd500_machine_free(&m);
    printf("\n");
}

void test_lget_local_short() {
    printf("=== Test 2: LGet with LOCAL_SHORT Addressing ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, 0x100000);
    nd500_cpu_init(&cpu, &m);

    /* Set L and B registers */
    cpu.L = 0x12345678;
    cpu.B = 0x2000;

    /* Target is B + offset*4 = 0x2000 + 5*4 = 0x2014 */
    uint32_t target_addr = cpu.B + (5 * 4);
    printf("  L = 0x%08X, B = 0x%08X\n", cpu.L, cpu.B);
    printf("  Target (B + 5*4) = 0x%08X\n", target_addr);

    /* Clear target memory */
    m.memory[target_addr + 0] = 0x00;
    m.memory[target_addr + 1] = 0x00;
    m.memory[target_addr + 2] = 0x00;
    m.memory[target_addr + 3] = 0x00;

    /* Construct LGet with LOCAL_SHORT: FD C0 45
     * FD C0 = opcode for l=:
     * 45    = LOCAL_SHORT, offset 5 (01 prefix + 5)
     */
    uint8_t code[] = {
        0xFD, 0xC0,   /* opcode: l=: */
        0x45          /* LOCAL_SHORT: b.5 */
    };
    memcpy(m.memory, code, sizeof(code));
    cpu.PC = 0;

    printf("  Instruction: FD C0 45 (l=: b.5)\n");

    Nd500FetchedInstruction fi;
    nd500_decode_at(&m, 0, &fi);

    printf("  Operand 0 mode: %d, effective_address: 0x%08X\n",
           fi.operands[0].mode, fi.operands[0].effective_address);

    InstrExecFunc func = g_instr_exec_table[fi.opcode];
    if (func) {
        func(&cpu, &fi);
    }

    uint32_t mem_value = ((uint32_t)m.memory[target_addr] << 24) |
                         ((uint32_t)m.memory[target_addr+1] << 16) |
                         ((uint32_t)m.memory[target_addr+2] << 8) |
                         (uint32_t)m.memory[target_addr+3];

    printf("  Memory at 0x%08X: 0x%08X\n", target_addr, mem_value);
    printf("  Expected: 0x%08X\n", cpu.L);

    if (mem_value == cpu.L) {
        printf("  PASS\n");
    } else {
        printf("  FAIL\n");
    }

    nd500_machine_free(&m);
    printf("\n");
}

void test_lget_register() {
    printf("=== Test 3: LGet with REGISTER Addressing ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, 0x100000);
    nd500_cpu_init(&cpu, &m);

    /* Set L register */
    cpu.L = 0xCAFEBABE;
    cpu.I[0] = 0x00000000;  /* I1 before */

    printf("  L = 0x%08X, I1 before = 0x%08X\n", cpu.L, cpu.I[0]);

    /* Construct LGet with REGISTER: FD C0 D0
     * FD C0 = opcode for l=:
     * D0    = REGISTER I1
     */
    uint8_t code[] = {
        0xFD, 0xC0,   /* opcode: l=: */
        0xD0          /* REGISTER: I1 */
    };
    memcpy(m.memory, code, sizeof(code));
    cpu.PC = 0;

    printf("  Instruction: FD C0 D0 (l=: i1)\n");

    Nd500FetchedInstruction fi;
    nd500_decode_at(&m, 0, &fi);

    printf("  Operand 0 mode: %d, reg: %d\n", fi.operands[0].mode, fi.operands[0].reg);

    InstrExecFunc func = g_instr_exec_table[fi.opcode];
    if (func) {
        func(&cpu, &fi);
    }

    printf("  I1 after = 0x%08X\n", cpu.I[0]);
    printf("  Expected: 0x%08X\n", cpu.L);

    if (cpu.I[0] == cpu.L) {
        printf("  PASS\n");
    } else {
        printf("  FAIL\n");
    }

    nd500_machine_free(&m);
    printf("\n");
}

void test_lget_debug() {
    printf("=== Debug Test: LGet Step by Step ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, 0x100000);
    nd500_cpu_init(&cpu, &m);

    /* Match the user's actual scenario */
    cpu.L = 0x0801D7F0;  /* Some value */

    uint32_t target_addr = 0x1000;

    /* Clear target memory */
    for (int i = 0; i < 8; i++) {
        m.memory[target_addr + i] = 0x00;
    }

    printf("  L = 0x%08X\n", cpu.L);
    printf("  Target addr = 0x%08X\n", target_addr);

    /* Construct instruction */
    uint8_t code[] = {
        0xFD, 0xC0,
        0xC4,
        0x00, 0x00, 0x10, 0x00
    };
    memcpy(m.memory, code, sizeof(code));
    cpu.PC = 0;

    Nd500FetchedInstruction fi;
    nd500_decode_at(&m, 0, &fi);

    printf("  Decoded:\n");
    printf("    opcode: 0x%04X\n", fi.opcode);
    printf("    mnemonic: %s\n", fi.mnemonic);
    printf("    operand_count: %d\n", fi.operand_count);
    printf("    op[0].mode: %d (expected 3=ABSOLUTE)\n", fi.operands[0].mode);
    printf("    op[0].effective_address: 0x%08X\n", fi.operands[0].effective_address);
    printf("    op[0].data_len: %d\n", fi.operands[0].data_len);
    printf("    op[0].data: %02X %02X %02X %02X\n",
           fi.operands[0].data[0], fi.operands[0].data[1],
           fi.operands[0].data[2], fi.operands[0].data[3]);

    /* Check ND500_ADDR_ABSOLUTE constant */
    printf("    ND500_ADDR_ABSOLUTE = %d\n", ND500_ADDR_ABSOLUTE);

    /* Execute */
    InstrExecFunc func = g_instr_exec_table[fi.opcode];
    printf("  Dispatch function: %p\n", (void*)func);

    if (func) {
        printf("  Executing...\n");
        func(&cpu, &fi);
    } else {
        printf("  NO DISPATCH FUNCTION!\n");
    }

    /* Check memory */
    uint32_t mem_value = ((uint32_t)m.memory[target_addr] << 24) |
                         ((uint32_t)m.memory[target_addr+1] << 16) |
                         ((uint32_t)m.memory[target_addr+2] << 8) |
                         (uint32_t)m.memory[target_addr+3];

    printf("  Memory bytes: %02X %02X %02X %02X\n",
           m.memory[target_addr], m.memory[target_addr+1],
           m.memory[target_addr+2], m.memory[target_addr+3]);
    printf("  Memory value: 0x%08X\n", mem_value);
    printf("  Expected: 0x%08X\n", cpu.L);

    if (mem_value == cpu.L) {
        printf("  PASS\n");
    } else {
        printf("  FAIL - Memory not written correctly!\n");
    }

    nd500_machine_free(&m);
    printf("\n");
}

void test_lget_with_mmu() {
    printf("=== Test 4: LGet with MMU Enabled (Data MMU) ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, 0x200000);  /* 2MB memory */
    nd500_cpu_init(&cpu, &m);
    nd500_cpu_reset(&cpu);

    /* Initialize MMU */
    nd500_mmu_init(&cpu);

    /* Set L register to a known value */
    cpu.L = 0xDEADBEEF;
    printf("  L = 0x%08X\n", cpu.L);

    /*
     * Virtual address 0x0801D7F0 breakdown:
     *   Segment = (0x0801D7F0 >> 27) & 0x1F = 1
     *   Page    = (0x0801D7F0 >> 11) & 0xFFFF = 0x400EB (262379)
     *   Offset  = 0x0801D7F0 & 0x7FF = 0x7F0 (2032)
     *
     * For this test, we'll use a simpler address that fits our memory:
     * Virtual address 0x08001000 (segment 1, page 0, offset 0)
     */
    uint32_t virtual_addr = 0x08001000;
    uint32_t physical_addr = 0x00010000;  /* Physical address where we want data to go */

    printf("  Virtual addr:  0x%08X\n", virtual_addr);
    printf("  Physical addr: 0x%08X (where we expect data to be written)\n", physical_addr);

    /* Set up MMU for segment 1 (virtual addresses 0x08000000-0x0FFFFFFF):
     * 1. Set PST entry with PS_AZI (direct mode) pointing to physical page
     * 2. Set data capability for domain 0, segment 1
     */
    int psn = 100;  /* PST entry number */

    /* Physical PFN = physical_addr >> 11 (PGSHIFT) */
    uint32_t physical_pfn = physical_addr >> 11;
    printf("  PST[%d]: index_mode=PS_AZI, pfn=0x%X\n", psn, physical_pfn);

    nd500_mmu_set_pst_entry(&cpu, psn, PS_AZI, physical_pfn);

    /* Set data capability for domain 0, segment 1 (bits 27-31 of virtual addr = 1)
     * Capability value: PSN (lower 13 bits) + flags
     * We need it writable, so don't set DC_WRP
     */
    int segment = (virtual_addr >> 27) & 0x1F;  /* = 1 */
    uint16_t dc_value = psn;  /* PSN in lower bits, no write-protect */
    printf("  Data capability for domain 0, segment %d: PSN=%d (0x%04X)\n", segment, psn, dc_value);

    nd500_mmu_set_data_capability(&cpu, 0, segment, dc_value);

    /* Set current domain (CAD) */
    cpu.CAD = 0;
    printf("  CPU.CAD = %d (current domain)\n", cpu.CAD);

    /* Clear target memory at PHYSICAL address */
    m.memory[physical_addr + 0] = 0x00;
    m.memory[physical_addr + 1] = 0x00;
    m.memory[physical_addr + 2] = 0x00;
    m.memory[physical_addr + 3] = 0x00;

    /* Also clear at VIRTUAL address (if it falls in our memory range) */
    if (virtual_addr < m.memory_size - 4) {
        m.memory[virtual_addr + 0] = 0x00;
        m.memory[virtual_addr + 1] = 0x00;
        m.memory[virtual_addr + 2] = 0x00;
        m.memory[virtual_addr + 3] = 0x00;
    }

    printf("  Memory before @ physical 0x%08X: %02X %02X %02X %02X\n",
           physical_addr,
           m.memory[physical_addr], m.memory[physical_addr+1],
           m.memory[physical_addr+2], m.memory[physical_addr+3]);

    /* Enable DATA MMU only (not program MMU, so we can still execute from low memory) */
    nd500_mmu_enable_data(&cpu);
    printf("  Data MMU: %s\n", nd500_mmu_is_data_enabled(&cpu) ? "ENABLED" : "disabled");

    /* Construct LGet instruction with virtual address as target:
     * FD C0 = l=:
     * C4 = ABSOLUTE addressing
     * 08 00 10 00 = 0x08001000 (big-endian)
     */
    uint8_t code[] = {
        0xFD, 0xC0,
        0xC4,
        (virtual_addr >> 24) & 0xFF,
        (virtual_addr >> 16) & 0xFF,
        (virtual_addr >> 8) & 0xFF,
        virtual_addr & 0xFF
    };
    memcpy(m.memory, code, sizeof(code));
    cpu.PC = 0;

    printf("  Instruction: FD C0 C4 %02X %02X %02X %02X (l=: $0x%08X)\n",
           code[3], code[4], code[5], code[6], virtual_addr);

    /* Decode the instruction */
    Nd500FetchedInstruction fi;
    nd500_decode_at(&m, 0, &fi);

    printf("  Decoded effective_address: 0x%08X\n", fi.operands[0].effective_address);

    /* Execute the instruction */
    InstrExecFunc func = g_instr_exec_table[fi.opcode];
    if (func) {
        printf("  Executing...\n");
        func(&cpu, &fi);
    } else {
        printf("  ERROR: No dispatch function!\n");
        nd500_machine_free(&m);
        return;
    }

    /* Check memory at PHYSICAL address (where MMU should have mapped the write) */
    uint32_t mem_at_physical = ((uint32_t)m.memory[physical_addr] << 24) |
                               ((uint32_t)m.memory[physical_addr+1] << 16) |
                               ((uint32_t)m.memory[physical_addr+2] << 8) |
                               (uint32_t)m.memory[physical_addr+3];

    /* Check memory at VIRTUAL address (should NOT be written if MMU worked) */
    uint32_t mem_at_virtual = 0;
    if (virtual_addr < m.memory_size - 4) {
        mem_at_virtual = ((uint32_t)m.memory[virtual_addr] << 24) |
                         ((uint32_t)m.memory[virtual_addr+1] << 16) |
                         ((uint32_t)m.memory[virtual_addr+2] << 8) |
                         (uint32_t)m.memory[virtual_addr+3];
    }

    printf("  Memory after @ physical 0x%08X: %02X %02X %02X %02X (0x%08X)\n",
           physical_addr,
           m.memory[physical_addr], m.memory[physical_addr+1],
           m.memory[physical_addr+2], m.memory[physical_addr+3],
           mem_at_physical);

    if (virtual_addr < m.memory_size - 4) {
        printf("  Memory after @ virtual  0x%08X: %02X %02X %02X %02X (0x%08X)\n",
               virtual_addr,
               m.memory[virtual_addr], m.memory[virtual_addr+1],
               m.memory[virtual_addr+2], m.memory[virtual_addr+3],
               mem_at_virtual);
    }

    printf("  Expected L value: 0x%08X\n", cpu.L);

    /* Test MMU translation directly */
    uint32_t translated = nd500_mmu_translate(&cpu, virtual_addr, 1, 0);  /* is_write=1, is_instruction=0 */
    printf("  MMU translate(0x%08X) = 0x%08X (expected: 0x%08X)\n",
           virtual_addr, translated, physical_addr);

    if (mem_at_physical == cpu.L) {
        printf("  PASS: L written to PHYSICAL address via MMU\n");
    } else if (mem_at_virtual == cpu.L) {
        printf("  PARTIAL: L written to VIRTUAL address (MMU bypass?)\n");
    } else {
        printf("  FAIL: L not written correctly anywhere!\n");
        printf("         - Physical address has: 0x%08X\n", mem_at_physical);
        printf("         - Virtual address has:  0x%08X\n", mem_at_virtual);
    }

    /* Cleanup */
    nd500_mmu_disable(&cpu);
    nd500_machine_free(&m);
    printf("\n");
}

void test_lget_user_scenario() {
    printf("=== Test 5: LGet User's Exact Scenario ===\n");

    /* Reproduce: 0802D448: FD C0 C4 08 01 D7 F0    l=:          $0x801D7F0 */
    Nd500Machine m;
    Nd500Cpu cpu;
    /* Need at least 0x0801D7F4 bytes (134.2 MB!) to test this address directly */
    nd500_machine_init(&m, 0x10000000);  /* 256MB memory to cover the address */
    nd500_cpu_init(&cpu, &m);
    nd500_cpu_reset(&cpu);

    /* Set L to the value from the user's scenario */
    cpu.L = 0x12345678;  /* Some test value */
    printf("  L = 0x%08X\n", cpu.L);

    /* The user's target address */
    uint32_t target_addr = 0x0801D7F0;
    printf("  Target address: 0x%08X\n", target_addr);
    printf("  Memory size: 0x%08X\n", (unsigned)m.memory_size);

    /* Check if address is within our memory */
    if (target_addr >= m.memory_size) {
        printf("  WARNING: Target address 0x%08X is beyond memory size 0x%X\n",
               target_addr, (unsigned)m.memory_size);
        printf("  This would cause a write to go nowhere!\n");
    } else {
        /* Clear target memory */
        m.memory[target_addr + 0] = 0x00;
        m.memory[target_addr + 1] = 0x00;
        m.memory[target_addr + 2] = 0x00;
        m.memory[target_addr + 3] = 0x00;
        printf("  Memory cleared at target address\n");
    }

    /* Construct the exact instruction */
    uint8_t code[] = {
        0xFD, 0xC0,           /* l=: opcode */
        0xC4,                 /* ABSOLUTE addressing */
        0x08, 0x01, 0xD7, 0xF0  /* address 0x0801D7F0 */
    };
    memcpy(m.memory, code, sizeof(code));
    cpu.PC = 0;

    printf("  Instruction bytes: FD C0 C4 08 01 D7 F0\n");

    /* Decode */
    Nd500FetchedInstruction fi;
    nd500_decode_at(&m, 0, &fi);

    printf("  Decoded:\n");
    printf("    effective_address: 0x%08X\n", fi.operands[0].effective_address);
    printf("    mode: %d (ND500_ADDR_ABSOLUTE=%d)\n", fi.operands[0].mode, ND500_ADDR_ABSOLUTE);
    printf("    data bytes: %02X %02X %02X %02X\n",
           fi.operands[0].data[0], fi.operands[0].data[1],
           fi.operands[0].data[2], fi.operands[0].data[3]);

    /* Verify address computation (big-endian check) */
    uint32_t computed = ((uint32_t)fi.operands[0].data[0] << 24) |
                        ((uint32_t)fi.operands[0].data[1] << 16) |
                        ((uint32_t)fi.operands[0].data[2] << 8) |
                        (uint32_t)fi.operands[0].data[3];
    printf("    computed from data (BE): 0x%08X\n", computed);

    uint32_t computed_le = ((uint32_t)fi.operands[0].data[3] << 24) |
                           ((uint32_t)fi.operands[0].data[2] << 16) |
                           ((uint32_t)fi.operands[0].data[1] << 8) |
                           (uint32_t)fi.operands[0].data[0];
    printf("    computed from data (LE): 0x%08X (wrong if used)\n", computed_le);

    /* Execute */
    InstrExecFunc func = g_instr_exec_table[fi.opcode];
    if (func) {
        printf("  Executing...\n");
        func(&cpu, &fi);
    }

    /* Check result */
    if (target_addr < m.memory_size - 4) {
        uint32_t mem_value = ((uint32_t)m.memory[target_addr] << 24) |
                             ((uint32_t)m.memory[target_addr+1] << 16) |
                             ((uint32_t)m.memory[target_addr+2] << 8) |
                             (uint32_t)m.memory[target_addr+3];

        printf("  Memory at 0x%08X (BE read): 0x%08X\n", target_addr, mem_value);
        printf("  Memory bytes: %02X %02X %02X %02X\n",
               m.memory[target_addr], m.memory[target_addr+1],
               m.memory[target_addr+2], m.memory[target_addr+3]);
        printf("  Expected: 0x%08X\n", cpu.L);

        if (mem_value == cpu.L) {
            printf("  PASS: Value correctly written\n");
        } else {
            printf("  FAIL: Value mismatch!\n");
        }
    } else {
        printf("  Cannot verify - address beyond memory\n");
    }

    nd500_machine_free(&m);
    printf("\n");
}

void test_lget_endian_verification() {
    printf("=== Test 6: Endianness Verification ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, 0x10000);
    nd500_cpu_init(&cpu, &m);

    /* Set L to a value with distinct bytes to verify endianness */
    cpu.L = 0x01020304;  /* Each byte is different */
    printf("  L = 0x%08X (bytes: 01 02 03 04)\n", cpu.L);

    uint32_t target_addr = 0x1000;

    /* Clear target */
    memset(&m.memory[target_addr], 0, 8);

    /* Instruction: l=: $0x1000 */
    uint8_t code[] = {
        0xFD, 0xC0,           /* l=: */
        0xC4,                 /* ABSOLUTE */
        0x00, 0x00, 0x10, 0x00  /* 0x00001000 big-endian */
    };
    memcpy(m.memory, code, sizeof(code));
    cpu.PC = 0;

    Nd500FetchedInstruction fi;
    nd500_decode_at(&m, 0, &fi);

    printf("  effective_address: 0x%08X\n", fi.operands[0].effective_address);

    InstrExecFunc func = g_instr_exec_table[fi.opcode];
    if (func) func(&cpu, &fi);

    printf("  Memory bytes at 0x%X: %02X %02X %02X %02X\n",
           target_addr,
           m.memory[target_addr], m.memory[target_addr+1],
           m.memory[target_addr+2], m.memory[target_addr+3]);

    /* ND-500 is big-endian, so 0x01020304 should be stored as:
     * addr+0: 0x01 (MSB)
     * addr+1: 0x02
     * addr+2: 0x03
     * addr+3: 0x04 (LSB)
     */
    if (m.memory[target_addr] == 0x01 &&
        m.memory[target_addr+1] == 0x02 &&
        m.memory[target_addr+2] == 0x03 &&
        m.memory[target_addr+3] == 0x04) {
        printf("  PASS: Big-endian storage correct\n");
    } else if (m.memory[target_addr] == 0x04 &&
               m.memory[target_addr+1] == 0x03 &&
               m.memory[target_addr+2] == 0x02 &&
               m.memory[target_addr+3] == 0x01) {
        printf("  FAIL: Little-endian storage detected! (ENDIAN BUG)\n");
    } else {
        printf("  FAIL: Unexpected byte order\n");
    }

    nd500_machine_free(&m);
    printf("\n");
}

int main() {
    printf("LGet (l=:) Instruction Tests\n");
    printf("============================\n\n");

    test_lget_debug();
    test_lget_basic();
    test_lget_local_short();
    test_lget_register();
    test_lget_with_mmu();
    test_lget_endian_verification();
    test_lget_user_scenario();

    printf("=== All LGet Tests Complete ===\n");
    return 0;
}
