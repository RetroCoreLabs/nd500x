/*
 * nd500x_telnet.h - single-client TCP/telnet terminal host for the shell.
 *
 * Reuses the telnet IAC state machine and POSIX socket approach from nd100x
 * (src/ndlib/telnetserver.c / net_compat.h) - adapted to nd500x's single
 * ConsoleIO terminal instead of nd100x's multi-terminal Device registry.
 *
 * A telnet client (TDV/VT emulator) connects, and its byte stream is bridged to
 * the emulator's ConsoleIO so the shell REPL and any DOM it runs talk to the
 * remote terminal exactly as they would to the local console.
 */
#ifndef ND500X_TELNET_H
#define ND500X_TELNET_H

#include "../../libmon/mon_file_table.h"   /* ConsoleIO */

/* Start listening on TCP <port>. Returns 0 on success, -1 on error. */
int nd500x_telnet_start(int port);

/* Block until a client connects (and telnet negotiation is sent). Returns 1 on
 * a live client, 0 on failure/shutdown. */
int nd500x_telnet_wait_client(void);

/* True while a client is connected. */
int nd500x_telnet_connected(void);

/* Stop the server and close the socket. */
void nd500x_telnet_stop(void);

/* The ConsoleIO bridging INBT/OUTBT to the telnet client (for running DOMs). */
ConsoleIO* nd500x_telnet_console(void);

/* Line I/O for the shell REPL over the telnet connection. */
void nd500x_telnet_write(const char* s);              /* send string (LF->CRLF) */
int  nd500x_telnet_readline(char* buf, int len);      /* read a line; -1 on disconnect */

#endif /* ND500X_TELNET_H */
