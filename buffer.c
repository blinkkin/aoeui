#include "all.h"
#include <sys/mman.h>
#ifndef MAP_ANONYMOUS
# include <fcntl.h>
#endif


/*
 *	Buffers contain payload bytes and a "gap" of unused space.
 *	The gap is contiguous, and may appear in the midst of the
 *	payload.  Editing operations shift the gap around in order
 *	to enlarge it (when bytes are to be deleted) or to add new
 *	bytes to its boundaries.
 *
 *	Buffers are represented by anonymous mmap'ed pages.
 *
 *	Besides being used to hold the content of files, buffers
 *	are used for cut/copied text (the "clip buffer") and for
 *	undo histories.
 */

static unsigned log2_pagesize;

static void compute_pagesize(void)
{
	int pgsz;

	if (log2_pagesize)
		return;
	pgsz = getpagesize();
	if (pgsz <= 4096)
		pgsz = 4096;
	for (log2_pagesize = 0; 1 << log2_pagesize < pgsz; log2_pagesize++)
		;
}

struct buffer *buffer_create(void)
{
	struct buffer *buffer = allocate(NULL, sizeof *buffer);

	if (!log2_pagesize)
		compute_pagesize();
	memset(buffer, 0, sizeof *buffer);
	return buffer;
}

void buffer_destroy(struct buffer *buffer)
{
	if (!buffer)
		return;
	munmap(buffer->data, buffer->allocated);
	allocate(buffer, 0);
}

static void place_gap(struct buffer *buffer, unsigned offset)
{
	unsigned gapsize = buffer_gap_bytes(buffer);

	if (offset > buffer->payload)
		offset = buffer->payload;
	if (offset <= buffer->gap)
		memmove(buffer->data + offset + gapsize, buffer->data + offset,
			buffer->gap - offset);
	else
		memmove(buffer->data + buffer->gap,
			buffer->data + buffer->gap + gapsize,
			offset - buffer->gap);
	buffer->gap = offset;
}

static void resize(struct buffer *buffer, unsigned bytes)
{
	void *p, *old;
	static int anonymous_fd;
	int mapflags;

	bytes += (1 << log2_pagesize) - 1;
	bytes = (bytes >> log2_pagesize) * 11 / 10 << log2_pagesize;

	if (bytes < buffer->allocated) {
		/* shrink */
		munmap(buffer->data + bytes, buffer->allocated - bytes);
		buffer->allocated = bytes;
		return;
	}
	if (bytes == buffer->allocated)
		return;

	old = buffer->data;

#ifdef MREMAP_MAYMOVE
	if (old) {
		/* attempt extension */
		errno = 0;
		p = mremap(buffer->data, buffer->allocated, bytes,
			   MREMAP_MAYMOVE);
		if (p != MAP_FAILED)
			goto done;
	}
#endif

	/* new/replacement allocation */
	mapflags = MAP_PRIVATE;
#ifdef MAP_ANONYMOUS
	mapflags |= MAP_ANONYMOUS;
#else
	if (!anonymous_fd) {
		errno = 0;
		anonymous_fd = open("/dev/zero", O_RDWR);
		if (anonymous_fd <= 0)
			die("could not open /dev/zero for anonymous "
			    "mappings");
	}
#endif
	errno = 0;
	p = mmap(0, bytes, PROT_READ|PROT_WRITE, mapflags, anonymous_fd, 0);
	if (p == MAP_FAILED)
		die("anonymous mmap(%d) failed", bytes);

	if (old) {
		memcpy(p, old, buffer->allocated);
		munmap(old, buffer->allocated);
	}

done:	buffer->data = p;
	buffer->allocated = bytes;
}

unsigned buffer_raw(struct buffer *buffer, char **out,
		    unsigned offset, unsigned bytes)
{
	if (!buffer)
		return 0;
	if (offset >= buffer->payload)
		offset = buffer->payload;
	if (offset + bytes > buffer->payload)
		bytes = buffer->payload - offset;
	if (!bytes)
		return 0;

	if (offset < buffer->gap && offset + bytes > buffer->gap)
		place_gap(buffer, offset + bytes);
	*out = buffer->data + offset;
	if (offset >= buffer->gap)
		*out += buffer_gap_bytes(buffer);
	return bytes;
}

unsigned buffer_get(struct buffer *buffer, void *out,
		    unsigned offset, unsigned bytes)
{
	unsigned left;

	if (!buffer)
		return 0;
	if (offset >= buffer->payload)
		offset = buffer->payload;
	if (offset + bytes > buffer->payload)
		bytes = buffer->payload - offset;
	if (!bytes)
		return 0;
	left = bytes;
	if (offset < buffer->gap) {
		unsigned before = buffer->gap - offset;
		if (before > bytes)
			before = bytes;
		memcpy(out, buffer->data + offset, before);
		out = (char *) out + before;
		offset += before;
		left -= before;
		if (!left)
			return bytes;
	}
	offset += buffer_gap_bytes(buffer);
	memcpy(out, buffer->data + offset, left);
	return bytes;
}

unsigned buffer_delete(struct buffer *buffer,
		       unsigned offset, unsigned bytes)
{
	if (!buffer)
		return 0;
	if (offset > buffer->payload)
		offset = buffer->payload;
	if (offset + bytes > buffer->payload)
		bytes = buffer->payload - offset;
	place_gap(buffer, offset);
	buffer->payload -= bytes;
	return bytes;
}

unsigned buffer_insert(struct buffer *buffer, const void *in,
		       unsigned offset, unsigned bytes)
{
	if (!buffer)
		return 0;
	if (offset > buffer->payload)
		offset = buffer->payload;
	if (bytes > buffer_gap_bytes(buffer)) {
		place_gap(buffer, buffer->payload);
		resize(buffer, buffer->payload + bytes);
	}
	place_gap(buffer, offset);
	if (in)
		memcpy(buffer->data + offset, in, bytes);
	else
		memset(buffer->data + offset, 0, bytes);
	buffer->gap += bytes;
	buffer->payload += bytes;
	return bytes;
}

unsigned buffer_move(struct buffer *to, unsigned to_offset,
		     struct buffer *from, unsigned from_offset,
		     unsigned bytes)
{
	char *raw;

	bytes = buffer_raw(from, &raw, from_offset, bytes);
	buffer_insert(to, raw, to_offset, bytes);
	return buffer_delete(from, from_offset, bytes);
}
