/*
 * Simulated Unix Kernel for ND-500
 *
 * This example demonstrates the NDIX-C kernel build process:
 * - Memory layout with fixed addresses (text at 0x08000000, data at 0x10000000)
 * - Compilation to assembly
 * - Linking with OMAGIC format (-i flag)
 * - Splitting into PSEG (instruction) and DSEG (data) segments
 *
 * Based on NDIX-C Release 3 kernel architecture
 */

/*
 * ND-500 Memory Layout (NDIX-C)
 *
 * Virtual Address Space:
 *   0x08000000 - _Textbase  - Kernel text (code) segment base
 *   0x10000000 - _Physbase  - Physical memory mapping region
 *   0x18000000 - _Sysbase   - System data structures
 *   0x20000000 - _usrpt     - User process page tables
 *   0x30000000 - _sharebase - Shared memory with ND-100 front-end
 *   0xE8000000 - _u         - Current process U-area (8KB)
 *   0xF0000000 - _Udata     - User data mapping
 *   0xF8000000 - _Ustack    - User stack mapping
 */

/* Kernel configuration constants */
#define MAXPROC  100    /* Maximum number of processes */
#define NFILE    128    /* Number of file table entries */
#define NINODE   64     /* Number of in-core inodes */
#define NBUF     32     /* Number of buffer headers */

/* Process states */
#define SSLEEP   1      /* Sleeping */
#define SWAIT    2      /* Waiting */
#define SRUN     3      /* Running */
#define SIDL     4      /* Intermediate state in creation */
#define SZOMB    5      /* Zombie */
#define SSTOP    6      /* Stopped */

/* System call numbers */
#define SYS_exit   1
#define SYS_read   3
#define SYS_write  4
#define SYS_open   5
#define SYS_close  6

/*
 * Process structure (simplified proc structure)
 */
struct proc {
    int p_stat;         /* Process status */
    int p_pid;          /* Process ID */
    int p_ppid;         /* Parent process ID */
    int p_pri;          /* Priority */
    int p_cpu;          /* CPU usage for scheduling */
    int p_nice;         /* Nice value for scheduling */
    int p_flag;         /* Process flags */
    char *p_addr;       /* Kernel virtual address of u-area */
};

/*
 * File structure (simplified file table entry)
 */
struct file {
    int f_flag;         /* File flags (read/write/etc) */
    int f_type;         /* File type */
    int f_count;        /* Reference count */
    int f_offset;       /* File offset */
    struct inode *f_inode; /* Pointer to inode */
};

/*
 * Inode structure (simplified in-core inode)
 */
struct inode {
    int i_flag;         /* Inode flags */
    int i_count;        /* Reference count */
    int i_dev;          /* Device */
    int i_number;       /* Inode number */
    int i_mode;         /* File mode */
    int i_nlink;        /* Number of links */
    int i_uid;          /* Owner user ID */
    int i_gid;          /* Owner group ID */
    int i_size;         /* File size in bytes */
};

/*
 * Buffer header (simplified buffer cache)
 */
struct buf {
    int b_flags;        /* Buffer flags */
    struct buf *b_forw; /* Forward link */
    struct buf *b_back; /* Backward link */
    int b_dev;          /* Device */
    int b_blkno;        /* Block number */
    char *b_addr;       /* Buffer address */
    int b_bcount;       /* Byte count */
};

/*
 * Kernel data structures (in DSEG)
 */
struct proc proctab[MAXPROC];   /* Process table */
struct file filetab[NFILE];     /* File table */
struct inode inodetab[NINODE];  /* In-core inode table */
struct buf buftab[NBUF];        /* Buffer headers */

/* Global kernel variables */
int nproc = 0;          /* Current number of processes */
int nfile = 0;          /* Current number of open files */
int ninode = 0;         /* Current number of in-core inodes */

/* Kernel statistics */
int ncpu = 1;           /* Number of CPUs */
int boottime = 0;       /* Boot time */
int hz = 100;           /* Clock frequency (ticks per second) */

/* Version string */
char version[] = "NDIX-C Simulated Kernel v1.0 for ND-500\n";

/*
 * Stack area for kernel (64KB)
 * Must be allocated before kernel initialization
 */
char stack_area[65536];     /* 64KB stack space */

/*
 * Kernel initialization data at fixed addresses
 * These simulate the shared memory region at 0x30000000
 */
char xmsg_cmd_buf[2048];    /* Message command buffer (0x30000000) */
char xmsg_resp_buf[2048];   /* Message response buffer (0x30000800) */
int clockrec;               /* Clock record (0x30001040) */
int console_in;             /* Console input (0x30001044) */
int console_out;            /* Console output (0x30001048) */

/*
 * Function prototypes (kernel code in PSEG)
 * Note: K&R C style - no parameter lists in prototypes
 */
void start();               /* Kernel entry point */
void init_kernel();         /* Initialize kernel data structures */
void init_proctab();        /* Initialize process table */
void init_filetab();        /* Initialize file table */
void init_inodetab();       /* Initialize inode table */
int sys_read();             /* System call: read */
int sys_write();            /* System call: write */
int sys_exit();             /* System call: exit */
void scheduler();           /* Process scheduler */
void trap_handler();        /* Trap/exception handler */

/*
 * Kernel main entry point (called from locore.c bootstrap)
 * Stack is already initialized by locore.c INIT instruction
 */
void kernel_main()
{
    /* Initialize kernel subsystems */
    init_kernel();

    /* Start scheduler */
    scheduler();

    /* Should never return */
    while(1)
        ;
}

/*
 * Initialize kernel data structures
 */
void init_kernel()
{
    int i;

    /* Clear statistics */
    nproc = 0;
    nfile = 0;
    ninode = 0;
    boottime = 0;

    /* Initialize subsystems */
    init_proctab();
    init_filetab();
    init_inodetab();

    /* Clear message buffers */
    for (i = 0; i < 2048; i++) {
        xmsg_cmd_buf[i] = 0;
        xmsg_resp_buf[i] = 0;
    }

    /* Set clock */
    clockrec = 0;
    console_in = 0;
    console_out = 0;
}

/*
 * Initialize process table
 */
void init_proctab()
{
    int i;

    for (i = 0; i < MAXPROC; i++) {
        proctab[i].p_stat = 0;      /* Not allocated */
        proctab[i].p_pid = 0;
        proctab[i].p_ppid = 0;
        proctab[i].p_pri = 0;
        proctab[i].p_cpu = 0;
        proctab[i].p_nice = 0;
        proctab[i].p_flag = 0;
        proctab[i].p_addr = (char *)0;
    }

    /* Create process 0 (swapper) */
    proctab[0].p_stat = SRUN;
    proctab[0].p_pid = 0;
    proctab[0].p_ppid = 0;
    nproc = 1;
}

/*
 * Initialize file table
 */
void init_filetab()
{
    int i;

    for (i = 0; i < NFILE; i++) {
        filetab[i].f_flag = 0;
        filetab[i].f_type = 0;
        filetab[i].f_count = 0;
        filetab[i].f_offset = 0;
        filetab[i].f_inode = (struct inode *)0;
    }
}

/*
 * Initialize inode table
 */
void init_inodetab()
{
    int i;

    for (i = 0; i < NINODE; i++) {
        inodetab[i].i_flag = 0;
        inodetab[i].i_count = 0;
        inodetab[i].i_dev = 0;
        inodetab[i].i_number = 0;
        inodetab[i].i_mode = 0;
        inodetab[i].i_nlink = 0;
        inodetab[i].i_uid = 0;
        inodetab[i].i_gid = 0;
        inodetab[i].i_size = 0;
    }
}

/*
 * System call: read
 */
int sys_read(fd, buf, count)
int fd;
char *buf;
int count;
{
    /* Simplified read - just return 0 */
    return 0;
}

/*
 * System call: write
 */
int sys_write(fd, buf, count)
int fd;
char *buf;
int count;
{
    /* Simplified write - just return count */
    return count;
}

/*
 * System call: exit
 */
int sys_exit(status)
int status;
{
    /* Mark current process as zombie */
    if (nproc > 0) {
        proctab[0].p_stat = SZOMB;
    }
    return 0;
}

/*
 * Process scheduler
 * Simple round-robin scheduler
 */
void scheduler()
{
    int i;
    int current;

    current = 0;

    /* Infinite scheduling loop */
    while (1) {
        /* Find next runnable process */
        for (i = 0; i < MAXPROC; i++) {
            current = (current + 1) % MAXPROC;

            if (proctab[current].p_stat == SRUN) {
                /* Switch to this process */
                /* In real kernel, this would do context switch */
                break;
            }
        }

        /* Update clock */
        clockrec++;
    }
}

/*
 * Trap/exception handler
 */
void trap_handler(trapno)
int trapno;
{
    /* Simple trap handler - just return */
    return;
}

/*
 * Main function (for testing without kernel entry point)
 * This won't be used when linked with locore.c bootstrap
 */
int main()
{
    /* Call kernel initialization */
    kernel_main();

    return 0;
}
