#include "all.h"
#include <sys/mman.h>
#include <fcntl.h>

#ifndef S_IRUSR
#define S_IRUSR 0400
#endif
#ifndef S_IWUSR
#define S_IWUSR 0200
#endif

/*
 *	Buffers contain payload bytes and a "gap" of unused space.
 *	The gap is contiguous, and may appear in the midst of the
 *	payload.  Editing operations shift the gap around in order
 *	to enlarge it (when bytes are to be deleted) or to add new
 *	bytes to its boundaries.
 *
 *	Buffers are represented by pages mmap'ed from the file's
 *	temporary# file, or anonymous storage for things that aren't
 *	file texts,
 *
 *	Besides being used to hold the content of files, buffers
 *	are used for cut/copied text (the "clip buffer"), macros,
 *	and for undo histories.
 */

struct buffer *buffer_create(char *path)
{
	struct buffer *buffer = allocate0(sizeof *buffer);
	if (path && *path) {
		buffer->path = allocate(NULL, strlen(path) + 2);
		sprintf(buffer->path, "%s#", path);
		errno = 0;
		buffer->fd = open(buffer->path, O_CREAT|O_TRUNC|O_RDWR,
				  S_IRUSR|S_IWUSR);
		if (buffer->fd < 0)
			message("could not create temporary file %s",
				buffer->path);
	} else
		buffer->fd = -1;
	return buffer;
}

void buffer_destroy(struct buffer *buffer)
{
	if (!buffer)
		return;
	munmap(buffer->data, buffer->allocated);
	if (buffer->fd >= 0) {
		close(buffer->fd);
		unlink(buffer->path);
	}
	allocate(buffer->path, 0);
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
	if (buffer->fd >= 0 && gapsize)
		memset(buffer->data + buffer->gap, ' ', gapsize);
}

static void resize(struct buffer *buffer, unsigned bytes)
{
	void *p;
	char *old = buffer->data;
	int fd, mapflags = 0;
	static unsigned pagesize;

	/* Whole pages, with extras as size increases */
	if (!pagesize)
		pagesize = getpagesize();
	bytes += pagesize-1;
	bytes /= pagesize;
	bytes *= 11;
	bytes /= 10;
	bytes *= pagesize;

	if (bytes < buffer->allocated)
		munmap(old + bytes, buffer->allocated - bytes);
	if (buffer->fd >= 0 && bytes != buffer->allocated) {
		errno = 0;
		if (ftruncate(buffer->fd, bytes))
			die("could not adjust %s from 0x%x to 0x%x bytes",
			    buffer->path, buffer->allocated, bytes);
	}
	if (bytes <= buffer->allocated) {
		buffer->allocated = bytes;
		return;
	}

#ifdef MREMAP_MAYMOVE
	if (old) {
		/* attempt extension */
		errno = 0;
		p = mremap(old, buffer->allocated, bytes, MREMAP_MAYMOVE);
		if (p != MAP_FAILED)
			goto done;
	}
#endif

	/* new/replacement allocation */
	if ((fd = buffer->fd) >= 0) {
		mapflags |= MAP_SHARED;
		if (old) {
			munmap(old, buffer->allocated);
			old = NULL;
		}
	} else {
#ifdef MAP_ANONYMOUS
		mapflags |= MAP_ANONYMOUS;
#elif defined MAP_ANON
		mapflags |= MAP_ANON;
#else
		static int anonymous_fd = -1;
		if (anonymous_fd < 0) {
			errno = 0;
			anonymous_fd = open("/dev/zero", O_RDWR);
			if (anonymous_fd < 0)
				die("could not open /dev/zero for "
				    "anonymous mappings");
		}
		fd = anonymous_fd;
#endif
		mapflags |= MAP_PRIVATE;
	}

	errno = 0;
	p = mmap(0, bytes, PROT_READ|PROT_WRITE, mapflags, fd, 0);
	if (p == MAP_FAILED)
		die("mmap(0x%x bytes, fd %d) failed", bytes, fd);

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
	char *raw = NULL;
	bytes = buffer_raw(from, &raw, from_offset, bytes);
	buffer_insert(to, raw, to_offset, bytes);
	return buffer_delete(from, from_offset, bytes);
}

void buffer_snap(struct buffer *buffer)
{
	if (buffer && buffer->fd >= 0) {
		place_gap(buffer, buffer->payload);
		ftruncate(buffer->fd, buffer->payload);
	}
}
