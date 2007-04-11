#include "all.h"

/* TAGS file searching */

static char *extract_id(struct view *view)
{
	unsigned at = locus_get(view, CURSOR);
	char *id;

	if (is_idch(view_char(view, at, NULL)))
		locus_set(view, CURSOR, at = find_id_end(view, at));
	locus_set(view, MARK, find_id_start(view, at));
	id = view_extract_selection(view);
	locus_set(view, MARK, UNSET);
	return id;
}

void find_tag(struct view *view)
{
	struct view *tags = view_find("TAGS"), *new_view;
	char *id, *this;
	unsigned first, last, at, wordstart, wordend, line;
	int cmp;

	if (!tags) {
		tags = view_open("TAGS");
		if (tags->text->flags & TEXT_CREATED) {
			message("Please load a TAGS file.");
			view_close(tags);
			return;
		}
	}
	if (locus_get(view, MARK) != UNSET) {
		id = view_extract_selection(view);
		view_delete_selection(view);
	} else
		id = extract_id(view);
	if (!id) {
		window_beep(view);
		return;
	}

	first = 0;
	last = tags->bytes;
	while (first < last) {
		at = find_line_start(tags, first + last >> 1);
		if (at < first)
			at = find_line_end(tags, at) + 1;
		if (at >= last)
			goto done;
		wordstart = find_nonspace(tags, at);
		wordend = find_space(tags, wordstart);
		this = view_extract(tags, at, wordend - wordstart);
		if (!this)
			goto done;
		cmp = strcmp(id, this);
		allocate(this, 0);
		if (!cmp)
			break;
		if (cmp < 0)
			last = at;
		else
			first = find_line_end(tags, at) + 1;
	}
	if (first >= last)
		goto done;

	allocate(id, 0);
	wordstart = find_nonspace(tags, wordend); /* line number */
	wordend = find_space(tags, wordstart);
	this = view_extract(tags, wordstart, wordend - wordstart);
	line = atoi(this);
	allocate(this, 0);
	wordstart = find_nonspace(tags, wordend); /* file name */
	wordend = find_space(tags, wordstart);
	this = view_extract(tags, wordstart, wordend - wordstart);
	new_view = view_open(this);
	allocate(this, 0);
	locus_set(new_view, CURSOR, find_line_number(new_view, line));
	locus_set(new_view, MARK, UNSET);
	window_below(view, new_view, 4);
	return;

done:	errno = 0;
	message("couldn't find tag %s", id);
	allocate(id, 0);
}
