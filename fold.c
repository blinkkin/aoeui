#include "all.h"

void view_fold(struct view *view, unsigned cursor, unsigned mark)
{
	unsigned bytes = mark - cursor;
	char buf[8];
	if (mark < cursor)
		bytes = -bytes, cursor = mark;
	if (cursor > view->bytes)
		return;
	if (cursor + bytes > view->bytes)
		bytes = view->bytes - cursor;
	if (!bytes)
		return;
	view_insert(view, buf, cursor + bytes, utf8_out(buf, FOLD_END+bytes));
	view_insert(view, buf, cursor, utf8_out(buf, FOLD_START+bytes));
	view->text->flags |= TEXT_FOLDED;
}

int view_unfold(struct view *view, unsigned offset)
{
	unsigned next, fbytes, next2;
	int ch = view_unicode(view, offset, &next);
	if (ch < FOLD_START || ch >= FOLD_END)
		return -1;
	fbytes = FOLDED_BYTES(ch);
	if (view_unicode(view, next + fbytes, &next2) !=
	    FOLD_END + fbytes)
		return -1;
	view_delete(view, next + fbytes, next2 - (next + fbytes));
	view_delete(view, offset, next - offset);
	return offset + fbytes;
}

void view_unfold_selection(struct view *view)
{
	unsigned offset = locus_get(view, CURSOR);
	unsigned end = locus_get(view, MARK);

	if (end == UNSET)
		return;
	if (end < offset) {
		unsigned t = end;
		end = offset;
		offset = t;
	}

	while (offset < end) {
		unsigned next, next2, fbytes;
		int ch = view_unicode(view, offset, &next);
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
		offset += fbytes;
	}
}

static unsigned indentation(struct view *view, unsigned offset)
{
	unsigned indent = 0, tabstop = view->text->tabstop;
	int ch;
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
	unsigned offset, maxindent = 0, next;
	for (offset = 0; offset < view->bytes; offset = next) {
		unsigned indent = indentation(view, offset);
		if ((signed) indent > 0 && indent > maxindent)
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
		unsigned offset, next;
		int start = -1;
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
	unsigned offset, next;
	for (offset = 0; view_unicode(view, offset, &next) >= 0; offset = next)
		if (view_unfold(view, offset) >= 0)
			next = offset;
}

void text_unfold_all(struct text *text)
{
	struct view *view;
	if (!(text->flags & TEXT_FOLDED))
		return;
	for (view = text->views; view; view = view->next)
		view_unfold_all(view);
	text->flags &= ~TEXT_FOLDED;
}
