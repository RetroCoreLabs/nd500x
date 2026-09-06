/*
 * ndix_ffs.h - read a file out of the NDIX root filesystem in a disk image.
 *
 * Enough of 4.3BSD FFS to look up a path and pull the bytes back, nothing more:
 * no writing, no allocation, no cylinder-group bookkeeping. It exists so that
 * "nd500x --ndix <image>" can find the kernel INSIDE the image and boot it,
 * making the image the only file that has to be delivered.
 *
 * This is faithful to how the machine really booted. machine/if.h:78 gives the
 * ND-500 kernel a booted[256] NAME in the FE_INIT response and
 * machine/machdep.c:831 only copies it - the ND-500 never locates or loads
 * itself. The ND-100 front end handed it a kernel that was already in memory,
 * and nd500x IS the front end here, so where the front end reads the kernel
 * from is its own business.
 *
 * The on-disk layout is not guessed. Every constant and offset below was read
 * out of the two programs that WRITE these images, which makes them the
 * authority on what is actually in them:
 *   pcc-nd500/src/nd500-mkfs/nd500-mkfs.c      (creates the filesystem)
 *   pcc-nd500/src/nd500-mkproto/nd500-mkproto.c (populates it)
 *
 * ASCII only - no unicode anywhere in this toolchain.
 */

#ifndef NDIX_FFS_H
#define NDIX_FFS_H

#include <stdint.h>
#include <stdio.h>

/* Read <path> (absolute, e.g. "/vmunix") out of the filesystem in <image_path>.
 *
 * Returns a malloc'd buffer holding the whole file, with *out_size set to its
 * length; the caller frees it. Returns NULL if the image cannot be opened, has
 * no NDIX filesystem, or does not contain the path - *out_size is 0 then. When
 * <why> is non-NULL it receives a short static reason string, suitable for an
 * error message; it is never NULL-terminated garbage and never needs freeing.
 *
 * Symbolic links are NOT followed and a component that is not a directory is a
 * lookup failure, so a path either resolves to one real file or fails. */
uint8_t* ndix_ffs_read_file(const char* image_path, const char* path,
                            long* out_size, const char** why);

/* Same, for an image that is already open - and which may not be a file at
 * all. The wasm front end holds the disc as bytes in memory and hands over an
 * fmemopen() stream, so the browser reads the kernel out of the image with
 * exactly the code the native build uses. The stream stays the caller's: this
 * neither closes nor rewinds it beyond its own seeks. */
uint8_t* ndix_ffs_read_file_fp(FILE* image, const char* path,
                               long* out_size, const char** why);

/* True if <image_path> holds a filesystem this reader understands. Cheap: reads
 * the super-block only. <why> as above. */
int ndix_ffs_probe(const char* image_path, const char** why);

#endif /* NDIX_FFS_H */
