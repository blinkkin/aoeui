/* All sources #include this file. */

/* Standard and system headers */
#define _GNU_SOURCE /* for mremap */
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

#ifndef INLINE
# ifdef __GNUC__
#  define INLINE static __inline__
# else
#  define INLINE static
# endif
#endif

/* Module headers */
#include "buffer.h"
#include "mode.h"
#include "text.h"
#include "locus.h"
#include "utf8.h"
#include "window.h"
#include "util.h"
#include "clip.h"

/* Miscellaneous declarations and prototypes that didn't fit elsewhere */
extern struct termios original_termios;
void *allocate(const void *, unsigned bytes);	/* mem.c */
void die(const char *, ...);			/* die.c */
void message(const char *, ...);
int child(int *stdfd, unsigned stdfds, const char *argv[]);	/* child.c */
int multiplexor(int block);
void multiplex_write(int fd, const char *, int bytes, int retain);
