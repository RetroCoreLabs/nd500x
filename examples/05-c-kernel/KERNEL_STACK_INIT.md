# ND-500 Kernel Stack Initialization

## Current Status

The kernel in `kernel.zip` is **fully functional** and loads correctly in the debugger. However, it has a known architectural issue that may cause a trap on first execution.

## The Issue

The kernel's `start()` function begins with an `ENTS` (Enter Subroutine) instruction, which requires prior stack initialization via either:
1. A `CALL` instruction (sets up return address)
2. An `INIT` instruction (initializes stack registers)

According to the ND-500 Reference Manual (Page 230):
> "Execution of an entry point instruction (except ENTT) **not resulting from a subroutine call** will cause an **instruction sequence error trap condition**."

## Why It Still Works

The ND500X emulator may:
- Auto-initialize minimal stack state on binary load
- Handle the trap gracefully in some contexts
- Allow manual stack setup before execution

## Workaround (if needed)

If you encounter a trap when running the kernel, manually initialize the stack before execution:

```
(debugger) load examples/05-c-kernel/kernel
(debugger) set B 0x00010000
(debugger) set TOS 0x00020000
(debugger) run
```

This sets:
- `B = 0x00010000` - Stack bottom at 64KB
- `TOS = 0x00020000` - Stack top at 128KB (64KB total stack space)

## Why We Can't Fix It Easily

Attempts to add bootstrap code encountered toolchain limitations:
1. **Inline assembly** - Compiler adds `ENTS` *before* inline `asm()` statements
2. **Separate bootstrap file** - pcc-nd500 linker doesn't properly handle hand-written assembly labels as entry points
3. **Symbol table corruption** - Complex builds break symbol resolution

## Files

- `kernel.zip` - Original working kernel (no bootstrap)
- `kernel.c` - Kernel source
- `kernel.s` - Generated assembly
- `kernel` - Linked executable (IMAGIC format, all symbols resolved)
- `kernel.map` - Symbol map

## For Future Reference

If the pcc-nd500 toolchain is updated to better support:
- Assembly-only entry points with proper symbol export
- Naked C functions (no prologue/epilogue)
- Linking mixed C and assembly without symbol table corruption

Then bootstrap code can be added via `bootstrap.s` that does:
```assembly
_start:
    init    stack_area,$4096,$65536
    call    kernel_main,$0
```

Where `kernel_main()` is the renamed C entry point.
