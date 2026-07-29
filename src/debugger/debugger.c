/*
 * Native ND-500 Debugger REPL
 * Uses shared command library with readline integration
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif
#include "debugger.h"
#include "commands.h"
#include "../machine/machine_protos.h"

/* Output callback for native debugger - prints to stdout */
static void native_output(const char* line, void* ctx) {
	(void)ctx; /* Unused */
	printf("%s\n", line);
}

/* Error callback for native debugger - prints to stderr */
static void native_error(const char* line, void* ctx) {
	(void)ctx; /* Unused */
	fprintf(stderr, "%s\n", line);
}

#ifdef HAVE_READLINE

/* Command generator for readline completion */
static char* command_generator(const char* text, int state) {
	static int list_index, len;
	static const char** command_list = NULL;

	if (!state) {
		list_index = 0;
		len = strlen(text);
		if (!command_list) {
			command_list = nd500_cmd_get_command_list();
		}
	}

	/* Check line buffer to determine context */
	char* line = rl_line_buffer;
	int point = rl_point;

	/* Find start of current word */
	int word_start = point;
	while (word_start > 0 && !isspace(line[word_start - 1])) {
		word_start--;
	}

	/* Check if we're completing a subcommand */
	const char** subcommands = NULL;
	if (word_start > 0) {
		/* Extract the first word (command) */
		char cmd[64];
		int cmd_len = 0;
		int i = 0;
		while (i < word_start && cmd_len < 63) {
			if (!isspace(line[i])) {
				cmd[cmd_len++] = line[i];
			} else if (cmd_len > 0) {
				break;
			}
			i++;
		}
		cmd[cmd_len] = '\0';

		if (cmd_len > 0) {
			subcommands = nd500_cmd_get_subcommands(cmd);
		}
	}

	/* Generate completions from appropriate list */
	if (subcommands) {
		while (subcommands[list_index]) {
			const char* match = subcommands[list_index++];
			if (len == 0 || strncmp(match, text, len) == 0) {
				return strdup(match);
			}
		}
	} else {
		while (command_list && command_list[list_index]) {
			const char* match = command_list[list_index++];
			if (len == 0 || strncmp(match, text, len) == 0) {
				return strdup(match);
			}
		}
	}

	return NULL;
}

/* Completion function for readline */
static char** command_completion(const char* text, int start, int end) {
	char** matches = NULL;
	(void)end; /* Unused */

	/* Always suppress filename completion */
	rl_attempted_completion_over = 1;

	if (start == 0) {
		/* Completing the first word (command) */
		matches = rl_completion_matches(text, command_generator);
	} else {
		/* Completing subsequent words (subcommands) */
		matches = rl_completion_matches(text, command_generator);
	}

	return matches;
}

/* History management */
static void load_history(void) {
	char* home = getenv("HOME");
	if (home) {
		char history_file[512];
		snprintf(history_file, sizeof(history_file), "%s/.nd500x_history", home);

		/* Load history from file (ignore errors) */
		read_history(history_file);
	}
}

static void save_history(void) {
	char* home = getenv("HOME");
	if (home) {
		char history_file[512];
		snprintf(history_file, sizeof(history_file), "%s/.nd500x_history", home);

		/* Save history to file */
		write_history(history_file);
	}
}

/* Execute history command (!!, !nnn, !string) */
static int execute_history_command(const char* line, char* output, size_t max_len) {
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
	return 0;
}

/* Read line with readline and tab completion */
static int read_line_with_completion(char* line, size_t max_len, uint32_t pc) {
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
}

#else /* !HAVE_READLINE */

/* Fallback without readline */
static int read_line_with_completion(char* line, size_t max_len, uint32_t pc) {
	printf("\x1b[90m[\x1b[0m\x1b[36m%08X\x1b[0m\x1b[90m]\x1b[0m ", pc);
	fflush(stdout);

	if (fgets(line, max_len, stdin) == NULL) {
		return 0;
	}

	/* Remove newline */
	line[strcspn(line, "\r\n")] = '\0';
	return 1;
}

#endif /* HAVE_READLINE */

/* Special commands that need native handling */
static int handle_special_commands(Nd500Machine* m, const char* line) {
	/* Handle 'history' command specially (requires readline state) */
	if (strcmp(line, "history") == 0) {
#ifdef HAVE_READLINE
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
		return 1;
	}

	/* Handle 'dap' command specially (requires DAP_ENABLED) */
	if (strncmp(line, "dap", 3) == 0) {
#ifdef DAP_ENABLED
		char* rest = (char*)(line + 3);
		while (*rest && isspace(*rest)) rest++;
		int port = rest && *rest ? atoi(rest) : 4500;
		if (nd500_dap_start(m, port) == 0) {
			printf("DAP server started on %d\n", port);
		} else {
			printf("failed to start DAP server\n");
		}
#else
		printf("DAP not available (libdap missing)\n");
#endif
		return 1;
	}

	return 0; /* Not a special command */
}

/* Set when another transport (telnet) also carries the guest console; see
 * nd500_debugger_set_stdin_eof_quiet() in debugger.h. */
static int g_stdin_eof_quiet = 0;

void nd500_debugger_set_stdin_eof_quiet(int quiet) { g_stdin_eof_quiet = quiet; }

/* Main debugger REPL */
int nd500_debugger_repl(Nd500Machine* m) {
	char line[256];
	int was_running = 0;  /* Track if we were running to detect stops */
	printf("nd500x debug mode. Commands: m, d, step, regs, load, run, stop, symb, show, bp, wp, continue, help, q\n");

#ifdef HAVE_READLINE
	printf("Tab completion and command history enabled - press TAB to complete, UP/DOWN for history\n");
	printf("History commands: !! (last), !nnn (number), !string (search), history (list)\n");

	/* Initialize readline completion */
	rl_attempted_completion_function = command_completion;
	rl_completion_append_character = '\0';
	rl_basic_word_break_characters = " \t\n\"\\'`@$><=;|&{(";

	/* Show all matches with single TAB press */
	rl_variable_bind("show-all-if-ambiguous", "on");

	/* Disable filename completion completely */
	rl_attempted_completion_over = 1;
	rl_completion_query_items = 0;  /* Don't ask "Display all X possibilities?" */

	/* Load history from file */
	load_history();
#endif

	/* Set up command context */
	CmdContext ctx = {
		.output = native_output,
		.error = native_error,
		.context = NULL
	};

	/* Console-forwarding mode: with ND500X_CONSOLE_STDIN=1, lines typed while
	 * the machine is running are sent to the guest console (mx_bin input ring
	 * via nd500_fecall_console_input) instead of being parsed as debugger
	 * commands. Prefix a line with '~' to force it to the debugger. */
	const char* cse = getenv("ND500X_CONSOLE_STDIN");
	int console_stdin = (cse && cse[0] && cse[0] != '0') ? 1 : 0;

	/* Main REPL loop */
	for (;;) {
		if (!read_line_with_completion(line, sizeof(line), m && m->cpu ? m->cpu->PC : 0)) {
			/* stdin EOF. In console-forwarding mode do NOT kill a running
			 * machine: the boot pipeline's input feeder closing (subshell
			 * exit, cat dying, redirected stdin draining) used to take the
			 * whole emulator down mid-boot. Linger until the machine stops
			 * on its own, then exit as before. */
			if (console_stdin && m && m->run_flag) {
				/* Ctrl-D at a live guest shell means EOF *to the guest*, not
				 * "kill the emulator": push EOT into the console input ring
				 * so the shell sees end-of-file (and, in single user, exits
				 * so init can move on). Only the SECOND consecutive EOF ends
				 * the session, matching how a terminal behaves. */
				extern void nd500_fecall_console_input(const char* buf, int len);
				static int eof_seen = 0;
				if (g_stdin_eof_quiet) {
					fprintf(stderr, "[repl] stdin closed - guest console still "
					        "served over telnet; machine keeps running\n");
					while (m->run_flag) {
						struct timespec ts = {0, 200000000}; /* 200ms */
						nanosleep(&ts, NULL);
					}
					break;
				}
				if (!eof_seen++) {
					const char eot = 0x04;
					nd500_fecall_console_input(&eot, 1);
					fprintf(stderr, "\n[repl] Ctrl-D sent to the guest console "
					        "(press again to detach)\n");
					continue;
				}
				fprintf(stderr, "[repl] stdin closed - console input ended; "
				        "machine keeps running (Ctrl-C to stop)\n");
				while (m->run_flag) {
					struct timespec ts = {0, 200000000}; /* 200ms */
					nanosleep(&ts, NULL);
				}
			}
			break;
		}
		if (console_stdin && m && m->run_flag) {
			if (line[0] == '~') {
				memmove(line, line + 1, strlen(line));   /* strip escape */
			} else {
				extern void nd500_fecall_console_input(const char* buf, int len);
				size_t n = strlen(line);
				if (n < sizeof(line) - 1) line[n++] = '\n';
				nd500_fecall_console_input(line, (int)n);
				continue;
			}
		}
		/* Check if execution stopped since last prompt */
		if (was_running && m && !m->run_flag) {
			if (m->stop_reason != STOP_NONE) {
				printf("\x1b[33mStopped:\x1b[0m %s at 0x%08X\n",
				       nd500_stop_reason_str(m->stop_reason), m->stop_addr);
			} else {
				printf("\x1b[33mStopped\x1b[0m at PC=0x%08X\n",
				       m->cpu ? m->cpu->PC : 0);
			}
			was_running = 0;
		}

		/* Skip empty lines */
		if (strlen(line) == 0) continue;

		/* Check for special commands that need native handling */
		if (handle_special_commands(m, line)) {
			was_running = m ? m->run_flag : 0;
			continue;
		}

		/* Execute command via shared library */
		int result = nd500_cmd_execute(m, line, &ctx);

		/* Update running state after command (might have started/stopped) */
		was_running = m ? m->run_flag : 0;

		/* Handle quit command */
		if (result == 1) {
#ifdef HAVE_READLINE
			save_history();
#endif
			break;
		}
	}

#ifdef HAVE_READLINE
	/* Save history on exit */
	save_history();
#endif

	return 0;
}
