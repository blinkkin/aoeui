#include "all.h"

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
