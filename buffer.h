struct buffer {
	char *data;
	unsigned allocated, payload, gap;
};

struct buffer *buffer_create(void);
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

static INLINE unsigned buffer_bytes(struct buffer *buffer)
{
	return buffer ? buffer->payload : 0;
}

static INLINE unsigned buffer_gap_bytes(struct buffer *buffer)
{
	return buffer->allocated - buffer->payload;
}

static INLINE int buffer_byte(struct buffer *buffer, unsigned offset)
{
	if (!buffer || offset >= buffer->payload)
		return -1;
	if (offset >= buffer->gap)
		offset += buffer_gap_bytes(buffer);
	return offset[(unsigned char *) buffer->data];
}
