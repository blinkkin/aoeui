#include "all.h"

static struct buffer *clip_gap;

void clip_init(void)
{
	buffer_delete(clip_gap, 0, ~0);
}

unsigned clip(struct view *view, unsigned offset, unsigned bytes, int append)
{
	char *raw;

	if (!clip_gap)
		clip_gap = buffer_create();
	bytes = view_raw(view, &raw, offset, bytes);
	return buffer_insert(clip_gap, raw,
				 append ? buffer_bytes(clip_gap) : 0,
				 bytes);
}

unsigned clip_paste(struct view *view, unsigned offset)
{
	char *raw;
	unsigned bytes = buffer_raw(clip_gap, &raw, 0,
					buffer_bytes(clip_gap));
	return view_insert(view, raw, offset, bytes);
}
