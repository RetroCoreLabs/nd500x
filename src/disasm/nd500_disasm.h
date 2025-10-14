#pragma once

#include <stdint.h>
#include <stddef.h>

/* Forward decl to avoid heavy deps */
struct Nd500Machine;

/*
 * Format disassembly for [addr, addr+len) into out buffer.
 * Uses the core CPU decoder (nd500_decode_at) and does NOT implement any
 * fetch/decode/EA logic here. Purely presentation/formatting.
 * Returns number of bytes written to out (not including terminating NUL).
 */
size_t nd500_disasm_format_range(struct Nd500Machine* m,
                                 uint32_t addr,
                                 uint32_t len,
                                 char* out,
                                 size_t out_cap);

size_t nd500_disasm_format_range_json(struct Nd500Machine* m,
                                      uint32_t addr,
                                      uint32_t len,
                                      char* out,
                                      size_t out_cap);


