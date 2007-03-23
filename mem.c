#include "all.h"

void *allocate(const void *old, unsigned bytes)
{
	void *new = realloc((void *) old, bytes);

	if (!new && bytes)
		die("out of memory, tried to allocate %d bytes", bytes);
	return new;
}
