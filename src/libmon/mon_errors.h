/*
 * SINTRAN III monitor call error codes
 *
 * Monitor calls return an error number in W1 (I1) with the K flag set.
 *
 * IMPORTANT (radix): the SINTRAN manual's error table lists these in BOTH
 * octal and decimal. Monitor calls return the OCTAL value, but this emulator
 * (and the C# RetroCore emulator) store the DECIMAL value in W1. The names
 * below carry the DECIMAL value; the comment gives the octal for reference.
 *
 * Reference: SINTRAN III Monitor Calls (ND-60.228.2 EN), Appendix A,
 *            "Error Returns from SINTRAN III Background Programs".
 *            /mnt/e/Dev/Ronny/NDInsight/Developer/MON/Monitor Calls.md
 *
 * Do NOT invent error numbers. Every value here is quoted from that table.
 */

#ifndef MON_ERRORS_H
#define MON_ERRORS_H

/* --- General / file identity ------------------------------------------- */
#define MON_ERR_BAD_FILE_NUMBER        2    /* 002B Bad file number */
#define MON_ERR_END_OF_FILE            3    /* 003B End of file */
#define MON_ERR_DEVICE_NOT_RESERVED    5    /* 005B Device not reserved */
#define MON_ERR_END_OF_DEVICE         10    /* 012B End of device (timeout) */

/* --- Devices ------------------------------------------------------------ */
#define MON_ERR_NO_SUCH_DEVICE_NAME   24    /* 030B No such device name */
#define MON_ERR_DEVICE_ALREADY_RESERVED 133 /* 205B Device already reserved */

/* --- File names / versions --------------------------------------------- */
#define MON_ERR_NO_SUCH_FILE_NAME     46    /* 056B No such file name */
#define MON_ERR_AMBIGUOUS_FILE_NAME   47    /* 057B Ambiguous file name */
#define MON_ERR_NO_SUCH_FILE_VERSION  60    /* 074B No such file version */
#define MON_ERR_FILE_ALREADY_EXISTS   62    /* 076B File already exists */

/* --- Open state --------------------------------------------------------- */
#define MON_ERR_NO_SUCH_ACCESS_CODE   68    /* 104B No such access code */
#define MON_ERR_FILE_ALREADY_OPEN     69    /* 105B File already open */
#define MON_ERR_NOT_WRITE_ACCESS      70    /* 106B Not write access */
#define MON_ERR_TOO_MANY_FILES_OPEN   71    /* 107B Attempt to open too many files */
#define MON_ERR_NOT_READ_ACCESS       73    /* 111B Not read access */

/* --- Access-mode mismatches (random vs sequential) ---------------------- */
#define MON_ERR_NOT_OPEN_SEQ_WRITE    83    /* 123B Not open for sequential write */
#define MON_ERR_NOT_OPEN_SEQ_READ     84    /* 124B Not open for sequential read */
#define MON_ERR_NOT_OPEN_RAND_WRITE   85    /* 125B Not open for random write */
#define MON_ERR_NOT_OPEN_RAND_READ    86    /* 126B Not open for random read */

/* --- File-number table state -------------------------------------------- */
#define MON_ERR_FILE_NUMBER_RANGE     87    /* 127B File number out of range */
#define MON_ERR_FILE_NUMBER_IN_USE    88    /* 130B File number already used */
#define MON_ERR_FILE_NOT_OPEN         90    /* 132B No file opened with this number */
#define MON_ERR_NOT_MASS_STORAGE      91    /* 133B Not mass storage file */

/* --- Buffers / transfers ------------------------------------------------ */
#define MON_ERR_NO_BUFFER_SPACE       89    /* 131B No more buffer space */
#define MON_ERR_TRANSFER_ERROR        97    /* 141B Transfer error */

/* --- Blocks / addressing ------------------------------------------------ */
#define MON_ERR_NO_SUCH_BLOCK         99    /* 143B No such block */
#define MON_ERR_ILLEGAL_ADDRESS      107    /* 153B Illegal address reference in monitor call */

/* --- Parameters --------------------------------------------------------- */
#define MON_ERR_MISSING_PARAMETER    111    /* 157B Missing parameter */
#define MON_ERR_ILLEGAL_PARAMETER    124    /* 174B Illegal parameter */

#endif /* MON_ERRORS_H */
