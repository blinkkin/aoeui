#define _GNU_SOURCE /* for mremap */
#ifndef HELP_PATH
# define HELP_PATH "/usr/share/aoeui/help.txt"
#endif

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

#ifndef INLINE
# ifdef __GNUC__
#  define INLINE static __inline__
# else
#  define INLINE static
# endif
#endif

#include "buffer.h"
#include "mode.h"
#include "text.h"
#include "locus.h"
#include "utf8.h"
#include "window.h"
#include "util.h"
#include "clip.h"

void *allocate(const void *, unsigned bytes);	/* mem.c */
void die(const char *, ...);			/* die.c */
void message(const char *, ...);
int multiplexor(int block);			/* child.c */
void multiplex_write(int fd, const char *, unsigned, int close);
