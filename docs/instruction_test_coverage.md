# ND500X Instruction Test Coverage Report

Generated from nd500_tests.json and instructions_gen.c

## Summary

| Category | Count |
|----------|-------|
| Total unique mnemonics | 175 |
| Tested | 80 (46%) |
| Untested | 95 (54%) |
| Total test cases | 21,545 |

## Test Results (Latest Run)

| Metric | Value |
|--------|-------|
| Passed | 21,545 |
| Failed | 0 |
| Pass Rate | 100% |

## Untested Instructions by Class

### ARITHMETIC (20 instructions)

| Instruction | Opcode Variants |
|-------------|-----------------|
| addc | 4 |
| axi | 8 |
| div4 | 12 |
| ixi | 12 |
| mul4 | 12 |
| mulad | 20 |
| padd | 1 |
| paddr | 1 |
| pmpy | 1 |
| pmpyr | 1 |
| ppack | 1 |
| ppackr | 1 |
| psub | 1 |
| psubr | 1 |
| pupack | 1 |
| pupackr | 1 |
| rem | 8 |
| subc | 4 |
| udiv | 4 |
| umul | 4 |

### BITFIELD (5 instructions)

| Instruction | Opcode Variants |
|-------------|-----------------|
| clebi | 3 |
| getb | 4 |
| putbf | 12 |
| putbi | 12 |
| setbi | 3 |

### BRANCH (4 instructions)

| Instruction | Opcode Variants |
|-------------|-----------------|
| jumps | 1 |
| loop | 10 |
| loopd | 10 |
| loopi | 10 |

### CALL (12 instructions)

| Instruction | Opcode Variants |
|-------------|-----------------|
| call | 1 |
| callg | 1 |
| chain | 4 |
| entb | 1 |
| entd | 1 |
| entf | 1 |
| entfn | 1 |
| entm | 1 |
| ents | 1 |
| entsn | 1 |
| entt | 1 |
| rett | 1 |

### COMPARE (3 instructions)

| Instruction | Opcode Variants |
|-------------|-----------------|
| comp | 24 |
| pcomp | 1 |
| scomp | 1 |

### CONTROL (6 instructions)

| Instruction | Opcode Variants |
|-------------|-----------------|
| clte | 1 |
| init | 1 |
| set1 | 6 |
| sete | 1 |
| setk | 1 |
| tset | 1 |

### FLOAT_MATH (25 instructions)

| Instruction | Opcode Variants |
|-------------|-----------------|
| acos | 8 |
| alog | 8 |
| alog10 | 8 |
| alog2 | 8 |
| asin | 8 |
| atan | 8 |
| atan2 | 8 |
| biconv | 5 |
| byconr | 2 |
| byconv | 5 |
| cos | 8 |
| dconv | 5 |
| exp | 8 |
| fconr | 2 |
| fconv | 5 |
| hconr | 2 |
| hconv | 5 |
| poly | 8 |
| pwconv | 4 |
| sin | 8 |
| sqrt | 8 |
| tan | 8 |
| wconr | 2 |
| wconv | 5 |
| wpconv | 4 |

### IO (1 instructions)

| Instruction | Opcode Variants |
|-------------|-----------------|
| riom | 1 |

### LOGICAL (2 instructions)

| Instruction | Opcode Variants |
|-------------|-----------------|
| inv | 16 |
| invc | 4 |

### MOVE (3 instructions)

| Instruction | Opcode Variants |
|-------------|-----------------|
| bmove | 5 |
| clr | 24 |
| stz | 6 |

### SHIFT (2 instructions)

| Instruction | Opcode Variants |
|-------------|-----------------|
| pshift | 1 |
| pshiftr | 1 |

### STRING (18 instructions)

| Instruction | Opcode Variants |
|-------------|-----------------|
| schpar | 1 |
| scopa | 1 |
| scopt | 1 |
| scotr | 1 |
| scpuno | 1 |
| sfill | 24 |
| sfilln | 24 |
| smatch | 1 |
| smove | 6 |
| smovn | 6 |
| smvtr | 1 |
| smvtu | 1 |
| smvun | 1 |
| smvwh | 1 |
| sscan | 1 |
| sskip | 1 |
| sspan | 1 |
| sspar | 1 |

### SYSTEM (34 instructions)

| Instruction | Opcode Variants |
|-------------|-----------------|
| bladdr | 6 |
| cind | 12 |
| cpgu | 1 |
| cwip | 1 |
| dcc | 1 |
| dctsb | 1 |
| ddirt | 1 |
| dmof | 1 |
| dmon | 1 |
| freeb | 1 |
| int | 8 |
| intr | 8 |
| lcntxt | 1 |
| lind | 12 |
| lregbl | 1 |
| pcc | 1 |
| pctsb | 1 |
| phyladr | 4 |
| pmof | 1 |
| pmon | 1 |
| rdus | 16 |
| rladdr | 6 |
| rpgu | 8 |
| rphs | 1 |
| rwip | 8 |
| scntxt | 1 |
| sloca | 2 |
| sregbl | 1 |
| svers | 1 |
| tutti | 1 |
| wdus | 12 |
| wphs | 1 |
| zpgu | 1 |
| zwip | 1 |

## Tested Instructions

| Instruction | Test Cases | Opcode Variants |
|-------------|------------|-----------------|
| abs | 980 | 20 |
| add2 | 375 | 5 |
| add3 | 45 | 5 |
| and | 1392 | 16 |
| bp | 1 | 1 |
| clrk | 1 | 1 |
| comp2 | 450 | 6 |
| decr | 245 | 5 |
| div2 | 45 | 5 |
| div3 | 30 | 5 |
| getbf | 108 | 12 |
| getbi | 288 | 12 |
| go | 18 | 3 |
| ifkgo | 2 | 2 |
| ifkret | 2 | 1 |
| ifstgo | 16 | 2 |
| incr | 245 | 5 |
| jumpg | 4 | 1 |
| laddr | 216 | 24 |
| move | 90 | 6 |
| mul2 | 75 | 5 |
| mul3 | 45 | 5 |
| neg | 980 | 20 |
| noop | 1 | 1 |
| or | 1344 | 16 |
| ret | 1 | 1 |
| retb | 1 | 1 |
| retbk | 1 | 1 |
| retd | 1 | 1 |
| retk | 1 | 1 |
| sha | 234 | 3 |
| shl | 243 | 3 |
| shr | 237 | 3 |
| solo | 1 | 1 |
| sub2 | 375 | 5 |
| sub3 | 45 | 5 |
| swap | 72 | 6 |
| test | 312 | 6 |
| xor | 1392 | 16 |
| psum | 240 | 20 |