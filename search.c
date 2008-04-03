#include "all.h"
#include <regex.h>

/*
 *	Incremental search mode
 */

struct mode_search {
	command command;
	struct mode *previous;
	unsigned char *pattern;
	unsigned bytes, alloc, last_bytes;
	int backward;
	unsigned start, mark;
	regex_t *regex;
	int regex_ready;
};

static int match_char(int x, int y)
{
	if (x < 0 || y < 0)
		return 0;
	if (x >= 'a' && x <= 'z')
		x += 'A' - 'a';
	if (y >= 'a' && y <= 'z')
		y += 'A' - 'a';
	return x == y;
}

static int match_pattern(struct view *view, unsigned offset)
{
	struct mode_search *mode = (struct mode_search *) view->mode;
	unsigned j;
	for (j = 0; j < mode->bytes; j++)
		if (!match_char(mode->pattern[j],
				view_byte(view, offset++)))
			return 0;
	return mode->bytes;
}

static int match_regex(struct view *view, unsigned *offset, int advance)
{
	int j, err;
	char *raw;
	unsigned bytes = view_raw(view, &raw, *offset, ~0);
	unsigned flags = 0;
	regmatch_t match[10];
	struct mode_search *mode = (struct mode_search *) view->mode;

	if (view_char_prior(view, *offset, NULL) != '\n')
		flags |= REG_NOTBOL;
	err = regexec(mode->regex, raw, 10, match, flags);
	if (err && err != REG_NOMATCH)
		window_beep(view);
	if (err)
		return 0;
	if (!advance && match[0].rm_so)
		return 0;
	if (match[0].rm_so >= bytes)
		return 0;
	if (match[0].rm_eo > bytes)
		match[0].rm_eo = bytes;
	for (j = 1; j < 10; j++) {
		if (match[j].rm_so < 0 ||
		    match[j].rm_so >= bytes)
			continue;
		if (match[j].rm_eo > bytes)
			match[j].rm_eo = bytes;
		clip_init(j);
		clip(j, view, *offset + match[j].rm_so,
		     match[j].rm_eo - match[j].rm_so, 0);
	}
	*offset += match[0].rm_so;
	return match[0].rm_eo - match[0].rm_so;
}

static int scan_forward(struct view *view, unsigned *length,
			unsigned offset, unsigned max_offset)
{
	struct mode_search *mode = (struct mode_search *) view->mode;

	if (offset + mode->bytes > view->bytes)
		return -1;
	if (max_offset + mode->bytes > view->bytes)
		max_offset = view->bytes - mode->bytes;
	if (mode->regex) {
		if ((*length = match_regex(view, &offset, 1)) &&
		    offset < max_offset)
			return offset;
	} else
		for (; offset < max_offset; offset++)
			if ((*length = match_pattern(view, offset)))
				return offset;
	return -1;
}

static int scan_backward(struct view *view, unsigned *length,
			 unsigned offset, unsigned min_offset)
{
	struct mode_search *mode = (struct mode_search *) view->mode;

	if (min_offset + mode->bytes > view->bytes)
		return -1;
	if (offset + mode->bytes > view->bytes)
		offset = view->bytes - mode->bytes;
	if (mode->regex) {
		for (; offset+1 > min_offset; offset--)
			if ((*length = match_regex(view, &offset, 0)))
				return offset;
	} else
		for (; offset+1 > min_offset; offset--)
			if ((*length = match_pattern(view, offset)))
				return offset;
	return -1;
}

static int search(struct view *view, int backward, int new)
{
	struct mode_search *mode = (struct mode_search *) view->mode;
	unsigned cursor, length;
	int at;

	if (!mode->bytes) {
		locus_set(view, CURSOR, mode->start);
		locus_set(view, MARK, UNSET);
		return 1;
	}

	if (mode->regex) {
		int err;
		if (new && mode->regex_ready) {
			regfree(mode->regex);
			mode->regex_ready = 0;
		}
		if (!mode->regex_ready) {
			mode->pattern[mode->bytes] = '\0';
			err = regcomp(mode->regex, (char *) mode->pattern,
				      REG_EXTENDED | REG_ICASE | REG_NEWLINE);
			if (err)
				return 0;
			mode->regex_ready = 1;
		}
	}

	cursor = locus_get(view, CURSOR);

	if (backward) {
		at = scan_backward(view, &length, cursor - !new, 0);
		if (at < 0)
			at = scan_backward(view, &length,
					   view->bytes, cursor+1);
	} else {
		at = scan_forward(view, &length,
				  cursor + !new, view->bytes);
		if (at < 0)
			at = scan_forward(view, &length, 0, cursor-1);
	}
	if (at < 0) {
		window_beep(view);
		macros_abort();
		return 0;
	}

	/* A hit! */
	locus_set(view, CURSOR, at);
	locus_set(view, MARK, at + length);
	mode->last_bytes = mode->bytes;
	return 1;
}

static void command_handler(struct view *view, unsigned ch)
{
	struct mode_search *mode = (struct mode_search *) view->mode;
	static char cmdchar[][2] = {
		{ CONTROL('H'), CONTROL('G') },
		{ CONTROL('T'), CONTROL('H') },
		{ CONTROL('V'), CONTROL('U') }
	};

	/* Backspace removes the last character from the search target and
	 * returns to the previous hit
	 */
	if (ch == 0x7f /*BCK*/)
		if (!mode->bytes) {
			window_beep(view);
			goto done;
		} else {
			mode->bytes--;
			search(view, !mode->backward, 1);
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
		mode->pattern[mode->bytes] = '\0';
		if (!search(view, mode->backward, new) &&
		    !mode->regex)
			mode->bytes -= new;
		return;
	}

	/* ^H moves to hit that is earlier in the text.
	 * ^T and ^/ (^A, ^_) proceed to a later hit.
	 */
	if (mode->last_bytes &&
	    (ch == cmdchar[0][is_asdfg] ||
	     ch == cmdchar[1][is_asdfg] ||
	     ch == CONTROL('A') ||
	     ch == CONTROL('_'))) {
		mode->bytes = mode->last_bytes;
		search(view, mode->backward = (ch == cmdchar[is_asdfg][0]), 0);
		return;
	}

	/* Hitting ^T or ^/ (^A, ^_) with an empty search pattern causes
	 * the last successful search target to be reused.
	 */
	if ((ch == cmdchar[1][is_asdfg] ||
	     ch == CONTROL('A') ||
	     ch == CONTROL('_')) &&
	    !mode->bytes && view->last_search) {
		mode->bytes = strlen(view->last_search);
		mode->alloc = mode->bytes + 8;
		mode->pattern = allocate(mode->pattern, mode->alloc);
		memcpy(mode->pattern, view->last_search, mode->bytes+1);
		search(view, mode->backward, 0);
		return;
	}

	/* Search is done */
done:	if (mode->bytes) {
		view->last_search = allocate(view->last_search, mode->bytes+1);
		memcpy(view->last_search, mode->pattern, mode->bytes);
		view->last_search[mode->bytes] = '\0';
	}

	view->mode = mode->previous;

	if (ch == '\r' || ch == CONTROL('A') || ch == CONTROL('_'))
		;
	else if (ch == cmdchar[2][is_asdfg]) /* unset mark */
		locus_set(view, MARK, mode->mark);
	else if (ch != 0x7f /*BCK*/)
		view->mode->command(view, ch);

	/* Release search mode resources */
	allocate(mode->pattern, 0);
	if (mode->regex_ready)
		regfree(mode->regex);
	allocate(mode->regex, 0);
	allocate(mode, 0);
}

void mode_search(struct view *view, int regex)
{
	struct mode_search *mode = allocate0(sizeof *mode);
	mode->previous = view->mode;
	mode->command = command_handler;
	mode->start = locus_get(view, CURSOR);
	mode->mark = locus_get(view, MARK);
	if (regex)
		mode->regex = allocate0(sizeof *mode->regex);
	view->mode = (struct mode *) mode;
}
