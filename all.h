/* All editor sources #include this file. */

/* Standard and system headers */
#define _GNU_SOURCE /* for mremap */
#define _POSIX_C_SOURCE_199309 /* for nanosleep */
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>
#include <sys/time.h>

#ifndef INLINE
# ifdef __GNUC__
#  define INLINE static __inline__
# else
#  define INLINE static
# endif
#endif

#include "types.h"

/* Module headers */
#include "utf8.h"
#include "buffer.h"
#include "mode.h"
#include "locus.h"
#include "text.h"
#include "window.h"
#include "util.h"
#include "clip.h"
#include "macro.h"
#include "display.h"

/* Miscellaneous declarations and prototypes that didn't fit elsewhere */
extern struct termios original_termios;

void *reallocate(const void *, size_t);		/* mem.c */
#define allocate(sz) (reallocate(NULL, (sz)))
void *allocate0(size_t);
#define RELEASE(p) (reallocate((p), 0), (p) = NULL)

void depart(int exit_status);			/* die.c */
void die(const char *, ...);
void message(const char *, ...);
void status(const char *, ...);
void status_hide(void);

Boolean_t multiplexor(Boolean_t block);		/* child.c */
void multiplex_write(fd_t fd, const char *, ssize_t bytes, Boolean_t retain);
