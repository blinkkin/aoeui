struct buffer {
	char *data;
	unsigned allocated, payload, gap;
	int fd;
	char *path;
};

struct buffer *buffer_create(char *path);
void buffer_destroy(struct buffer *);
unsigned buffer_raw(struct buffer *, char **,
		    unsigned offset, unsigned bytes);
unsigned buffer_get(struct buffer *, void *,
		    unsigned offset, unsigned bytes);
unsigned buffer_delete(struct buffer *,
		       unsigned offset, unsigned bytes);
unsigned buffer_insert(struct buffer *, const void *,
		       unsigned offset, unsigned bytes);
unsigned buffer_move(struct buffer *dest, unsigned,
		     struct buffer *src, unsigned, unsigned bytes);

INLINE unsigned buffer_bytes(struct buffer *buffer)
{
	return buffer ? buffer->payload : 0;
}

INLINE unsigned buffer_gap_bytes(struct buffer *buffer)
{
	return buffer->allocated - buffer->payload;
}

INLINE int buffer_byte(struct buffer *buffer, unsigned offset)
{
	if (!buffer || offset >= buffer->payload)
		return -1;
	if (offset >= buffer->gap)
		offset += buffer_gap_bytes(buffer);
	return offset[(unsigned char *) buffer->data];
}
