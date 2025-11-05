# ND500X Disassembler ANSI Color Output Specification

## 1. Overview

This document defines the implementation of ANSI colorized disassembly output for the ND500X emulator, based on the requirements from the nd500-dis color output specification.

The implementation maintains strict separation of concerns:
- **Color logic** lives in `src/ndlib/ndlib_color.c` (new file)
- **Disassembly logic** stays in `src/machine/debug_api.c` (minimal changes)
- **Command-line handling** in `src/frontend/nd500x/nd500x.c` (minimal changes)

---

## 2. Architecture

### 2.1 Module Structure

```
┌────────────────────────────────────────┐
│   Frontend (nd500x.c)                  │
│   - Parses -ansi/-noansi flags         │
│   - Calls ndlib_color_init()           │
└──────────────┬─────────────────────────┘
               │
               ▼
┌────────────────────────────────────────┐
│   Color Module (ndlib_color.c)         │
│   - Terminal detection (isatty)        │
│   - TERM environment check             │
│   - Color function API                 │
│   - Global use_color state             │
└──────────────┬─────────────────────────┘
               │
               ▼
┌────────────────────────────────────────┐
│   Debug API (debug_api.c)              │
│   - Calls color_*() functions          │
│   - No color logic here                │
│   - Pure disassembly formatting        │
└────────────────────────────────────────┘
```

### 2.2 Dependencies

- **ndlib_color.c** depends on: `<stdbool.h>`, `<unistd.h>`, `<string.h>`, `<stdlib.h>`
- **debug_api.c** depends on: `ndlib_color.h` (new include)
- **nd500x.c** depends on: `ndlib_color.h` (new include)

---

## 3. API Definition

### 3.1 Color Functions (ndlib_color.h)

```c
/* Initialize color system with command-line override
 * ansi_flag: 0=auto-detect, 1=force-enable, -1=force-disable
 */
void ndlib_color_init(int ansi_flag);

/* Color accessor functions - return ANSI codes or empty strings */
const char* color_address(void);   /* Gray    - \033[90m */
const char* color_bytes(void);     /* Yellow  - \033[33m */
const char* color_instr(void);     /* Green   - \033[92m */
const char* color_oper(void);      /* White   - \033[97m */
const char* color_branch(void);    /* Red     - \033[91m */
const char* color_label(void);     /* Cyan    - \033[96m */
const char* color_comment(void);   /* Blue    - \033[94m */
const char* color_meta(void);      /* Yellow  - \033[93m */
const char* color_reset(void);     /* Reset   - \033[0m  */

/* Query current color state (for testing/debugging) */
int ndlib_color_enabled(void);
```

### 3.2 Command-Line Behavior

| Flag       | Description                                    |
|------------|------------------------------------------------|
| `-ansi`    | Force-enable ANSI color even if detection fails |
| `-noansi`  | Disable all color output regardless of terminal |
| *(default)*| Enable color if stdout is TTY + ANSI terminal   |

---

## 4. Implementation Details

### 4.1 Terminal Detection (ndlib_color.c)

```c
static bool use_color = false;

void ndlib_color_init(int ansi_flag) {
    if (ansi_flag == 1) {
        /* Force enable via -ansi flag */
        use_color = true;
        return;
    }
    if (ansi_flag == -1) {
        /* Force disable via -noansi flag */
        use_color = false;
        return;
    }
    
    /* Auto-detect: check TTY + TERM environment */
    const char *term = getenv("TERM");
    bool term_supports_ansi = false;
    
    if (term && (strstr(term, "xterm") || 
                 strstr(term, "ansi") || 
                 strstr(term, "vt100") || 
                 strstr(term, "rxvt") ||
                 strstr(term, "screen") ||
                 strstr(term, "tmux"))) {
        term_supports_ansi = true;
    }
    
    use_color = isatty(STDOUT_FILENO) && term_supports_ansi;
}
```

### 4.2 Color Function Implementation

Each function returns either an ANSI code or empty string:

```c
const char* color_address(void) { 
    return use_color ? "\033[90m" : ""; 
}

const char* color_bytes(void) { 
    return use_color ? "\033[33m" : ""; 
}

/* ... etc for all color functions ... */
```

### 4.3 Integration into debug_api.c

**Current code** (line 133-156):
```c
printf("%08X: ", fi.address);
for (uint32_t b = 0; b < fi.total_len && b < 16; b++) {
    printf("%02X ", fi.bytes[b]);
}
printf("%-*s", (int)(24 - fi.total_len * 3), "");

if (sy && *sy) {
    printf("<%s> ", sy);
}
if (fi.operand_count > 0) {
    printf("%-12s ", full_mn);
} else {
    printf("%s", full_mn);
}
```

**New colorized code**:
```c
/* Determine instruction color based on opcode type */
const char* instr_color = color_instr();
if (nd500_instr_is_branch(fi.opcode)) {
    instr_color = color_branch();
}

/* Address in gray */
printf("%s%08X:%s ", color_address(), fi.address, color_reset());

/* Bytes in yellow */
printf("%s", color_bytes());
for (uint32_t b = 0; b < fi.total_len && b < 16; b++) {
    printf("%02X ", fi.bytes[b]);
}
printf("%s%-*s", color_reset(), (int)(24 - fi.total_len * 3), "");

/* Label/symbol in cyan */
if (sy && *sy) {
    printf("%s<%s>%s ", color_label(), sy, color_reset());
}

/* Mnemonic with appropriate color */
if (fi.operand_count > 0) {
    printf("%s%-12s%s ", instr_color, full_mn, color_reset());
} else {
    printf("%s%s%s", instr_color, full_mn, color_reset());
}

/* Operands in white */
/* (existing operand printing code gets wrapped with color_oper()) */
```

### 4.4 Opcode Classification

The nd500x already has opcode classification via:
- `nd500_instr_is_branch(opcode)` - identifies branch/jump instructions
- `map_mnemonic_symbol()` - maps mnemonics to symbols
- Instruction metadata from generated tables

**Branch instructions** will use `color_branch()`:
- Detected via `nd500_instr_is_branch()` function
- Includes: `go`, `if<=go`, `if>=go`, `if><go`, `if=go`, `if<>go`, etc.

**Regular instructions** will use `color_instr()`:
- Data movement: `move`, `stz`, etc.
- Arithmetic/logic: `add`, `sub`, `+`, `-`, etc.
- All other opcodes

---

## 5. File Changes Summary

### 5.1 New Files

**src/ndlib/ndlib_color.c** (NEW)
- All ANSI color code logic
- Terminal detection
- Color state management
- ~150 lines

**src/ndlib/ndlib_color.h** (NEW)
- Public API declarations
- Function prototypes
- ~30 lines

### 5.2 Modified Files

**src/machine/debug_api.c** (MODIFIED)
- Add `#include "../ndlib/ndlib_color.h"`
- Update `nd500_dbg_disasm_print()` to use color functions
- ~20 lines changed in printf statements

**src/frontend/nd500x/nd500x.c** (MODIFIED)
- Add command-line flag parsing for `-ansi`/`-noansi`
- Call `ndlib_color_init()` before debugger starts
- ~10 lines added

**CMakeLists.txt** (MODIFIED)
- Add `src/ndlib/ndlib_color.c` to ndlib sources
- No other changes needed

---

## 6. Example Output

### 6.1 Plain Text (default when redirected or -noansi)
```
00000000: B8 CF 1C 00 00 00       ents         $28
00000006: 1A 08 44                w move       $8,b.16
00000009: 0C 45                   w1 :=        b.20
0000000B: 54 46                   w1 +         b.24
0000004A: C7 14 00                if><go       $20
```

### 6.2 Colorized Output (TTY with ANSI support)
```
[gray]00000000:[reset] [yellow]B8 CF 1C 00 00 00[reset]       [green]ents[reset]         [white]$28[reset]
[gray]00000006:[reset] [yellow]1A 08 44[reset]                [green]w move[reset]       [white]$8,b.16[reset]
[gray]00000009:[reset] [yellow]0C 45[reset]                   [green]w1 :=[reset]        [white]b.20[reset]
[gray]0000000B:[reset] [yellow]54 46[reset]                   [green]w1 +[reset]         [white]b.24[reset]
[gray]0000004A:[reset] [yellow]C7 14 00[reset]                [red]if><go[reset]       [white]$20[reset]
```

**With symbols**:
```
[gray]00000000:[reset] [yellow]B8 CF 1C 00[reset] [cyan]<_add>[reset] [green]ents[reset] [white]$28[reset]
```

---

## 7. Testing

### 7.1 Manual Testing

| Command | Expected Result |
|---------|----------------|
| `./nd500x --debug` | Auto-detect: colored if TTY+ANSI |
| `./nd500x --debug -ansi` | Force colors ON |
| `./nd500x --debug -noansi` | Force colors OFF |
| `./nd500x --debug \| cat` | No colors (pipe detected) |
| `./nd500x --debug > out.txt` | No colors (redirect detected) |

### 7.2 Terminal Compatibility

| Terminal | Detection Method | Expected |
|----------|------------------|----------|
| xterm | `TERM=xterm*` | ✓ Colors |
| gnome-terminal | `TERM=xterm*` | ✓ Colors |
| konsole | `TERM=xterm*` | ✓ Colors |
| tmux | `TERM=tmux*` or `screen*` | ✓ Colors |
| screen | `TERM=screen*` | ✓ Colors |
| Linux console | `TERM=linux` | ✗ No colors (safe) |
| Windows CMD | No TTY | ✗ No colors |
| VSCode terminal | `TERM=xterm*` | ✓ Colors |

---

## 8. Future Enhancements

1. **Configuration file**: Load colors from `~/.nd500x/colors.conf`
2. **Theme support**: Multiple color schemes (dark/light/solarized)
3. **Bold/underline**: Enhanced styling for headers/metadata
4. **256-color mode**: Richer palette for modern terminals
5. **True color (24-bit)**: Full RGB support

---

## 9. Rationale for Design Choices

### Why separate ndlib_color.c?
- **Modularity**: Color logic is isolated and testable
- **Reusability**: Can be used by other output functions (symbol tables, register dumps)
- **Maintainability**: Changes to color codes don't touch disassembly logic
- **Testability**: Can test color detection without running full disassembler

### Why global use_color flag?
- **Performance**: No need to pass state through function calls
- **Simplicity**: Color functions are simple accessors
- **Thread-safety**: Not needed - single-threaded debugger

### Why not use a struct for color codes?
- **Simplicity**: Functions are cleaner than struct member access
- **Future-proofing**: Can add logic inside functions (e.g., theme switching)
- **API stability**: Function signatures won't change if internal structure changes

---

## 10. Implementation Checklist

- [ ] Create `src/ndlib/ndlib_color.h` with API declarations
- [ ] Create `src/ndlib/ndlib_color.c` with implementation
- [ ] Modify `src/machine/debug_api.c` to use color functions
- [ ] Modify `src/frontend/nd500x/nd500x.c` for flag parsing
- [ ] Update `CMakeLists.txt` to compile new source file
- [ ] Test on Linux with xterm/gnome-terminal
- [ ] Test with `-ansi` and `-noansi` flags
- [ ] Test output redirection (should disable colors)
- [ ] Test in WSL environment
- [ ] Update README.md with new command-line flags

---

## 11. Color Palette Reference

| Element | Color | ANSI Code | Purpose |
|---------|-------|-----------|---------|
| Address | Gray (bright black) | `\033[90m` | Less prominent, structural |
| Bytes | Yellow | `\033[33m` | Raw data, hexadecimal |
| Instruction | Green (bright) | `\033[92m` | Standard operations |
| Branch | Red (bright) | `\033[91m` | Control flow (attention) |
| Operand | White (bright) | `\033[97m` | Data operands, registers |
| Label | Cyan (bright) | `\033[96m` | Symbols, entry points |
| Comment | Blue (bright) | `\033[94m` | Metadata, annotations |
| Meta | Yellow (bright) | `\033[93m` | File headers, statistics |
| Reset | - | `\033[0m` | Return to default |

---

## 12. Compatibility Notes

### Windows Support
- **WSL**: Full support (Linux terminal)
- **Native Windows**: Use Windows Terminal or ConEmu
- **CMD.exe**: Will auto-disable colors (no ANSI support)
- **PowerShell 7+**: Should work (ANSI support added)

### macOS Support
- **Terminal.app**: Full support
- **iTerm2**: Full support
- **tmux on macOS**: Full support

---

*This specification follows the requirements from the nd500-dis color output specification while adapting to the nd500x architecture and maintaining clean separation of concerns.*

