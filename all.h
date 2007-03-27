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

#define INLINE __inline__

#include "buffer.h"
#include "mode.h"
#include "text.h"
#include "locus.h"
#include "utf8.h"
#include "window.h"
#include "util.h"
#include "clip.h"

void *allocate(const void *, unsigned bytes);

void die(const char *, ...);
void message(const char *, ...);

extern struct text *text_list;
