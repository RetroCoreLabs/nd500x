#include <stdio.h>
#include <string.h>
#include "breakpoints.h"

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
    printf("ID  Address    Enabled  Hits   Type\n");
    printf("--- ---------- -------- ------ ----------\n");
    for (int i = 0; i < mgr->bp_count; i++) {
        Breakpoint* bp = &mgr->breakpoints[i];
        printf("%3d 0x%08X %-8s %6u %s\n",
               i, bp->address,
               bp->enabled ? "yes" : "no",
               bp->hit_count,
               bp->one_shot ? "one-shot" : "normal");
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
    
    mgr->wp_count++;
    const char* type_str = (type == WP_TYPE_READ) ? "read" :
                           (type == WP_TYPE_WRITE) ? "write" : "change";
    printf("Watchpoint %d set at 0x%08X (length=%u, type=%s)\n", id, address, length, type_str);
    return id;
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

