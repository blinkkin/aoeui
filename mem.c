#include "all.h"

void *allocate(const void *old, unsigned bytes)
{
	void *new = realloc((void *) old, bytes);
	if (!new && bytes)
		die("out of memory, tried to allocate %d bytes", bytes);
	return new;
}

void *allocate0(unsigned bytes)
{
	void *new = allocate(NULL, bytes);
	if (new)
		memset(new, 0, bytes);
	return new;
}
