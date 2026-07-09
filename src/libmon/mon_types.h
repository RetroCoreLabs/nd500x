/*
 * SINTRAN III Monitor Call Types and Structures
 *
 * libmon - Reusable MON call emulation library for ND-500
 *
 * Reference: ND-860228.2 EN (SINTRAN III Monitor Calls)
 */

#ifndef MON_TYPES_H
#define MON_TYPES_H

#include <stdint.h>
#include <stddef.h>

/* =========================================================================
 * MON CALL RESULT CODES
 * ========================================================================= */

typedef enum {
    MON_SUCCESS = 0,        /* Operation successful, K flag = 0 */
    MON_ERROR = 1           /* Operation failed, K flag = 1, error in I1 */
} MonResult;

/* =========================================================================
 * IMPLEMENTATION STATUS
 *
 * Used to track which MON calls are implemented and working.
 * Allows breaking/halting on unimplemented calls during development.
 * ========================================================================= */

typedef enum {
    MON_STATUS_NOT_IMPLEMENTED = 0,  /* Stub only - will break if called */
    MON_STATUS_IN_PROGRESS = 1,      /* Partially implemented - may break */
    MON_STATUS_VALIDATED = 2,        /* Fully tested and working */
    MON_STATUS_DEPRECATED = 3        /* No longer supported by SINTRAN.
                                      * Handler IS called (returns error 52
                                      * with K flag), but MOINF (312B)
                                      * reports the call as non-existent. */
} MonImplStatus;

/* =========================================================================
 * UNIMPLEMENTED MON BEHAVIOR
 *
 * Controls what happens when a MON call that isn't implemented is invoked.
 * ========================================================================= */

typedef enum {
    MON_UNIMPL_CONTINUE = 0,   /* Log warning and continue (return error) */
    MON_UNIMPL_BREAK = 1,      /* Break into debugger */
    MON_UNIMPL_HALT = 2        /* Halt CPU execution */
} MonUnimplBehavior;

/* =========================================================================
 * MON CALL CONTEXT
 *
 * Contains all information needed by a MON handler to execute.
 * Passed to every handler function.
 * ========================================================================= */

#define MON_MAX_ARGS 256

typedef struct MonContext {
    /* CPU and machine pointers (opaque to libmon) */
    void* cpu;
    void* machine;

    /* MON call identification */
    uint32_t mon_number;        /* MON number (decimal form of octal) */
    uint32_t instruction_address; /* PC of the CALL instruction (for logging) */
    uint32_t return_address;    /* PC to return to after MON call */

    /* Arguments from CALL/CALLG instruction */
    uint32_t arg_count;                     /* Number of arguments */
    uint32_t arg_addresses[MON_MAX_ARGS];   /* Effective addresses of args */

    /* Memory access callbacks (set by host emulator) */
    uint32_t (*read_word)(void* cpu, uint32_t addr);
    void (*write_word)(void* cpu, uint32_t addr, uint32_t val);
    uint16_t (*read_halfword)(void* cpu, uint32_t addr);
    void (*write_halfword)(void* cpu, uint32_t addr, uint16_t val);
    uint8_t (*read_byte)(void* cpu, uint32_t addr);
    void (*write_byte)(void* cpu, uint32_t addr, uint8_t val);

    /* Flag manipulation callbacks */
    void (*set_k_flag)(void* cpu, int value);
    void (*set_error_code)(void* cpu, int32_t code);

    /* Register access callbacks for return values */
    void (*set_i1)(void* cpu, uint32_t value);  /* I1/W1 register */
    uint32_t (*get_i1)(void* cpu);

    /* Segment allocation callback (set by host emulator) */
    int (*allocate_segment)(void* cpu, void* machine, uint8_t domain,
        uint32_t requested_segment, uint32_t segment_size_bytes,
        uint32_t* out_assigned_segment);

    /* Control flow signals (set by handler or dispatcher) */
    int halt_requested;         /* Request CPU halt (MON 0B LEAVE) */
    int break_requested;        /* Request debugger break */
    const char* halt_reason;    /* Human-readable halt reason */

    /* Error reporting (set by handlers via mon_set_error) */
    int32_t error_code;         /* Error code from last MON call (CPU-agnostic) */
    int error_flag;             /* 1 if error occurred (maps to K flag) */

} MonContext;

/* =========================================================================
 * MON HANDLER FUNCTION TYPE
 *
 * Signature for all MON call handler functions.
 * ========================================================================= */

typedef MonResult (*MonHandler)(MonContext* ctx);

/* =========================================================================
 * MON REGISTRY ENTRY
 *
 * Describes a registered MON call handler.
 * ========================================================================= */

typedef struct {
    uint32_t mon_number;        /* MON number (decimal) */
    const char* octal_str;      /* Octal string (e.g., "11B") */
    const char* name;           /* Short name (e.g., "TIME") */
    const char* long_name;      /* Long name (e.g., "GetBasicTime") */
    const char* description;    /* Description from YAML */
    const char* params_desc;    /* Parameter description (e.g., "[O] BasicTime (LONGINT)") */
    MonHandler handler;         /* Handler function pointer */
    MonImplStatus status;       /* Implementation status */
    uint8_t param_count;        /* Expected parameter count */
    uint8_t nd100_compat;       /* 1 if compatible with ND-100 */
    uint8_t nd500_compat;       /* 1 if compatible with ND-500 */
} MonRegistryEntry;

/* =========================================================================
 * PARAMETER TYPES (from YAML)
 * ========================================================================= */

typedef enum {
    MON_PARAM_INTEGER = 0,      /* INTEGER (16-bit on ND-100, 32-bit on ND-500) */
    MON_PARAM_LONGINT = 1,      /* LONGINT (32-bit) */
    MON_PARAM_REAL = 2,         /* REAL (single precision float) */
    MON_PARAM_DOUBLE = 3,       /* DOUBLE (double precision float) */
    MON_PARAM_STRING = 4,       /* String descriptor */
    MON_PARAM_BYTE = 5,         /* BYTE (8-bit) */
    MON_PARAM_POINTER = 6       /* Pointer/Address */
} MonParamType;

/* Parameter I/O direction */
typedef enum {
    MON_PARAM_IN = 0,           /* Input parameter (I) */
    MON_PARAM_OUT = 1,          /* Output parameter (O) */
    MON_PARAM_INOUT = 2         /* Input/Output parameter (IO) */
} MonParamIO;

/* =========================================================================
 * MON HALT REASONS
 *
 * Why the CPU halted due to a MON call.
 * ========================================================================= */

typedef enum {
    MON_HALT_NONE = 0,
    MON_HALT_LEAVE = 1,             /* MON 0B LEAVE - normal program exit */
    MON_HALT_UNIMPLEMENTED = 2,     /* Unimplemented MON call */
    MON_HALT_IN_PROGRESS = 3,       /* In-progress MON call (optional halt) */
    MON_HALT_ERROR = 4              /* MON call error */
} MonHaltReason;

#endif /* MON_TYPES_H */
