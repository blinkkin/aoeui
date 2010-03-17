/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
/* All editor sources #include this file. */

/* Standard and system headers */
#define _GNU_SOURCE /* for mremap */
#define _POSIX_C_SOURCE_199309 /* for nanosleep */
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <regex.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#ifdef __APPLE__
# include <util.h>
#else
# include <pty.h>
#endif

#ifndef NAME_MAX
# define NAME_MAX 256
#endif
#ifndef S_IRUSR
# define S_IRUSR 0400
#endif
#ifndef S_IWUSR
# define S_IWUSR 0200
#endif

#ifndef INLINE
# ifdef __GNUC__
#  define INLINE static __inline__
# else
#  define INLINE static
# endif
#endif

/* Module headers */
#include "types.h"
#include "utf8.h"
#include "buffer.h"
#include "locus.h"
#include "text.h"
#include "window.h"
#include "util.h"
#include "clip.h"
#include "macro.h"
#include "display.h"
#include "mode.h"

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
