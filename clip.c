/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#include "all.h"

static struct buffer **clip_buffer;
static unsigned clip_buffers;

void clip_init(unsigned reg)
{
	if (reg < clip_buffers)
		buffer_delete(clip_buffer[reg], 0, ~(size_t)0);
}

size_t clip(unsigned reg, struct view *view, position_t offset,
	    size_t bytes, Boolean_t append)
{
	char *raw;

	if (reg >= clip_buffers) {
		clip_buffer = reallocate(clip_buffer,
					 (reg+1) * sizeof *clip_buffer);
		memset(clip_buffer + clip_buffers, 0,
		       (reg + 1 - clip_buffers) * sizeof *clip_buffer);
		clip_buffers = reg + 1;
	}

	if (!clip_buffer[reg])
		clip_buffer[reg] = buffer_create(NULL);
	bytes = view_raw(view, &raw, offset, bytes);
	return buffer_insert(clip_buffer[reg], raw,
			     append ? buffer_bytes(clip_buffer[reg]) : 0,
			     bytes);
}

size_t clip_paste(struct view *view, position_t offset, unsigned reg)
{
	char *raw;
	size_t bytes;

	if (reg >= clip_buffers)
		return 0;
	bytes = buffer_raw(clip_buffer[reg], &raw, 0,
			   buffer_bytes(clip_buffer[reg]));
	return view_insert(view, raw, offset, bytes);
}
