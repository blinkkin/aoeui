/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#include "all.h"

/* Error-checking wrappers for memory management */

void *reallocate(const void *old, size_t bytes)
{
	void *new = realloc((void *) old, bytes);
	if (!new && bytes)
		die("could not allocate %lu bytes", (long) bytes);
	return new;
}

void *allocate0(size_t bytes)
{
	void *new = allocate(bytes);
	if (new)
		memset(new, 0, bytes);
	return new;
}
