# MOVE - Move Data

**Mnemonic:** `move`  
**Function:** Copy data from source to destination  
**Class:** MOVE  
**Format:** `t MOVE <source>,<dest>`

**Description:** Copies data from source to destination. Source unaffected. Constant destinations illegal.

**Operands:** 2  
**Variants:** 6

**Examples:**

```assembly
% Copy variable to another
W MOVE B.SRC, B.DEST

% Load constant to memory
W MOVE 100, B.COUNT

% Copy between record fields
W MOVE R.FIELD1, R.FIELD2
```

**Reference:** §10.7 Move  
**See Also:** [SWAP](swap.md), [:=](assignto.md)
