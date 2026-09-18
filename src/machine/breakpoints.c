/*
 * breakpoints.c - breakpoint and watchpoint tables
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include "breakpoints.h"
#include "../cpu/cpu_protos.h"

void bp_mgr_init(BreakpointManager* mgr) {
    if (!mgr) return;
    memset(mgr, 0, sizeof(*mgr));
}

/* Breakpoint management */
int bp_add(BreakpointManager* mgr, uint32_t address, bool one_shot) {
    if (!mgr || mgr->bp_count >= MAX_BREAKPOINTS) return -1;

    int id = mgr->bp_count;
    Breakpoint* bp = &mgr->breakpoints[id];
    bp->address = address;
    bp->type = BP_TYPE_EXECUTE;
    bp->enabled = true;
    bp->one_shot = one_shot;
    bp->hit_count = 0;
    bp->condition[0] = '\0';

    mgr->bp_count++;
    printf("Breakpoint %d set at 0x%08X%s\n", id, address, one_shot ? " (one-shot)" : "");
    return id;
}

int bp_add_conditional(BreakpointManager* mgr, uint32_t address, const char* condition, bool one_shot) {
    if (!mgr || mgr->bp_count >= MAX_BREAKPOINTS || !condition) return -1;

    int id = mgr->bp_count;
    Breakpoint* bp = &mgr->breakpoints[id];
    bp->address = address;
    bp->type = BP_TYPE_CONDITIONAL;
    bp->enabled = true;
    bp->one_shot = one_shot;
    bp->hit_count = 0;
    strncpy(bp->condition, condition, sizeof(bp->condition) - 1);
    bp->condition[sizeof(bp->condition) - 1] = '\0';

    mgr->bp_count++;
    printf("Conditional breakpoint %d set at 0x%08X: %s%s\n", id, address, condition, one_shot ? " (one-shot)" : "");
    return id;
}

int bp_delete(BreakpointManager* mgr, int id) {
    if (!mgr || id < 0 || id >= mgr->bp_count) return -1;

    /* Shift remaining breakpoints down */
    for (int i = id; i < mgr->bp_count - 1; i++) {
        mgr->breakpoints[i] = mgr->breakpoints[i + 1];
    }
    mgr->bp_count--;
    printf("Breakpoint %d deleted\n", id);
    return 0;
}

int bp_enable(BreakpointManager* mgr, int id) {
    if (!mgr || id < 0 || id >= mgr->bp_count) return -1;
    mgr->breakpoints[id].enabled = true;
    printf("Breakpoint %d enabled\n", id);
    return 0;
}

int bp_disable(BreakpointManager* mgr, int id) {
    if (!mgr || id < 0 || id >= mgr->bp_count) return -1;
    mgr->breakpoints[id].enabled = false;
    printf("Breakpoint %d disabled\n", id);
    return 0;
}

void bp_list(BreakpointManager* mgr) {
    if (!mgr || mgr->bp_count == 0) {
        printf("No breakpoints set\n");
        return;
    }

    printf("=== BREAKPOINTS ===\n");
    printf("ID  Address    Enabled  Hits   Type        Condition\n");
    printf("--- ---------- -------- ------ ----------- ---------\n");
    for (int i = 0; i < mgr->bp_count; i++) {
        Breakpoint* bp = &mgr->breakpoints[i];
        const char* type_str = (bp->type == BP_TYPE_CONDITIONAL) ? "conditional" : "normal";
        printf("%3d 0x%08X %-8s %6u %-11s %s\n",
               i, bp->address,
               bp->enabled ? "yes" : "no",
               bp->hit_count,
               type_str,
               bp->type == BP_TYPE_CONDITIONAL ? bp->condition : "");
    }
}

bool bp_should_break_at(BreakpointManager* mgr, uint32_t pc) {
    if (!mgr) return false;

    /* Check if we should break on next instruction (step command) */
    if (mgr->break_on_next_instruction) {
        mgr->break_on_next_instruction = false;
        return true;
    }

    /* Check all breakpoints */
    for (int i = 0; i < mgr->bp_count; i++) {
        Breakpoint* bp = &mgr->breakpoints[i];
        if (bp->enabled && bp->address == pc) {
            /* For conditional breakpoints, evaluate the condition */
            if (bp->type == BP_TYPE_CONDITIONAL) {
                /* TODO: Get current CPU state for condition evaluation */
                /* For now, just break on conditional breakpoints */
                /* In a full implementation, we'd need access to CPU registers */
            }

            bp->hit_count++;
            printf("\nBreakpoint %d hit at 0x%08X (hit count: %u)\n", i, pc, bp->hit_count);

            /* Handle one-shot breakpoints */
            if (bp->one_shot) {
                printf("One-shot breakpoint %d deleted\n", i);
                bp_delete(mgr, i);
            }
            return true;
        }
    }
    return false;
}

/* Watchpoint management */
int wp_add(BreakpointManager* mgr, uint32_t address, uint32_t length, WatchpointType type) {
    if (!mgr || mgr->wp_count >= MAX_WATCHPOINTS) return -1;

    int id = mgr->wp_count;
    Watchpoint* wp = &mgr->watchpoints[id];
    wp->address = address;
    wp->length = length;
    wp->type = type;
    wp->enabled = true;
    wp->last_value = 0;
    wp->hit_count = 0;
    wp->register_name[0] = '\0';

    mgr->wp_count++;
    const char* type_str = (type == WP_TYPE_READ) ? "read" :
                           (type == WP_TYPE_WRITE) ? "write" :
                           (type == WP_TYPE_CHANGE) ? "change" : "register";
    printf("Watchpoint %d set at 0x%08X (length=%u, type=%s)\n", id, address, length, type_str);
    return id;
}

int wp_add_register(BreakpointManager* mgr, const char* reg_name, uint32_t reg_index) {
    if (!mgr || mgr->wp_count >= MAX_WATCHPOINTS || !reg_name) return -1;

    int id = mgr->wp_count;
    Watchpoint* wp = &mgr->watchpoints[id];
    wp->address = reg_index;  /* Store register index in address field */
    wp->length = 4;           /* Registers are 32-bit */
    wp->type = WP_TYPE_REGISTER;
    wp->enabled = true;
    wp->last_value = 0;
    wp->hit_count = 0;
    strncpy(wp->register_name, reg_name, sizeof(wp->register_name) - 1);
    wp->register_name[sizeof(wp->register_name) - 1] = '\0';

    mgr->wp_count++;
    printf("Register watchpoint %d set on %s (index=%u)\n", id, reg_name, reg_index);
    return id;
}

int wp_register_index_for_name(const char* name) {
    static const char* names[WP_REG_INDEX_COUNT] = {
        "PC", "I1", "I2", "I3", "I4", "L", "B", "R"
    };
    if (!name) return -1;
    for (int i = 0; i < WP_REG_INDEX_COUNT; i++) {
        if (strcasecmp(names[i], name) == 0) return i;
    }
    return -1;
}

int wp_check_registers(BreakpointManager* mgr, const uint32_t regs[WP_REG_INDEX_COUNT]) {
    if (!mgr) return -1;
    for (int i = 0; i < mgr->wp_count; i++) {
        Watchpoint* wp = &mgr->watchpoints[i];
        if (!wp->enabled || wp->type != WP_TYPE_REGISTER) continue;
        if (wp->address >= WP_REG_INDEX_COUNT) continue;
        uint32_t value = regs[wp->address];
        if (value != wp->last_value) {
            wp->hit_count++;
            printf("\nRegister watchpoint %d hit on %s (old=0x%08X new=0x%08X)\n",
                   i, wp->register_name, wp->last_value, value);
            wp->last_value = value;
            return i;
        }
    }
    return -1;
}

int wp_delete(BreakpointManager* mgr, int id) {
    if (!mgr || id < 0 || id >= mgr->wp_count) return -1;

    /* Shift remaining watchpoints down */
    for (int i = id; i < mgr->wp_count - 1; i++) {
        mgr->watchpoints[i] = mgr->watchpoints[i + 1];
    }
    mgr->wp_count--;
    printf("Watchpoint %d deleted\n", id);
    return 0;
}

int wp_enable(BreakpointManager* mgr, int id) {
    if (!mgr || id < 0 || id >= mgr->wp_count) return -1;
    mgr->watchpoints[id].enabled = true;
    printf("Watchpoint %d enabled\n", id);
    return 0;
}

int wp_disable(BreakpointManager* mgr, int id) {
    if (!mgr || id < 0 || id >= mgr->wp_count) return -1;
    mgr->watchpoints[id].enabled = false;
    printf("Watchpoint %d disabled\n", id);
    return 0;
}

void wp_list(BreakpointManager* mgr) {
    if (!mgr || mgr->wp_count == 0) {
        printf("No watchpoints set\n");
        return;
    }

    printf("=== WATCHPOINTS ===\n");
    printf("ID  Address    Length Enabled  Hits   Type\n");
    printf("--- ---------- ------ -------- ------ ------\n");
    for (int i = 0; i < mgr->wp_count; i++) {
        Watchpoint* wp = &mgr->watchpoints[i];
        const char* type_str = (wp->type == WP_TYPE_READ) ? "read" :
                               (wp->type == WP_TYPE_WRITE) ? "write" : "change";
        printf("%3d 0x%08X %6u %-8s %6u %s\n",
               i, wp->address, wp->length,
               wp->enabled ? "yes" : "no",
               wp->hit_count, type_str);
    }
}

bool wp_should_break_on_read(BreakpointManager* mgr, uint32_t addr) {
    if (!mgr) return false;

    for (int i = 0; i < mgr->wp_count; i++) {
        Watchpoint* wp = &mgr->watchpoints[i];
        if (wp->enabled &&
            (wp->type == WP_TYPE_READ || wp->type == WP_TYPE_CHANGE) &&
            addr >= wp->address && addr < (wp->address + wp->length)) {
            wp->hit_count++;
            printf("\nWatchpoint %d hit on read at 0x%08X\n", i, addr);
            return true;
        }
    }
    return false;
}

bool wp_should_break_on_write(BreakpointManager* mgr, uint32_t addr, uint32_t value) {
    if (!mgr) return false;

    for (int i = 0; i < mgr->wp_count; i++) {
        Watchpoint* wp = &mgr->watchpoints[i];
        if (wp->enabled &&
            addr >= wp->address && addr < (wp->address + wp->length)) {

            if (wp->type == WP_TYPE_WRITE) {
                wp->hit_count++;
                printf("\nWatchpoint %d hit on write at 0x%08X (value=0x%08X)\n", i, addr, value);
                return true;
            }

            if (wp->type == WP_TYPE_CHANGE && value != wp->last_value) {
                wp->hit_count++;
                printf("\nWatchpoint %d hit on change at 0x%08X (old=0x%08X new=0x%08X)\n",
                       i, addr, wp->last_value, value);
                wp->last_value = value;
                return true;
            }

            /* Update last value for change detection */
            if (wp->type == WP_TYPE_CHANGE) {
                wp->last_value = value;
            }
        }
    }
    return false;
}

/* Simple expression evaluator for conditional breakpoints */
static uint32_t parse_register(const char* name, uint32_t* registers) {
    if (strcmp(name, "PC") == 0) return registers[0];  /* PC is first in array */
    if (strcmp(name, "I1") == 0) return registers[1];
    if (strcmp(name, "I2") == 0) return registers[2];
    if (strcmp(name, "I3") == 0) return registers[3];
    if (strcmp(name, "I4") == 0) return registers[4];
    if (strcmp(name, "L") == 0) return registers[5];
    if (strcmp(name, "B") == 0) return registers[6];
    if (strcmp(name, "R") == 0) return registers[7];
    return 0;
}

static uint32_t parse_number(const char* str) {
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        return (uint32_t)strtoul(str + 2, NULL, 16);
    }
    return (uint32_t)strtoul(str, NULL, 10);
}

ExprResult bp_evaluate_condition(const char* condition, uint32_t pc, uint32_t* registers, uint32_t* memory) {
    ExprResult result = {false, 0, ""};

    if (!condition || strlen(condition) == 0) {
        strcpy(result.error_msg, "Empty condition");
        return result;
    }

    /* Simple condition parser: supports register comparisons */
    /* Format: "REG == value" or "REG != value" or "REG > value" etc. */
    char expr[256];
    strncpy(expr, condition, sizeof(expr) - 1);
    expr[sizeof(expr) - 1] = '\0';

    /* Find comparison operator */
    char* op = NULL;
    if ((op = strstr(expr, " == ")) != NULL) {
        *op = '\0';
        char* reg_name = expr;
        char* value_str = op + 4;

        uint32_t reg_val = parse_register(reg_name, registers);
        uint32_t expected = parse_number(value_str);

        result.valid = true;
        result.value = (reg_val == expected) ? 1 : 0;
        return result;
    }
    else if ((op = strstr(expr, " != ")) != NULL) {
        *op = '\0';
        char* reg_name = expr;
        char* value_str = op + 4;

        uint32_t reg_val = parse_register(reg_name, registers);
        uint32_t expected = parse_number(value_str);

        result.valid = true;
        result.value = (reg_val != expected) ? 1 : 0;
        return result;
    }
    else if ((op = strstr(expr, " > ")) != NULL) {
        *op = '\0';
        char* reg_name = expr;
        char* value_str = op + 3;

        uint32_t reg_val = parse_register(reg_name, registers);
        uint32_t expected = parse_number(value_str);

        result.valid = true;
        result.value = (reg_val > expected) ? 1 : 0;
        return result;
    }
    else if ((op = strstr(expr, " < ")) != NULL) {
        *op = '\0';
        char* reg_name = expr;
        char* value_str = op + 3;

        uint32_t reg_val = parse_register(reg_name, registers);
        uint32_t expected = parse_number(value_str);

        result.valid = true;
        result.value = (reg_val < expected) ? 1 : 0;
        return result;
    }
    else {
        strcpy(result.error_msg, "Unsupported condition format");
        return result;
    }
}

bool wp_should_break_on_register_change(BreakpointManager* mgr, uint32_t reg_index, uint32_t value) {
    if (!mgr) return false;

    for (int i = 0; i < mgr->wp_count; i++) {
        Watchpoint* wp = &mgr->watchpoints[i];
        if (wp->enabled && wp->type == WP_TYPE_REGISTER && wp->address == reg_index) {
            if (value != wp->last_value) {
                wp->hit_count++;
                printf("\nRegister watchpoint %d hit on %s (old=0x%08X new=0x%08X)\n",
                       i, wp->register_name, wp->last_value, value);
                wp->last_value = value;
                return true;
            }
        }
    }
    return false;
}

