# SVERS - Store Microprogram Version

## Overview

**Mnemonic:** `svers`
**Function:** Store microprogram version
**Class:** SYSTEM
**Privilege:** supervisor
**Extension:** '87 architecture extension

**Format:** `SVERS <dest>`

---

## Description

Stores the microprogram version number to the specified destination address. This privileged instruction allows system software to query the CPU microcode version for compatibility checking and feature detection.

**Operation:**
```
microprogram_version → <dest>
Status bits set according to version value
```

**Key Characteristics:**
- Privileged instruction (supervisor mode only)
- '87 architecture extension (not on earlier models)
- Returns CPU microcode version identifier
- Used for system diagnostics and compatibility checks
- Version-specific feature detection

**Common Use Cases:**
- Operating system boot-time CPU detection
- Microcode compatibility verification
- System diagnostics and hardware inventory
- Feature availability checking
- Debug information collection

**Operands:** 1 (destination for version word)
**Variants:** 1 opcode

---

## Variants

Total variants: 1

| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0xFFFB | SVERS |

---

## Operands

### Operand 1 (Destination)

**Type:** Word
**Access:** Write

Receives the microprogram version number.

**Supported modes:**
- **LOCAL** - Local variable
- **RECORD** - Record field
- **PRE_INDEXED** - Indexed array
- **ABSOLUTE** - Absolute address

---

## Trap Conditions

- **Privilege violation:** If executed in user mode
- **Illegal instruction code (IIC):** Not supported on pre-'87 systems
- **Addressing traps:** Invalid destination address

---

## Data Status Bits

Status bits set according to version value (implementation-specific).

---

## Examples

### Example 1: Query microprogram version at boot

```assembly
        % Store microcode version
        SVERS B.MICROCODE_VER
```

**Explanation:** Operating system boot code querying CPU microcode version.

### Example 2: Feature detection

```assembly
        % Check if advanced features available
        SVERS TEMP_VERSION
        W1 := TEMP_VERSION
        W1 COMP MIN_REQUIRED_VERSION
        IF<GO UNSUPPORTED_CPU
        % CPU has required features
```

**Explanation:** Checking if CPU microcode version supports required features.

### Example 3: System diagnostics

```assembly
        % Collect system information
        SVERS DIAG_INFO.CPU_VERSION
```

**Explanation:** Storing version for diagnostic report.

### Example 4: Multi-CPU system inventory

```assembly
        % Query each CPU's microcode version
        W1 := 0                     % CPU index
QUERY_LOOP:
        % Switch to CPU
        CALL SELECT_CPU
        SVERS CPU_VERSIONS(W1)
        W1 INC
        W1 COMP NUM_CPUS
        IF<GO QUERY_LOOP
```

**Explanation:** Querying microcode versions across multiple CPUs.

### Example 5: Conditional code path selection

```assembly
        % Use optimized path if new microcode
        SVERS CURRENT_VER
        W1 := CURRENT_VER
        W1 COMP NEW_VERSION_THRESHOLD
        IF>=GO USE_OPTIMIZED_PATH
        % Use legacy code path
        CALL LEGACY_ROUTINE
        GO DONE
USE_OPTIMIZED_PATH:
        CALL OPTIMIZED_ROUTINE
DONE:
```

**Explanation:** Selecting algorithm based on microcode capabilities.

### Example 6: Debug information logging

```assembly
        % Log CPU version for bug reports
        SVERS LOG_BUFFER.CPU_VER
        CALL WRITE_LOG
```

**Explanation:** Including microcode version in debug logs.

### Example 7: Compatibility warning

```assembly
        % Warn if old microcode
        SVERS DETECTED_VER
        W1 := DETECTED_VER
        W1 COMP RECOMMENDED_VER
        IF>=GO VERSION_OK
        % Display warning
        CALL SHOW_UPGRADE_WARNING
VERSION_OK:
```

**Explanation:** Warning users about outdated microcode.

---

## Performance Notes

- **Execution:** 2-3 cycles
- **Implementation:** Reads hardware version register
- **Overhead:** Minimal

**Usage recommendations:**
- Query once at system boot, cache result
- Use for feature detection not performance-critical code
- Essential for multi-version software compatibility
- Log version in crash dumps for debugging

**Version numbering:**
- Format is implementation-specific
- Higher numbers typically indicate newer microcode
- Consult hardware documentation for version meanings
- May include major/minor version encoding

---

## Reference Manual

**Section:** §16.35
**Title:** SVERS - Store microprogram version ('87 extension)

---

## See Also

- [SREGBL](sregbl.md) - Save register block
- [SLOCA](sloca.md) - System location access
- [SYSTEM instruction reference](../../instruction-reference/SYSTEM.md)
