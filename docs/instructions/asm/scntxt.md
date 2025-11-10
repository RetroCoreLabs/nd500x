# SCNTXT - Save Context Block

## Overview
**Mnemonic:** `scntxt` | **Function:** Save context to physical memory | **Class:** SYSTEM | **Privilege:** supervisor | **Format:** `SCNTXT <mask>, <address>, <process>`

## Description
Saves CPU registers to physical memory according to mask. Complement of LCNTXT. Used for process context switching and state preservation. **Operation:** `registers → memory[address + reg_num * 4]`

## Variants
| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0xFFF9 | SCNTXT |

## Examples
```assembly
SCNTXT 0xFFFF, CTX_SAVE, PROC_NUM  % Save all regs
SCNTXT REG_MASK, SAVE_AREA, -1     % Current process
SCNTXT 0x000F, PARTIAL_SAVE, PID   % Save I regs only
SCNTXT MASK, 0, PROC               % Use process save area
SCNTXT 0xFFFF, THREAD_CTX, T_ID    % Thread save
SCNTXT DOMAIN_MASK, DOM_SAVE, DOM  % Domain save
SCNTXT CTX_BITS, EXCEPTION_CTX, -1 % Exception save
```

## Trap Conditions
- **Addressing traps**, **Privilege violation**

## Data Status Bits
- **Z,S,C,V**: Unaffected

## Reference Manual
**Section:** §16.27.3

## See Also
- [LCNTXT](lcntxt.md), [SREGBL](sregbl.md), [LREGBL](lregbl.md)
