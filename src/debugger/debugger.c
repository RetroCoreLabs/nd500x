#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>
#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif
#include "debugger.h"
#include "../machine/machine_protos.h"
#include "../machine/breakpoints.h"
#include "../ndlib/ndlib.h"
#include "../cpu/cpu_protos.h"


static uint32_t parse_u32(const char* s, uint32_t defv) {
	if (!s || !*s) return defv;
	char* end = NULL;
	unsigned long v = 0;
	if (strncasecmp(s, "0x", 2) == 0) {
		v = strtoul(s + 2, &end, 16);
	} else {
		v = strtoul(s, &end, 10);
	}
	return (uint32_t)v;
}

/* Tab completion support */
static const char* debugger_commands[] = {
    "help", "?", "m", "d", "dis", "disasm", "step", "s", "regs", "set", "load", "run", "stop",
    "continue", "c", "cont", "symb", "symbols", "show", "bp", "break", "breakpoint",
    "wp", "watch", "watchpoint", "profile", "backtrace", "bt", "clear-traps", "history",
    "q", "quit", "exit", "dap"
};

static const char* show_subcommands[] = {
    "ea", "demangle", "trace", "profile", "trap", "traps", "trap-status"
};

static const char* bp_subcommands[] = {
    "cond", "list", "del", "enable", "disable"
};

static const char* wp_subcommands[] = {
    "reg", "list", "del", "enable", "disable"
};

static const char* profile_subcommands[] = {
    "show", "reset"
};

static const char* set_subcommands[] = {
    "PC", "I1", "I2", "I3", "I4", "A1", "A2", "A3", "A4", "E1", "E2", "E3", "E4",
    "L", "B", "R", "FLAGS", "TOS", "LL", "HL", "THA", "ST1", "ST2"
};

static int tab_complete_command(const char* partial, char* completion, size_t max_len) {
    int matches = 0;
    const char* match = NULL;
    
    for (size_t i = 0; i < sizeof(debugger_commands) / sizeof(debugger_commands[0]); i++) {
        if (strncmp(partial, debugger_commands[i], strlen(partial)) == 0) {
            matches++;
            match = debugger_commands[i];
        }
    }
    
    if (matches == 1 && match) {
        strncpy(completion, match, max_len - 1);
        completion[max_len - 1] = '\0';
        return 1;
    }
    
    return 0;
}

static int tab_complete_show_subcommand(const char* partial, char* completion, size_t max_len) {
    int matches = 0;
    const char* match = NULL;
    
    for (size_t i = 0; i < sizeof(show_subcommands) / sizeof(show_subcommands[0]); i++) {
        if (strncmp(partial, show_subcommands[i], strlen(partial)) == 0) {
            matches++;
            match = show_subcommands[i];
        }
    }
    
    if (matches == 1 && match) {
        strncpy(completion, match, max_len - 1);
        completion[max_len - 1] = '\0';
        return 1;
    }
    
    return 0;
}

static int tab_complete_bp_subcommand(const char* partial, char* completion, size_t max_len) {
    int matches = 0;
    const char* match = NULL;
    
    for (size_t i = 0; i < sizeof(bp_subcommands) / sizeof(bp_subcommands[0]); i++) {
        if (strncmp(partial, bp_subcommands[i], strlen(partial)) == 0) {
            matches++;
            match = bp_subcommands[i];
        }
    }
    
    if (matches == 1 && match) {
        strncpy(completion, match, max_len - 1);
        completion[max_len - 1] = '\0';
        return 1;
    }
    
    return 0;
}

static int tab_complete_wp_subcommand(const char* partial, char* completion, size_t max_len) {
    int matches = 0;
    const char* match = NULL;
    
    for (size_t i = 0; i < sizeof(wp_subcommands) / sizeof(wp_subcommands[0]); i++) {
        if (strncmp(partial, wp_subcommands[i], strlen(partial)) == 0) {
            matches++;
            match = wp_subcommands[i];
        }
    }
    
    if (matches == 1 && match) {
        strncpy(completion, match, max_len - 1);
        completion[max_len - 1] = '\0';
        return 1;
    }
    
    return 0;
}

static int tab_complete_profile_subcommand(const char* partial, char* completion, size_t max_len) {
    int matches = 0;
    const char* match = NULL;
    
    for (size_t i = 0; i < sizeof(profile_subcommands) / sizeof(profile_subcommands[0]); i++) {
        if (strncmp(partial, profile_subcommands[i], strlen(partial)) == 0) {
            matches++;
            match = profile_subcommands[i];
        }
    }
    
    if (matches == 1 && match) {
        strncpy(completion, match, max_len - 1);
        completion[max_len - 1] = '\0';
        return 1;
    }
    
    return 0;
}

static int handle_tab_completion(char* line, size_t* pos) {
    char completion[256];
    int completed = 0;
    
    /* Find the current word being typed */
    char* word_start = line;
    char* word_end = line + *pos;
    
    /* Find start of current word */
    while (word_start < word_end && !isspace(*(word_start))) {
        word_start++;
    }
    if (word_start < word_end) word_start++;
    
    /* Extract current word */
    size_t word_len = word_end - word_start;
    char current_word[256];
    strncpy(current_word, word_start, word_len);
    current_word[word_len] = '\0';
    
    /* Try to complete based on context */
    if (strncmp(line, "show ", 5) == 0) {
        completed = tab_complete_show_subcommand(current_word, completion, sizeof(completion));
    } else if (strncmp(line, "bp ", 3) == 0) {
        completed = tab_complete_bp_subcommand(current_word, completion, sizeof(completion));
    } else if (strncmp(line, "wp ", 3) == 0) {
        completed = tab_complete_wp_subcommand(current_word, completion, sizeof(completion));
    } else if (strncmp(line, "profile ", 8) == 0) {
        completed = tab_complete_profile_subcommand(current_word, completion, sizeof(completion));
    } else {
        completed = tab_complete_command(current_word, completion, sizeof(completion));
    }
    
    if (completed) {
        /* Replace current word with completion */
        size_t completion_len = strlen(completion);
        size_t remaining_len = strlen(word_end);
        
        /* Move remaining text to make room */
        memmove(word_start + completion_len, word_end, remaining_len + 1);
        
        /* Insert completion */
        memcpy(word_start, completion, completion_len);
        
        /* Update position */
        *pos = (word_start + completion_len) - line;
        
        return 1;
    }
    
    return 0;
}

/* Readline completion function */
static char* command_generator(const char* text, int state) {
    static int list_index, len;
    static const char* matches[] = {
        "help", "m", "d", "dis", "disasm", "step", "s", "regs", "load", "run", "stop",
        "continue", "c", "cont", "symb", "symbols", "show", "bp", "break", "breakpoint",
        "wp", "watch", "watchpoint", "profile", "backtrace", "bt", "clear-traps",
        "q", "quit", "exit", "dap"
    };
    static const char* show_matches[] = {
        "ea", "demangle", "trace", "profile", "trap", "traps", "trap-status"
    };
    static const char* bp_matches[] = {
        "cond", "list", "del", "enable", "disable"
    };
    static const char* wp_matches[] = {
        "reg", "list", "del", "enable", "disable"
    };
    static const char* profile_matches[] = {
        "show", "reset"
    };
    static const char* set_matches[] = {
        "PC", "I1", "I2", "I3", "I4", "A1", "A2", "A3", "A4", "E1", "E2", "E3", "E4",
        "L", "B", "R", "FLAGS", "TOS", "LL", "HL", "THA", "ST1", "ST2"
    };
    
    /* Check if we're completing a subcommand */
    char* line = rl_line_buffer;
    int point = rl_point;
    
    /* Find the start of the current word */
    int word_start = point;
    while (word_start > 0 && !isspace(line[word_start - 1])) {
        word_start--;
    }
    
    /* Check if we're after 'show ' */
    if (strncmp(line, "show ", 5) == 0 && word_start >= 5) {
        if (state == 0) {
            list_index = 0;
            len = text ? strlen(text) : 0;
        }
        
        while (list_index < sizeof(show_matches) / sizeof(show_matches[0])) {
            const char* match = show_matches[list_index++];
            if (len == 0 || strncmp(match, text, len) == 0) {
                return strdup(match);
            }
        }
        return NULL;
    }
    
    /* Check if we're after 'bp ' */
    if ((strncmp(line, "bp ", 3) == 0 || strncmp(line, "break ", 6) == 0 || strncmp(line, "breakpoint ", 11) == 0) && 
        word_start >= (strncmp(line, "bp ", 3) == 0 ? 3 : (strncmp(line, "break ", 6) == 0 ? 6 : 11))) {
        if (state == 0) {
            list_index = 0;
            len = text ? strlen(text) : 0;
        }
        
        while (list_index < sizeof(bp_matches) / sizeof(bp_matches[0])) {
            const char* match = bp_matches[list_index++];
            if (len == 0 || strncmp(match, text, len) == 0) {
                return strdup(match);
            }
        }
        return NULL;
    }
    
    /* Check if we're after 'wp ' */
    if ((strncmp(line, "wp ", 3) == 0 || strncmp(line, "watch ", 6) == 0 || strncmp(line, "watchpoint ", 11) == 0) && 
        word_start >= (strncmp(line, "wp ", 3) == 0 ? 3 : (strncmp(line, "watch ", 6) == 0 ? 6 : 11))) {
        if (state == 0) {
            list_index = 0;
            len = text ? strlen(text) : 0;
        }
        
        while (list_index < sizeof(wp_matches) / sizeof(wp_matches[0])) {
            const char* match = wp_matches[list_index++];
            if (len == 0 || strncmp(match, text, len) == 0) {
                return strdup(match);
            }
        }
        return NULL;
    }
    
    /* Check if we're after 'profile ' */
    if (strncmp(line, "profile ", 8) == 0 && word_start >= 8) {
        if (state == 0) {
            list_index = 0;
            len = text ? strlen(text) : 0;
        }
        
        while (list_index < sizeof(profile_matches) / sizeof(profile_matches[0])) {
            const char* match = profile_matches[list_index++];
            if (len == 0 || strncmp(match, text, len) == 0) {
                return strdup(match);
            }
        }
        return NULL;
    }
    
    /* Check if we're after 'set ' */
    if (strncmp(line, "set ", 4) == 0 && word_start >= 4) {
        if (state == 0) {
            list_index = 0;
            len = text ? strlen(text) : 0;
        }
        
        while (list_index < sizeof(set_matches) / sizeof(set_matches[0])) {
            const char* match = set_matches[list_index++];
            if (len == 0 || strncmp(match, text, len) == 0) {
                return strdup(match);
            }
        }
        return NULL;
    }
    
    if (!state) {
        list_index = 0;
        len = strlen(text);
    }
    
    /* Determine which list to use based on context */
    const char** match_list = matches;
    int match_count = sizeof(matches) / sizeof(matches[0]);
    
#ifdef HAVE_READLINE
    if (strncmp(rl_line_buffer, "show ", 5) == 0) {
        match_list = show_matches;
        match_count = sizeof(show_matches) / sizeof(show_matches[0]);
    } else if (strncmp(rl_line_buffer, "bp ", 3) == 0) {
        match_list = bp_matches;
        match_count = sizeof(bp_matches) / sizeof(bp_matches[0]);
    } else if (strncmp(rl_line_buffer, "wp ", 3) == 0) {
        match_list = wp_matches;
        match_count = sizeof(wp_matches) / sizeof(wp_matches[0]);
    } else if (strncmp(rl_line_buffer, "profile ", 8) == 0) {
        match_list = profile_matches;
        match_count = sizeof(profile_matches) / sizeof(profile_matches[0]);
    } else if (strncmp(rl_line_buffer, "set ", 4) == 0) {
        match_list = set_subcommands;
        match_count = sizeof(set_subcommands) / sizeof(set_subcommands[0]);
    }
#endif
    
    while (list_index < match_count) {
        if (strncmp(match_list[list_index], text, len) == 0) {
            return strdup(match_list[list_index++]);
        }
        list_index++;
    }
    
    return NULL;
}

static char** command_completion(const char* text, int start, int end) {
    char** matches = NULL;
    
    /* Always suppress filename completion */
    rl_attempted_completion_over = 1;
    
    /* Try our completion function */
    matches = rl_completion_matches(text, command_generator);
    
    return matches;
}

/* History management functions */
static void load_history(void) {
#ifdef HAVE_READLINE
    char* home = getenv("HOME");
    if (home) {
        char history_file[512];
        snprintf(history_file, sizeof(history_file), "%s/.nd500x_history", home);
        
        /* Load history from file */
        if (read_history(history_file) != 0) {
            /* File doesn't exist or error - that's okay */
        }
    }
#endif
}

static void save_history(void) {
#ifdef HAVE_READLINE
    char* home = getenv("HOME");
    if (home) {
        char history_file[512];
        snprintf(history_file, sizeof(history_file), "%s/.nd500x_history", home);
        
        /* Save history to file */
        write_history(history_file);
    }
#endif
}

static int execute_history_command(const char* line, char* output, size_t max_len) {
#ifdef HAVE_READLINE
    /* Check if line starts with ! */
    if (line[0] == '!') {
        const char* num_str = line + 1;
        
        /* Handle !! (last command) */
        if (num_str[0] == '!' && num_str[1] == '\0') {
            HIST_ENTRY* last = history_get(history_length);
            if (last) {
                strncpy(output, last->line, max_len - 1);
                output[max_len - 1] = '\0';
                return 1;
            }
            return 0;
        }
        
        /* Handle !nnn (specific history number) */
        char* end;
        long num = strtol(num_str, &end, 10);
        if (end != num_str && *end == '\0' && num > 0) {
            HIST_ENTRY* entry = history_get(num);
            if (entry) {
                strncpy(output, entry->line, max_len - 1);
                output[max_len - 1] = '\0';
                return 1;
            }
        }
        
        /* Handle !string (search for command starting with string) */
        HIST_ENTRY* entry = NULL;
        for (int i = history_length; i > 0; i--) {
            HIST_ENTRY* hist_entry = history_get(i);
            if (hist_entry && strstr(hist_entry->line, num_str) == hist_entry->line) {
                entry = hist_entry;
                break;
            }
        }
        if (entry) {
            strncpy(output, entry->line, max_len - 1);
            output[max_len - 1] = '\0';
            return 1;
        }
        
        return 0;
    }
#endif
    return 0;
}

static int read_line_with_tab_completion(char* line, size_t max_len, uint32_t pc) {
#ifdef HAVE_READLINE
    /* Create dynamic prompt with current PC */
    char prompt[64];
    snprintf(prompt, sizeof(prompt), "\x1b[90m[\x1b[0m\x1b[36m%08X\x1b[0m\x1b[90m]\x1b[0m ", pc);
    
    char* input = readline(prompt);
    if (input == NULL) {
        return 0;
    }
    
    /* Check for history commands */
    char history_output[256];
    if (execute_history_command(input, history_output, sizeof(history_output))) {
        printf("! %s\n", history_output);
        strncpy(line, history_output, max_len - 1);
        line[max_len - 1] = '\0';
        free(input);
        return 1;
    }
    
    /* Add to history if not empty and not a duplicate of last command */
    if (strlen(input) > 0) {
        HIST_ENTRY* last = history_get(history_length);
        if (!last || strcmp(input, last->line) != 0) {
            add_history(input);
        }
    }
    
    strncpy(line, input, max_len - 1);
    line[max_len - 1] = '\0';
    free(input);
    return 1;
#else
    /* Fallback to simple fgets with manual prompt */
    printf("\x1b[90m[\x1b[0m\x1b[36m%08X\x1b[0m\x1b[90m]\x1b[0m ", pc);
    fflush(stdout);
    
    if (fgets(line, max_len, stdin) == NULL) {
        return 0;
    }
    
    /* Remove newline */
    line[strcspn(line, "\r\n")] = '\0';
    return 1;
#endif
}

static void cmd_mem(Nd500Machine* m, const char* a1, const char* a2, uint32_t pc_default) {
	uint32_t addr = parse_u32(a1, pc_default);
	uint32_t len  = parse_u32(a2, 100);
    for (uint32_t i = 0; i < len; i += 16) {
        uint32_t line_addr = addr + i;
        char ascii[17];
        ascii[16] = '\0';
        /* ANSI colors */
        const char *c_reset = "\x1b[0m";
        const char *c_addr  = "\x1b[36m";   /* cyan */
        const char *c_dim   = "\x1b[90m";   /* bright black (dim) */

        /* Address */
        printf("%s%08X%s: ", c_addr, line_addr, c_reset);

        /* Hex bytes (grouped 8+8) */
        for (uint32_t j = 0; j < 16; ++j) {
            uint32_t idx = i + j;
            if (idx < len) {
                uint8_t b = nd500_bus_read8(m, line_addr + j);
                /* Dim zero bytes to make patterns pop */
                if (b == 0x00) printf("%s%02X%s ", c_dim, b, c_reset);
                else           printf("%02X ", b);
                ascii[j] = isprint(b) ? (char)b : '.';
            } else {
                printf("   ");
                ascii[j] = ' ';
            }
            if (j == 7) printf(" "); /* extra gap between 8-byte groups */
        }

        /* ASCII column */
        printf(" |");
        for (uint32_t j = 0; j < 16; ++j) {
            char ch = ascii[j];
            if (ch == '\0') ch = ' ';
            if (ch == '.') {
                printf("%s.%s", c_dim, c_reset);
            } else {
                putchar(ch);
            }
        }
        printf("|\n");
    }
}

static void cmd_dis(Nd500Machine* m, const char* a1, const char* a2, uint32_t pc_default) {
	uint32_t addr = parse_u32(a1, pc_default);
	uint32_t len  = parse_u32(a2, 100);
	nd500_dbg_disasm_print(m, addr, len);
}

int nd500_debugger_repl(Nd500Machine* m) {
	char line[256];
    printf("nd500x debug mode. Commands: m, d, step, regs, load, run, stop, symb, show, bp, wp, continue, help, q\n");
    
#ifdef HAVE_READLINE
    printf("Tab completion and command history enabled - press TAB to complete, UP/DOWN for history\n");
    printf("History commands: !! (last), !nnn (number), !string (search), history (list)\n");
    /* Initialize readline completion */
    rl_attempted_completion_function = command_completion;
    rl_completion_append_character = '\0';
    rl_basic_word_break_characters = "\t\n\"\\'`@$><=;|&{(";
    
    /* Disable filename completion completely */
    rl_attempted_completion_over = 1;
    rl_completion_query_items = 0;  /* Don't ask "Display all X possibilities?" */
    
    /* Load history from file */
    load_history();
#else
    printf("Tab completion enabled - press TAB to complete commands\n");
#endif
    
    while (read_line_with_tab_completion(line, sizeof(line), m && m->cpu ? m->cpu->PC : 0)) {
		char* tok = strtok(line, " \t\r\n");
		if (!tok) continue;
		if (strcmp(tok, "q") == 0 || strcmp(tok, "quit") == 0 || strcmp(tok, "exit") == 0) {
#ifdef HAVE_READLINE
			/* Save history before exiting */
			save_history();
#endif
			break;
		}
        else if (strcmp(tok, "m") == 0) {
			char* a1 = strtok(NULL, " \t\r\n");
			char* a2 = strtok(NULL, " \t\r\n");
            uint32_t pc = m->cpu ? m->cpu->PC : 0;
            cmd_mem(m, a1, a2, pc);
        } else if (strcmp(tok, "d") == 0 || strcasecmp(tok, "dis") == 0 || strcasecmp(tok, "disasm") == 0) {
			char* a1 = strtok(NULL, " \t\r\n");
			char* a2 = strtok(NULL, " \t\r\n");
            uint32_t pc = m->cpu ? m->cpu->PC : 0;
            cmd_dis(m, a1, a2, pc);
        } else if (strcmp(tok, "show") == 0) {
            char* sub = strtok(NULL, " \t\r\n");
            if (!sub) { printf("usage: show ea [on|off]\n"); continue; }
            if (strcmp(sub, "ea") == 0) {
                char* val = strtok(NULL, " \t\r\n");
                int newv;
                if (!val) {
                    /* toggle */
                    int cur = nd500_dbg_get_show_ea();
                    newv = !cur;
                } else if (strcasecmp(val, "on") == 0) {
                    newv = 1;
                } else if (strcasecmp(val, "off") == 0) {
                    newv = 0;
                } else {
                    printf("usage: show ea [on|off]\n");
                    continue;
                }
                nd500_dbg_set_show_ea(newv);
                printf("show ea: %s\n", newv ? "on" : "off");
            } else if (strcmp(sub, "demangle") == 0) {
                char* val = strtok(NULL, " \t\r\n");
                int newv;
                if (!val) {
                    int cur = nd500_dbg_get_demangle();
                    newv = !cur;
                } else if (strcasecmp(val, "on") == 0) {
                    newv = 1;
                } else if (strcasecmp(val, "off") == 0) {
                    newv = 0;
                } else { printf("usage: show demangle [on|off]\n"); continue; }
                nd500_dbg_set_demangle(newv);
                printf("show demangle: %s\n", newv ? "on" : "off");
            } else if (strcmp(sub, "trace") == 0) {
                char* val = strtok(NULL, " \t\r\n");
                int newv;
                if (!val) {
                    int cur = nd500_dbg_get_trace_mode();
                    newv = !cur;
                } else if (strcasecmp(val, "on") == 0) {
                    newv = 1;
                } else if (strcasecmp(val, "off") == 0) {
                    newv = 0;
                } else { printf("usage: show trace [on|off]\n"); continue; }
                nd500_dbg_set_trace_mode(newv);
                printf("show trace: %s\n", newv ? "on" : "off");
            } else if (strcmp(sub, "profile") == 0) {
                char* val = strtok(NULL, " \t\r\n");
                int newv;
                if (!val) {
                    int cur = nd500_dbg_get_profiling();
                    newv = !cur;
                } else if (strcasecmp(val, "on") == 0) {
                    newv = 1;
                } else if (strcasecmp(val, "off") == 0) {
                    newv = 0;
                } else { printf("usage: show profile [on|off]\n"); continue; }
                nd500_dbg_set_profiling(newv);
                printf("show profile: %s\n", newv ? "on" : "off");
            } else if (strcmp(sub, "trap") == 0) {
                char* val = strtok(NULL, " \t\r\n");
                int newv;
                if (!val) {
                    int cur = nd500_dbg_get_trap_invalid();
                    newv = !cur;
                } else if (strcasecmp(val, "on") == 0) {
                    newv = 1;
                } else if (strcasecmp(val, "off") == 0) {
                    newv = 0;
                } else {
                    printf("usage: show trap [on|off]\n");
                    continue;
                }
                nd500_dbg_set_trap_invalid(newv);
                printf("show trap: %s\n", newv ? "on" : "off");
            } else if (strcmp(sub, "traps") == 0) {
                char* val = strtok(NULL, " \t\r\n");
                if (!val) {
                    printf("usage: show traps [on|off]\n");
                    continue;
                }
                if (strcasecmp(val, "on") == 0) {
                    printf("Trap system: enabled\n");
                    printf("  - Invalid instruction 0x00 trap: enabled\n");
                    printf("  - Illegal instruction trap: enabled\n");
                    printf("  - Trap handler integration: enabled\n");
                } else if (strcasecmp(val, "off") == 0) {
                    printf("Trap system: disabled\n");
                } else {
                    printf("usage: show traps [on|off]\n");
                }
            } else if (strcmp(sub, "trap-status") == 0) {
                if (nd500_dbg_trap_occurred()) {
                    const char* desc = nd500_dbg_get_trap_description();
                    printf("Trap occurred: %s\n", desc ? desc : "Unknown trap");
                } else {
                    printf("No traps pending\n");
                }
            } else {
                printf("unknown show option\n");
            }
        } else if (strcmp(tok, "step") == 0 || strcmp(tok, "s") == 0) {
			char* a1 = strtok(NULL, " \t\r\n");
			uint32_t n = parse_u32(a1, 1);
			for (uint32_t i = 0; i < n; ++i) nd500_dbg_step(m, 1);
			printf("ok\n");
        } else if (strcmp(tok, "regs") == 0) {
            if (!m->cpu) { printf("no cpu linked\n"); continue; }
            Nd500Regs r; memset(&r, 0, sizeof(r));
            nd500_dbg_regs(m->cpu, &r);
            printf("PC=%08X FLAGS=%08X\n", r.PC, r.FLAGS);
            printf("I: %08X %08X %08X %08X\n", r.I[0], r.I[1], r.I[2], r.I[3]);
            printf("A: %08X %08X %08X %08X\n", r.A[0], r.A[1], r.A[2], r.A[3]);
            printf("E: %08X %08X %08X %08X\n", r.E[0], r.E[1], r.E[2], r.E[3]);
            printf("L=%08X B=%08X R=%08X\n", r.L, r.B, r.R);
            printf("TOS=%08X LL=%08X HL=%08X THA=%08X\n", r.TOS, r.LL, r.HL, r.THA);
            printf("OTE1=%08X OTE2=%08X CTE1=%08X CTE2=%08X\n", r.OTE1, r.OTE2, r.CTE1, r.CTE2);
            printf("MTE1=%08X MTE2=%08X TEMM1=%08X TEMM2=%08X\n", r.MTE1, r.MTE2, r.TEMM1, r.TEMM2);
        } else if (strcmp(tok, "load") == 0) {
			char* path = strtok(NULL, " \t\r\n");
			if (!path) { printf("usage: load <path>\n"); continue; }
			uint32_t entry = 0;
            if (ndlib_loadaout_file(m, path, &entry) == 0) {
				printf("loaded, entry=0x%08X\n", entry);
                if (ndlib_symbols_load(path) == 0) {
                    printf("symbols loaded\n");
                }
                /* Print metadata similar to nd500-dump */
                (void)ndlib_aout_dump_metadata(path);
			} else {
				printf("load failed\n");
			}
		} else if (strcmp(tok, "run") == 0) {
			nd500_dbg_run(m);
			printf("running...\n");
		} else if (strcmp(tok, "stop") == 0) {
			nd500_dbg_stop(m);
			printf("stopped\n");
        } else if (strcmp(tok, "symb") == 0 || strcmp(tok, "symbols") == 0) {
			ndlib_symbols_list_all();
        } else if (strcmp(tok, "profile") == 0) {
            char* subcmd = strtok(NULL, " \t\r\n");
            if (!subcmd || strcmp(subcmd, "show") == 0) {
                nd500_dbg_show_profile();
            } else if (strcmp(subcmd, "reset") == 0) {
                nd500_dbg_reset_profile();
            } else {
                printf("usage: profile [show|reset]\n");
            }
        } else if (strcmp(tok, "backtrace") == 0 || strcmp(tok, "bt") == 0) {
            nd500_dbg_show_backtrace();
        } else if (strcmp(tok, "bp") == 0 || strcmp(tok, "break") == 0 || strcmp(tok, "breakpoint") == 0) {
			/* Breakpoint commands: bp [addr], bp list, bp del <id>, bp enable <id>, bp disable <id> */
			char* a1 = strtok(NULL, " \t\r\n");
			char* a2 = strtok(NULL, " \t\r\n");
			
			if (!m->bp_mgr) { printf("no breakpoint manager\n"); continue; }
			
			if (!a1 || strcasecmp(a1, "list") == 0 || strcasecmp(a1, "ls") == 0) {
				bp_list(m->bp_mgr);
			} else if (strcasecmp(a1, "del") == 0 || strcasecmp(a1, "delete") == 0) {
				if (!a2) { printf("usage: bp del <id>\n"); continue; }
				int id = (int)parse_u32(a2, -1);
				bp_delete(m->bp_mgr, id);
			} else if (strcasecmp(a1, "enable") == 0 || strcasecmp(a1, "en") == 0) {
				if (!a2) { printf("usage: bp enable <id>\n"); continue; }
				int id = (int)parse_u32(a2, -1);
				bp_enable(m->bp_mgr, id);
			} else if (strcasecmp(a1, "disable") == 0 || strcasecmp(a1, "dis") == 0) {
				if (!a2) { printf("usage: bp disable <id>\n"); continue; }
				int id = (int)parse_u32(a2, -1);
				bp_disable(m->bp_mgr, id);
			} else if (strcasecmp(a1, "cond") == 0 || strcasecmp(a1, "conditional") == 0) {
				/* Conditional breakpoint: bp cond <addr> <condition> */
				if (!a2) { printf("usage: bp cond <addr> <condition>\n"); continue; }
				uint32_t addr = parse_u32(a2, m->cpu ? m->cpu->PC : 0);
				char* condition = strtok(NULL, "\r\n"); /* Get rest of line as condition */
				if (!condition) { printf("usage: bp cond <addr> <condition>\n"); continue; }
				bp_add_conditional(m->bp_mgr, addr, condition, false);
			} else {
				/* Set breakpoint at address */
				uint32_t addr = parse_u32(a1, m->cpu ? m->cpu->PC : 0);
				bp_add(m->bp_mgr, addr, false);
			}
        } else if (strcmp(tok, "wp") == 0 || strcmp(tok, "watch") == 0 || strcmp(tok, "watchpoint") == 0) {
			/* Watchpoint commands: wp <addr> [len] [type], wp reg <reg>, wp list, wp del <id> */
			char* a1 = strtok(NULL, " \t\r\n");
			char* a2 = strtok(NULL, " \t\r\n");
			char* a3 = strtok(NULL, " \t\r\n");
			
			if (!m->bp_mgr) { printf("no breakpoint manager\n"); continue; }
			
			if (!a1 || strcasecmp(a1, "list") == 0 || strcasecmp(a1, "ls") == 0) {
				wp_list(m->bp_mgr);
			} else if (strcasecmp(a1, "del") == 0 || strcasecmp(a1, "delete") == 0) {
				if (!a2) { printf("usage: wp del <id>\n"); continue; }
				int id = (int)parse_u32(a2, -1);
				wp_delete(m->bp_mgr, id);
			} else if (strcasecmp(a1, "enable") == 0 || strcasecmp(a1, "en") == 0) {
				if (!a2) { printf("usage: wp enable <id>\n"); continue; }
				int id = (int)parse_u32(a2, -1);
				wp_enable(m->bp_mgr, id);
			} else if (strcasecmp(a1, "disable") == 0 || strcasecmp(a1, "dis") == 0) {
				if (!a2) { printf("usage: wp disable <id>\n"); continue; }
				int id = (int)parse_u32(a2, -1);
				wp_disable(m->bp_mgr, id);
			} else if (strcasecmp(a1, "reg") == 0 || strcasecmp(a1, "register") == 0) {
				/* Register watchpoint: wp reg <reg_name> */
				if (!a2) { printf("usage: wp reg <register_name>\n"); continue; }
				
				/* Map register names to indices */
				uint32_t reg_index = 0;
				if (strcasecmp(a2, "PC") == 0) reg_index = 0;
				else if (strcasecmp(a2, "I1") == 0) reg_index = 1;
				else if (strcasecmp(a2, "I2") == 0) reg_index = 2;
				else if (strcasecmp(a2, "I3") == 0) reg_index = 3;
				else if (strcasecmp(a2, "I4") == 0) reg_index = 4;
				else if (strcasecmp(a2, "L") == 0) reg_index = 5;
				else if (strcasecmp(a2, "B") == 0) reg_index = 6;
				else if (strcasecmp(a2, "R") == 0) reg_index = 7;
				else { printf("Unknown register: %s\n", a2); continue; }
				
				wp_add_register(m->bp_mgr, a2, reg_index);
			} else {
				/* Set watchpoint: wp <addr> [len] [read|write|change] */
				uint32_t addr = parse_u32(a1, 0);
				uint32_t len = a2 ? parse_u32(a2, 4) : 4;
				WatchpointType type = WP_TYPE_WRITE; /* default */
				
				if (a3) {
					if (strcasecmp(a3, "read") == 0 || strcasecmp(a3, "r") == 0) type = WP_TYPE_READ;
					else if (strcasecmp(a3, "write") == 0 || strcasecmp(a3, "w") == 0) type = WP_TYPE_WRITE;
					else if (strcasecmp(a3, "change") == 0 || strcasecmp(a3, "c") == 0) type = WP_TYPE_CHANGE;
				}
				
				wp_add(m->bp_mgr, addr, len, type);
			}
        } else if (strcmp(tok, "continue") == 0 || strcmp(tok, "c") == 0 || strcmp(tok, "cont") == 0) {
			/* Continue execution after hitting a breakpoint */
			if (!m->cpu) { printf("no cpu linked\n"); continue; }
			/* Clear any pending traps before continuing */
			nd500_dbg_clear_traps();
			nd500_dbg_run(m);
			printf("continuing...\n");
        } else if (strcmp(tok, "clear-traps") == 0) {
			/* Clear any pending traps */
			nd500_dbg_clear_traps();
			printf("Traps cleared\n");
        } else if (strcmp(tok, "history") == 0) {
#ifdef HAVE_READLINE
            /* Show command history */
            printf("Command History:\n");
            for (int i = 1; i <= history_length; i++) {
                HIST_ENTRY* entry = history_get(i);
                if (entry) {
                    printf("%4d  %s\n", i, entry->line);
                }
            }
            if (history_length == 0) {
                printf("No commands in history\n");
            }
#else
            printf("History not available (readline not found)\n");
#endif
        } else if (strcmp(tok, "set") == 0) {
            /* Set register value */
            if (!m->cpu) { 
                printf("no cpu linked\n"); 
                continue; 
            }
            
            char* reg_name = strtok(NULL, " \t\r\n");
            char* value_str = strtok(NULL, " \t\r\n");
            
            if (!reg_name || !value_str) {
                printf("usage: set <register> <value>\n");
                printf("registers: PC, I1-I4, A1-A4, E1-E4, L, B, R, P, FLAGS, TOS, LL, HL, THA\n");
                continue;
            }
            
            uint32_t value = parse_u32(value_str, 0);
            
            /* Set register based on name */
            if (strcmp(reg_name, "PC") == 0) {
                m->cpu->PC = value;
                printf("PC = 0x%08X\n", value);
            } else if (strcmp(reg_name, "I1") == 0) {
                m->cpu->I[0] = value;
                printf("I1 = 0x%08X\n", value);
            } else if (strcmp(reg_name, "I2") == 0) {
                m->cpu->I[1] = value;
                printf("I2 = 0x%08X\n", value);
            } else if (strcmp(reg_name, "I3") == 0) {
                m->cpu->I[2] = value;
                printf("I3 = 0x%08X\n", value);
            } else if (strcmp(reg_name, "I4") == 0) {
                m->cpu->I[3] = value;
                printf("I4 = 0x%08X\n", value);
            } else if (strcmp(reg_name, "A1") == 0) {
                m->cpu->A[0] = value;
                printf("A1 = 0x%08X\n", value);
            } else if (strcmp(reg_name, "A2") == 0) {
                m->cpu->A[1] = value;
                printf("A2 = 0x%08X\n", value);
            } else if (strcmp(reg_name, "A3") == 0) {
                m->cpu->A[2] = value;
                printf("A3 = 0x%08X\n", value);
            } else if (strcmp(reg_name, "A4") == 0) {
                m->cpu->A[3] = value;
                printf("A4 = 0x%08X\n", value);
            } else if (strcmp(reg_name, "E1") == 0) {
                m->cpu->E[0] = value;
                printf("E1 = 0x%08X\n", value);
            } else if (strcmp(reg_name, "E2") == 0) {
                m->cpu->E[1] = value;
                printf("E2 = 0x%08X\n", value);
            } else if (strcmp(reg_name, "E3") == 0) {
                m->cpu->E[2] = value;
                printf("E3 = 0x%08X\n", value);
            } else if (strcmp(reg_name, "E4") == 0) {
                m->cpu->E[3] = value;
                printf("E4 = 0x%08X\n", value);
            } else if (strcmp(reg_name, "L") == 0) {
                m->cpu->L = value;
                printf("L = 0x%08X\n", value);
            } else if (strcmp(reg_name, "B") == 0) {
                m->cpu->B = value;
                printf("B = 0x%08X\n", value);
            } else if (strcmp(reg_name, "R") == 0) {
                m->cpu->R = value;
                printf("R = 0x%08X\n", value);
            } else if (strcmp(reg_name, "FLAGS") == 0) {
                m->cpu->FLAGS = value;
                printf("FLAGS = 0x%08X\n", value);
            } else if (strcmp(reg_name, "TOS") == 0) {
                m->cpu->TOS = value;
                printf("TOS = 0x%08X\n", value);
            } else if (strcmp(reg_name, "LL") == 0) {
                m->cpu->LL = value;
                printf("LL = 0x%08X\n", value);
            } else if (strcmp(reg_name, "HL") == 0) {
                m->cpu->HL = value;
                printf("HL = 0x%08X\n", value);
            } else if (strcmp(reg_name, "THA") == 0) {
                m->cpu->THA = value;
                printf("THA = 0x%08X\n", value);
            } else if (strcmp(reg_name, "ST1") == 0) {
                m->cpu->ST1 = value;
                printf("ST1 = 0x%08X\n", value);
            } else if (strcmp(reg_name, "ST2") == 0) {
                m->cpu->ST2 = value;
                printf("ST2 = 0x%08X\n", value);
            } else {
                printf("unknown register: %s\n", reg_name);
                printf("registers: PC, I1-I4, A1-A4, E1-E4, L, B, R, FLAGS, TOS, LL, HL, THA, ST1, ST2\n");
            }
        } else if (strcmp(tok, "dap") == 0) {
#ifdef WITH_DEBUGGER
            char* p = strtok(NULL, " \t\r\n");
            int port = p ? (int)parse_u32(p, 47285) : 47285;
            if (nd500_dap_start(m, port) == 0) printf("DAP server started on %d\n", port);
            else printf("failed to start DAP server\n");
#else
            printf("DAP not available (libdap missing)\n");
#endif
        } else if (strcmp(tok, "help") == 0 || strcmp(tok, "?") == 0) {
            printf("Commands:\n");
            printf("  help                        Show this help\n");
            printf("  m [addr [len]]              Hex dump memory (default addr=PC, len=100)\n");
            printf("  d [addr [len]]              Disassemble bytes (default addr=PC, len=100)\n");
            printf("  show ea [on|off]            Toggle/show effective-address breakdown in disassembly\n");
            printf("  show demangle [on|off]      Toggle C-symbol demangling (strip leading _)\n");
            printf("  show trace [on|off]         Toggle instruction execution tracing\n");
            printf("  show profile [on|off]      Toggle instruction execution profiling\n");
            printf("  profile [show|reset]       Show profiling statistics or reset data\n");
            printf("  backtrace (bt)             Show call stack backtrace\n");
            printf("  step [n] (s [n])            Execute n instructions (default 1)\n");
            printf("  regs                        Show CPU registers\n");
            printf("  set <register> <value>      Set register value\n");
            printf("  load <path>                 Load ND-500 a.out into memory\n");
            printf("  run                         Start execution (background)\n");
            printf("  stop                        Stop execution\n");
            printf("  continue (c/cont)           Continue execution after breakpoint\n");
            printf("  history                     Show command history\n");
            printf("  symb (symbols)              List all symbols\n");
            printf("\n");
            printf("Breakpoints:\n");
            printf("  bp [addr] (break/breakpoint) Set breakpoint at address (default: PC)\n");
            printf("  bp cond <addr> <condition>   Set conditional breakpoint\n");
            printf("  bp list                     List all breakpoints\n");
            printf("  bp del <id>                 Delete breakpoint\n");
            printf("  bp enable <id>              Enable breakpoint\n");
            printf("  bp disable <id>             Disable breakpoint\n");
            printf("\n");
            printf("Watchpoints:\n");
            printf("  wp <addr> [len] [type]      Set watchpoint (type: read, write, change)\n");
            printf("  wp reg <register>          Set register watchpoint (PC, I1-I4, L, B, R)\n");
            printf("  wp list                     List all watchpoints\n");
            printf("  wp del <id>                 Delete watchpoint\n");
            printf("  wp enable <id>              Enable watchpoint\n");
            printf("  wp disable <id>             Disable watchpoint\n");
            printf("  (watch/watchpoint)          Alternative names for wp\n");
            printf("\n");
            printf("Trap System:\n");
            printf("  show trap [on|off]          Toggle invalid instruction 0x00 trap\n");
            printf("  show traps [on|off]         Show trap system status\n");
            printf("  show trap-status            Show current trap status\n");
            printf("  clear-traps                 Clear any pending traps\n");
            printf("\n");
            printf("  dap <port>                  Start DAP server on port (WITH_DEBUGGER)\n");
            printf("  q (quit/exit)               Quit\n");
        } else {
			printf("unknown command\n");
		}
	}
	return 0;
}


