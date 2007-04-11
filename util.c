#include "all.h"
#include <dirent.h>
#ifndef NAME_MAX
# define NAME_MAX 256
#endif

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

char *tab_complete(const char *path)
{
	unsigned len = strlen(path);
	char *new = allocate(NULL, len + NAME_MAX);
	char *p;
	DIR *dir;
	struct dirent *dent, *best_dent = NULL;

	memcpy(new, path, len+1);
	p = strrchr(new, '/');
	if (p) {
		*p = '\0';
		dir = opendir(new);
		*p++ = '/';
	} else {
		dir = opendir(".");
		p = new;
	}
	if (dir) {
		while ((dent = readdir(dir)))
			if (!strncmp(p, dent->d_name, new + len - p) &&
			    (!best_dent ||
			     strcmp(dent->d_name, best_dent->d_name) < 0))
				best_dent = dent;
		if (best_dent)
			strcpy(p, best_dent->d_name);
		closedir(dir);
	}

	if (!strcmp(new, path)) {
		allocate(new, 0);
		new = NULL;
	}
	return new;
}

static int unicode(struct view *view, unsigned offset, unsigned *next)
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

static int unicode_prior(struct view *view, unsigned offset, unsigned *prev)
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
			ch = unicode(view, offset, NULL);
		}
	}
	if (prev)
		*prev = offset;
	return ch;
}

int view_char(struct view *view, unsigned offset, unsigned *next)
{
	unsigned next0;
	int ch = unicode(view, offset, &next0);
	if (!next)
		return ch;
	*next = next0;
	if (ch >= FOLD_START && ch < FOLD_END) {
		unsigned fbytes = FOLDED_BYTES(ch), next2;
		if (unicode(view, next0 + fbytes, &next2) ==
		    FOLD_END + fbytes)
			*next = next2;
	}
	return ch;
}

int view_char_prior(struct view *view, unsigned offset, unsigned *prev)
{
	int ch = unicode_prior(view, offset, &offset), ch0;
	unsigned fbytes, offset0;

	if (ch >= FOLD_END &&
	    (fbytes = FOLDED_BYTES(ch)) <= offset &&
	    (ch0 = unicode_prior(view, offset - fbytes,
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
}

int view_unfold(struct view *view, unsigned offset)
{
	unsigned next, fbytes, next2;
	int ch = unicode(view, offset, &next);
	if (ch < FOLD_START || ch >= FOLD_END)
		return -1;
	fbytes = FOLDED_BYTES(ch);
	if (unicode(view, next + fbytes, &next2) !=
	    FOLD_END + fbytes)
		return -1;
	view_delete(view, next + fbytes, next2 - (next + fbytes));
	view_delete(view, offset, next - offset);
	return offset + fbytes;
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
	for (offset = 0; unicode(view, offset, &next) >= 0; offset = next)
		if (view_unfold(view, offset) >= 0)
			next = offset;
}
