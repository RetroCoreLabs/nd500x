/*
 * nd500x_ndix.h - NDIX boot setup (--ndix) and the guest-tty telnet bridge.
 *
 * --ndix absorbs what the run-ndix.sh wrapper used to do: point the front-end
 * call layer at a root disk image, set the environment defaults the NDIX boot
 * needs, find the kernel image and hand the debugger a "load vmunix" + "run"
 * script. Every path comes from the command line or is derived from it; none
 * is compiled in.
 *
 * The telnet bridge registers NDIX tty units with the terminal server copied
 * from nd100x, so a remote client can be a guest terminal.
 */
#ifndef ND500X_NDIX_H
#define ND500X_NDIX_H

/* Resolve paths, export the NDIX environment defaults and chdir to the kernel
 * directory. <image> is the root disk image; <kernel> may be NULL (then it is
 * derived, see the .c file); <root_opt> is an explicit --sintran-root or NULL.
 * On success *load_cmd receives the debugger "load ..." argument to use.
 * Returns 0 on success, -1 with a message on stderr otherwise. */
int nd500x_ndix_setup(const char* image, const char* kernel, const char* root_opt,
                      char* load_cmd, int load_cmd_len);

/* Start the terminal server on <port> and register the NDIX guest ttys.
 * Returns 0 on success, -1 on error. */
struct Nd500Machine;
/* Non-zero when no <kernel>.init exists, so --ndix must do the boot setup itself. */
int nd500x_ndix_autoboot_needed(void);
/* Run that setup, deriving load addresses from the .pseg/.dseg files. */
int nd500x_ndix_autoboot(struct Nd500Machine* m,
                         int (*run)(struct Nd500Machine*, const char*, void*),
                         void* ctx);

/* Serve the first <count> guest ttys - console, tty01, tty02, tty81, in that
 * order. count <= 0 means all of them; a count above the number the image has
 * is reported and clamped, never silently accepted. */
int nd500x_ndix_telnet_start(int port, int count);

/* Stop the terminal server (safe if it was never started). */
void nd500x_ndix_telnet_stop(void);

/* True once the terminal server is running. */
int nd500x_ndix_telnet_active(void);

#endif /* ND500X_NDIX_H */
