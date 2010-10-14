/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#include "all.h"

/*
 *	Buffers contain payload bytes and a "gap" of unused space.
 *	The gap is contiguous, and may appear in the midst of the
 *	payload.  Editing operations shift the gap around in order
 *	to enlarge it (when bytes are to be deleted) or to add new
 *	bytes to its boundaries.
 *
 *	The gap comprises all of the free space in a buffer.
 *	Since the cost of moving data does not depend on the
 *	distance by which it is moved, there is no point to
 *	using any gap size other than the full amount of unused
 *	storage.
 *
 *	Buffers are represented by pages mmap'ed from the file's
 *	temporary# file, or anonymous storage for things that aren't
 *	file texts.
 *
 *	Besides being used to hold the content of files, buffers
 *	are used for cut/copied text (the "clip buffer"), macros,
 *	and for undo histories.
 */

struct buffer *buffer_create(char *path)
{
	struct buffer *buffer = allocate0(sizeof *buffer);
	if (path && *path) {
		buffer->path = allocate(strlen(path) + 2);
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
	if (buffer) {
		munmap(buffer->data, buffer->mapped);
		if (buffer->fd >= 0) {
			close(buffer->fd);
			unlink(buffer->path);
		}
		RELEASE(buffer->path);
		RELEASE(buffer);
	}
}

static void place_gap(struct buffer *buffer, position_t offset)
{
	size_t gapsize = buffer_gap_bytes(buffer);

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

static void resize(struct buffer *buffer, size_t payload_bytes)
{
	void *p;
	char *old = buffer->data;
	fd_t fd;
	int mapflags = 0;
	size_t map_bytes = payload_bytes;

	static size_t pagesize;
	if (!pagesize)
		pagesize = getpagesize();

	/* Whole pages, with extras as size increases */
	map_bytes += pagesize-1;
	map_bytes /= pagesize;
	map_bytes *= 11;
	map_bytes /= 10;
	map_bytes *= pagesize;

	if (map_bytes < buffer->mapped)
		munmap(old + map_bytes, buffer->mapped - map_bytes);
	if (buffer->fd >= 0 && map_bytes != buffer->mapped) {
		errno = 0;
		if (ftruncate(buffer->fd, map_bytes))
			die("could not adjust %s from %lu to %lu bytes",
			    buffer->path, (long) buffer->mapped,
			    (long) map_bytes);
	}
	if (map_bytes <= buffer->mapped) {
		buffer->mapped = map_bytes;
		return;
	}
#ifdef MREMAP_MAYMOVE
	if (old) {
		/* attempt extension */
		errno = 0;
		p = mremap(old, buffer->mapped, map_bytes, MREMAP_MAYMOVE);
		if (p != MAP_FAILED)
			goto done;
#define NEED_DONE_LABEL
	}
#endif

	/* new/replacement allocation */
	if ((fd = buffer->fd) >= 0) {
		mapflags |= MAP_SHARED;
		if (old) {
			munmap(old, buffer->mapped);
			old = NULL;
		}
	} else {
#ifdef MAP_ANONYMOUS
		mapflags |= MAP_ANONYMOUS;
#elif defined MAP_ANON
		mapflags |= MAP_ANON;
#else
		static fd_t anonymous_fd = -1;
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
	p = mmap(0, map_bytes, PROT_READ|PROT_WRITE, mapflags, fd, 0);
	if (p == MAP_FAILED)
		die("mmap(%lu bytes, fd %d) failed", (long) map_bytes, fd);

	if (old) {
		memcpy(p, old, buffer->payload);
		munmap(old, buffer->mapped);
	}

#ifdef NEED_DONE_LABEL
done:
#endif
	buffer->data = p;
	buffer->mapped = map_bytes;
}

size_t buffer_raw(struct buffer *buffer, char **out,
		  position_t offset, size_t bytes)
{
	if (!buffer) {
		*out = NULL;
		return 0;
	}
	if (offset >= buffer->payload)
		offset = buffer->payload;
	if (offset + bytes > buffer->payload)
		bytes = buffer->payload - offset;
	if (!bytes) {
		*out = NULL;
		return 0;
	}

	if (offset < buffer->gap && offset + bytes > buffer->gap)
		place_gap(buffer, offset + bytes);
	*out = buffer->data + offset;
	if (offset >= buffer->gap)
		*out += buffer_gap_bytes(buffer);
	return bytes;
}

size_t buffer_get(struct buffer *buffer, void *out,
		  position_t offset, size_t bytes)
{
	size_t left;

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

size_t buffer_delete(struct buffer *buffer,
		     position_t offset, size_t bytes)
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

size_t buffer_insert(struct buffer *buffer, const void *in,
		     position_t offset, size_t bytes)
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

size_t buffer_move(struct buffer *to, position_t to_offset,
		   struct buffer *from, position_t from_offset,
		   size_t bytes)
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
		if (ftruncate(buffer->fd, buffer->payload)) {
			/* don't care */
		}
	}
}
