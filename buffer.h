/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#ifndef BUFFER_H
#define BUFFER_H

/* Gap buffers */

struct buffer;

struct buffer *buffer_create(char *path);
void buffer_destroy(struct buffer *);
size_t buffer_raw(struct buffer *, char **, position_t, size_t);
size_t buffer_get(struct buffer *, void *, position_t, size_t);
size_t buffer_delete(struct buffer *, position_t, size_t);
size_t buffer_insert(struct buffer *, const void *, position_t, size_t);
size_t buffer_move(struct buffer *dest, position_t,
		   struct buffer *src, position_t, size_t);
void buffer_snap(struct buffer *);

/* do *not* use directly; this definition is here
 * just for the inline functions.
 */
struct buffer {
	char *data;
	size_t payload, mapped;
	position_t gap;
	fd_t fd;
	char *path;
};

INLINE size_t buffer_bytes(struct buffer *buffer)
{
	return buffer ? buffer->payload : 0;
}

INLINE size_t buffer_gap_bytes(struct buffer *buffer)
{
	return buffer->mapped - buffer->payload;
}

INLINE int buffer_byte(struct buffer *buffer, size_t offset)
{
	if (!buffer || offset >= buffer->payload)
		return -1;
	if (offset >= buffer->gap)
		offset += buffer_gap_bytes(buffer);
	return offset[(Byte_t *) buffer->data];
}

#endif
