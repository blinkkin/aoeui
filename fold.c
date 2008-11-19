/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#include "all.h"

void view_fold(struct view *view, position_t cursor, position_t mark)
{
	size_t bytes = mark - cursor;
	char buf[8];

	if (mark < cursor)
		bytes = -bytes, cursor = mark;
	if (cursor > view->bytes)
		return;
	if (cursor + bytes > view->bytes)
		bytes = view->bytes - cursor;
	if (!bytes)
		return;
	view_insert(view, buf, cursor + bytes, unicode_utf8(buf, FOLD_END+bytes));
	view_insert(view, buf, cursor, unicode_utf8(buf, FOLD_START+bytes));
	view->text->foldings++;
}

sposition_t view_unfold(struct view *view, position_t offset)
{
	position_t next, next2;
	size_t fbytes;
	Unicode_t ch = view_unicode(view, offset, &next);

	if (ch < FOLD_START || ch >= FOLD_END)
		return -1;
	fbytes = FOLDED_BYTES(ch);
	if (view_unicode(view, next + fbytes, &next2) !=
	    FOLD_END + fbytes)
		return -1;
	view_delete(view, next + fbytes, next2 - (next + fbytes));
	view_delete(view, offset, next - offset);
	view->text->foldings--;
	return offset + fbytes;
}

void view_unfold_selection(struct view *view)
{
	position_t offset = locus_get(view, CURSOR);
	position_t end = locus_get(view, MARK);

	if (end == UNSET)
		return;
	if (end < offset) {
		position_t t = end;
		end = offset;
		offset = t;
	}

	while (offset < end) {
		position_t next, next2;
		size_t fbytes;
		Unicode_t ch = view_unicode(view, offset, &next);
		if (!IS_UNICODE(ch))
			break;
		if (ch < FOLD_START || ch >= FOLD_END) {
			offset = next;
			continue;
		}
		fbytes = FOLDED_BYTES(ch);
		if (view_unicode(view, next + fbytes, &next2) !=
		    FOLD_END + fbytes) {
			offset = next;
			continue;
		}
		view_delete(view, next + fbytes, next2 - (next + fbytes));
		view_delete(view, offset, next - offset);
		view->text->foldings--;
		offset += fbytes;
	}
}

static int indentation(struct view *view, position_t offset)
{
	unsigned indent = 0, tabstop = view->text->tabstop;
	Unicode_t ch;
	tabstop |= !tabstop;
	for (;;)
		if ((ch = view_char(view, offset, &offset)) == ' ')
			indent++;
		else if (ch == '\t')
			indent = (indent / tabstop + 1) * tabstop;
		else if (ch == '\n')
			return -1;
		else
			break;
	return indent;
}

static unsigned max_indentation(struct view *view)
{
	position_t offset, next;
	int maxindent = 0;

	for (offset = 0; offset < view->bytes; offset = next) {
		int indent = indentation(view, offset);
		if (indent > maxindent)
			maxindent = indent;
		next = find_line_end(view, offset) + 1;
	}
	return maxindent;
}

void view_fold_indented(struct view *view, unsigned minindent)
{
	unsigned maxindent;

	minindent |= !minindent;
	while ((maxindent = max_indentation(view)) >= minindent) {
		position_t offset, next;
		sposition_t start = -1;
		for (offset = 0; offset < view->bytes; offset = next) {
			next = find_line_end(view, offset) + 1;
			if (indentation(view, offset) < maxindent) {
				if (start >= 0) {
					view_fold(view, next = start, offset-1);
					start = -1;
				}
			} else if (start < 0)
				start = offset - !!offset;
		}
		if (start >= 0)
			view_fold(view, start, offset-1);
	}
}

void view_unfold_all(struct view *view)
{
	position_t offset, next;
	if (!view->text->foldings)
		return;
	for (offset = 0;
	     IS_UNICODE(view_unicode(view, offset, &next));
	     offset = next)
		if (view_unfold(view, offset) >= 0) {
			if (!view->text->foldings)
				break;
			next = offset;
		}
}

void text_unfold_all(struct text *text)
{
	struct view *view;
	if (!text->foldings)
		return;
	view_unfold_all(view = view_create(text));
	view_close(view);
}
