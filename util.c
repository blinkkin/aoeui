#include "all.h"

int view_vprintf(struct view *view, const char *msg, va_list ap)
{
	char buff[1024];
	vsnprintf(buff, sizeof buff, msg, ap);
	return view_insert(view, buff, view->bytes, -1);
}

int view_printf(struct view *view, const char *msg, ...)
{
	va_list ap;
	int result;

	va_start(ap, msg);
	result = view_vprintf(view, msg, ap);
	va_end(ap);
	return result;
}

unsigned view_get_selection(struct view *view, unsigned *offset, int *append)
{
	unsigned cursor = locus_get(view, CURSOR);
	unsigned mark = locus_get(view, MARK);
	if (mark == UNSET)
		mark = cursor + (cursor < view->bytes);
	if (append)
		*append = cursor >= mark;
	if (mark <= cursor)
		return cursor - (*offset = mark);
	return mark - (*offset = cursor);
}

char *view_extract(struct view *view, unsigned offset, unsigned bytes)
{
	char *str;

	if (!view)
		return NULL;
	if (offset > view->bytes)
		return NULL;
	if (offset + bytes > view->bytes)
		bytes = view->bytes - offset;
	if (!bytes)
		return NULL;
	str = allocate(NULL, bytes+1);
	str[view_get(view, str, offset, bytes)] = '\0';
	return str;
}

char *view_extract_selection(struct view *view)
{
	unsigned offset;
	unsigned bytes = view_get_selection(view, &offset, NULL);
	return view_extract(view, offset, bytes);
}

unsigned view_delete_selection(struct view *view)
{
	unsigned offset;
	unsigned bytes = view_get_selection(view, &offset, NULL);
	view_delete(view, offset, bytes);
	locus_set(view, MARK, UNSET);
	return bytes;
}

struct view *view_next(struct view *view)
{
	struct view *new = view;
	do {
		if (new->next)
			new = new->next;
		else if (new->text->next)
			new = new->text->next->views;
		else
			new = text_list->views;
	} while (new != view && new->window);
	return new == view ? text_new() : new;
}

int view_unicode(struct view *view, unsigned offset, unsigned *next)
{
	int ch = view_byte(view, offset);
	char *raw;
	unsigned length;

	if (ch < 0x80) {
		if (next)
			*next = offset + (ch >= 0);
		return ch;
	}

	length = view_raw(view, &raw, offset, 8);
	length = utf8_length(raw, length);
	if (next)
		*next = offset + length;
	return utf8_unicode(raw, length);
}

int view_unicode_prior(struct view *view, unsigned offset, unsigned *prev)
{
	int ch = -1;
	char *raw;

	if (offset) {
		ch = view_byte(view, --offset);
		if (ch >= 0x80) {
			unsigned at = offset >= 7 ? offset-7 : 0;
			view_raw(view, &raw, at, offset-at+1);
			offset -= utf8_length_backwards(raw+offset-at,
					offset-at+1) - 1;
			ch = view_unicode(view, offset, NULL);
		}
	}
	if (prev)
		*prev = offset;
	return ch;
}

int view_char(struct view *view, unsigned offset, unsigned *next)
{
	unsigned next0;
	int ch = view_unicode(view, offset, &next0);
	if (!next)
		return ch;
	*next = next0;
	if (ch >= FOLD_START && ch < FOLD_END) {
		unsigned fbytes = FOLDED_BYTES(ch), next2;
		if (view_unicode(view, next0 + fbytes, &next2) ==
		    FOLD_END + fbytes)
			*next = next2;
	}
	return ch;
}

int view_char_prior(struct view *view, unsigned offset, unsigned *prev)
{
	int ch = view_unicode_prior(view, offset, &offset), ch0;
	unsigned fbytes, offset0;

	if (ch >= FOLD_END &&
	    (fbytes = FOLDED_BYTES(ch)) <= offset &&
	    (ch0 = view_unicode_prior(view, offset - fbytes,
				      &offset0)) >= FOLD_START &&
	    ch0 < FOLD_END &&
	    FOLDED_BYTES(ch0) == fbytes) {
		ch = ch0;
		offset = offset0;
	}
	if (prev)
		*prev = offset;
	return ch;
}
