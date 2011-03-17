/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#include "all.h"

ssize_t view_vprintf(struct view *view, const char *msg, va_list ap)
{
	char buff[1024];
	vsnprintf(buff, sizeof buff, msg, ap);
	return view_insert(view, buff, view->bytes, -1);
}

ssize_t view_printf(struct view *view, const char *msg, ...)
{
	va_list ap;
	int result;

	va_start(ap, msg);
	result = view_vprintf(view, msg, ap);
	va_end(ap);
	return result;
}

size_t view_get_selection(struct view *view, position_t *offset,
			  Boolean_t *append)
{
	position_t cursor = locus_get(view, CURSOR);
	position_t mark = locus_get(view, MARK);

	if (mark == UNSET)
		if ((mark = cursor) < view->bytes)
			view_char(view, mark, &mark);
	if (append)
		*append = cursor >= mark;
	if (mark <= cursor)
		return cursor - (*offset = mark);
	return mark - (*offset = cursor);
}

char *view_extract(struct view *view, position_t offset, unsigned bytes)
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
	str = allocate(bytes+1);
	str[view_get(view, str, offset, bytes)] = '\0';
	return str;
}

char *view_extract_selection(struct view *view)
{
	position_t offset;
	size_t bytes = view_get_selection(view, &offset, NULL);
	return view_extract(view, offset, bytes);
}

size_t view_delete_selection(struct view *view)
{
	position_t offset;
	size_t bytes = view_get_selection(view, &offset, NULL);
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

Unicode_t view_unicode(struct view *view, position_t offset, position_t *next)
{
	Unicode_t ch = view_byte(view, offset);
	char *raw;
	size_t length;

	if (!IS_UNICODE(ch) ||
	    ch < 0x80 ||
	    view->text->flags & TEXT_NO_UTF8) {
		if (ch == '\r' &&
		    view->text->flags & TEXT_CRNL &&
		    view_byte(view, offset + 1) == '\n') {
			if (next)
				*next = offset + 2;
			return '\n';
		}
		if (next)
			*next = offset + IS_UNICODE(ch);
		return ch;
	}

	length = view_raw(view, &raw, offset, 8);
	length = utf8_length(raw, length);
	if (next)
		*next = offset + length;
	return utf8_unicode(raw, length);
}

Unicode_t view_unicode_prior(struct view *view, position_t offset,
			     position_t *prev)
{
	Unicode_t ch = UNICODE_BAD;
	char *raw;

	if (offset) {
		ch = view_byte(view, --offset);
		if (IS_UNICODE(ch) &&
		    ch >= 0x80 &&
		    !(view->text->flags & TEXT_NO_UTF8)) {
			unsigned at = offset >= 7 ? offset-7 : 0;
			view_raw(view, &raw, at, offset-at+1);
			offset -= utf8_length_backwards(raw+offset-at,
					offset-at+1) - 1;
			ch = view_unicode(view, offset, NULL);
		} else if (ch == '\n' &&
			   view->text->flags & TEXT_CRNL &&
			   offset &&
			   view_byte(view, offset-1) == '\r')
			offset--;
	}
	if (prev)
		*prev = offset;
	return ch;
}

Unicode_t view_char(struct view *view, position_t offset, position_t *next)
{
	position_t next0;
	Unicode_t ch = view_unicode(view, offset, &next0);

	if (!next)
		return ch;
	*next = next0;
	if (IS_FOLDED(ch)) {
		size_t fbytes = FOLDED_BYTES(ch);
		position_t next2;
		if (view_unicode(view, next0 + fbytes, &next2) ==
		    FOLD_END + fbytes)
			*next = next2;
	}
	return ch;
}

Unicode_t view_char_prior(struct view *view, position_t offset,
			  position_t *prev)
{
	Unicode_t ch = view_unicode_prior(view, offset, &offset), ch0;
	size_t fbytes;
	position_t offset0;

	if (ch >= FOLD_END &&
	    (fbytes = FOLDED_BYTES(ch)) <= offset &&
	    IS_FOLDED(ch0 = view_unicode_prior(view, offset - fbytes,
					       &offset0)) &&
	    FOLDED_BYTES(ch0) == fbytes) {
		ch = ch0;
		offset = offset0;
	}
	if (prev)
		*prev = offset;
	return ch;
}

Boolean_t is_open_bracket(const char *brackets, Unicode_t ch)
{
	if (ch >= 0x80)
		return FALSE;
	for (; *brackets; brackets += 2)
		if (*brackets == ch)
			return TRUE;
	return FALSE;
}

Boolean_t is_close_bracket(const char *brackets, Unicode_t ch)
{
	if (ch >= 0x80)
		return FALSE;
	for (brackets++; *brackets; brackets += 2)
		if (*brackets == ch)
			return TRUE;
	return FALSE;
}
