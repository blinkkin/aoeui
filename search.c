#include "all.h"

/*
 *	Incremental search mode
 */

struct mode_search {
	command command;
	struct mode *previous;
	unsigned char *pattern;
	unsigned bytes, alloc, last_bytes;
	int backward;
	unsigned start_locus;
};

static int match(int x, int y)
{
	if (x < 0 || y < 0)
		return 0;
	if (x >= 'a' && x <= 'z')
		x += 'A' - 'a';
	if (y >= 'a' && y <= 'z')
		y += 'A' - 'a';
	return x == y;
}

static int scan_forward(struct view *view, unsigned offset, unsigned max_offset)
{
	struct mode_search *mode = (struct mode_search *) view->mode;
	unsigned j;

	if (offset + mode->bytes > view->bytes)
		return -1;
	if (max_offset + mode->bytes > view->bytes)
		max_offset = view->bytes - mode->bytes;
	for (; offset < max_offset; offset++) {
		for (j = 0; j < mode->bytes; j++)
			if (!match(mode->pattern[j], view_byte(view, offset+j)))
				break;
		if (j == mode->bytes)
			return offset;
	}
	return -1;
}

static int scan_backward(struct view *view, unsigned offset, unsigned min_offset)
{
	struct mode_search *mode = (struct mode_search *) view->mode;
	unsigned j;

	if (min_offset + mode->bytes > view->bytes)
		return -1;
	if (offset + mode->bytes > view->bytes)
		offset = view->bytes - mode->bytes;
	for (; offset+1 > min_offset; offset--) {
		for (j = 0; j < mode->bytes; j++)
			if (!match(mode->pattern[j], view_byte(view, offset+j)))
				break;
		if (j == mode->bytes)
			return offset;
	}
	return -1;
}

static int search(struct view *view, int backward, int new)
{
	struct mode_search *mode = (struct mode_search *) view->mode;
	unsigned mark;
	int at;

	if (!mode->bytes) {
		locus_set(view, CURSOR, locus_get(view, mode->start_locus));
		locus_set(view, MARK, UNSET);
		return 1;
	}

	mark = locus_get(view, MARK);
	if (mark == UNSET)
		mark = locus_get(view, CURSOR);
	if (backward) {
		at = scan_backward(view, mark - !new, 0);
		if (at < 0)
			at = scan_backward(view, view->bytes, mark+1);
	} else {
		at = scan_forward(view, mark + !new, view->bytes);
		if (at < 0)
			at = scan_forward(view, 0, mark-1);
	}
	if (at < 0) {
		window_beep(view);
		return 0;
	}
	locus_set(view, MARK, at);
	locus_set(view, CURSOR, at + mode->bytes);
	mode->last_bytes = mode->bytes;
	return 1;
}

static void command_handler(struct view *view, unsigned ch)
{
	struct mode_search *mode = (struct mode_search *) view->mode;

	/* Backspace removes the last character from the search target and
	 * returns to the previous hit
	 */
	if (ch == 0x7f /*BCK*/) {
		if (!mode->bytes)
			window_beep(view);
		else {
			mode->bytes--;
			search(view, !mode->backward, 1);
		}
		return;
	}

	/* Non-control characters are appended to the search target and
	 * we proceed to the next hit if the current position does not
	 * match the extended target.
	 */
	if (ch >= ' ' || ch == '\t') {
		unsigned new;
		if (mode->bytes + 8 > mode->alloc) {
			mode->alloc = mode->bytes + 64;
			mode->pattern = allocate(mode->pattern, mode->alloc);
		}
		mode->bytes += new = utf8_out((char *) mode->pattern +
					      mode->bytes, ch);
		if (!search(view, mode->backward, new))
			mode->bytes -= new;
		return;
	}

	/* ^H moves to hit that is earlier in the text.
	 * ^T, ^/, and ^_ proceed to a later hit.
	 */
	if (mode->last_bytes &&
	    (ch == 'H'-'@' || ch == 'T'-'@' || ch == '_'-'@')) {
		mode->bytes = mode->last_bytes;
		search(view, mode->backward = (ch == 'H'-'@'), 0);
		return;
	}

	/* Hitting ^T, ^/, or ^_ with an empty search pattern causes
	 * the last successful search target to be reused.
	 */
	if ((ch == 'T'-'@' || ch == '_'-'@') &&
	    !mode->bytes && view->last_search) {
		mode->bytes = strlen(view->last_search);
		mode->alloc = mode->bytes + 8;
		mode->pattern = allocate(mode->pattern, mode->alloc);
		memcpy(mode->pattern, view->last_search, mode->bytes+1);
		search(view, mode->backward, 0);
		return;
	}

	/* Search is done */
	if (mode->bytes) {
		view->last_search = allocate(view->last_search, mode->bytes+1);
		memcpy(view->last_search, mode->pattern, mode->bytes);
		view->last_search[mode->bytes] = '\0';
	}
	view->mode = mode->previous;
	locus_destroy(view, mode->start_locus);
	allocate(mode->pattern, 0);
	allocate(mode, 0);
	if (ch == '\r' || ch == '_'-'@')
		locus_set(view, MARK, UNSET);
	else
		view->mode->command(view, ch);
}

void mode_search(struct view *view)
{
	struct mode_search *mode = allocate(NULL, sizeof *mode);

	memset(mode, 0, sizeof *mode);
	mode->previous = view->mode;
	mode->command = command_handler;
	mode->start_locus = locus_create(view, locus_get(view, CURSOR));
	view->mode = (struct mode *) mode;
}
