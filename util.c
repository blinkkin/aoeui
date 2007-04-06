#include "all.h"
#include <dirent.h>
#ifndef NAME_MAX
# define NAME_MAX 256
#endif

unsigned find_line_start(struct view *view, unsigned offset)
{
	int ch;
	while ((ch = view_byte(view, --offset)) >= 0)
		if (ch == '\n')
			break;
	return offset+1;
}

unsigned find_line_end(struct view *view, unsigned offset)
{
	int ch;
	for (; (ch = view_byte(view, offset)) >= 0; offset++)
		if (ch == '\n')
			break;
	return offset;
}

unsigned find_space(struct view *view, unsigned offset)
{
	int ch;
	for (; (ch = view_byte(view, offset)) >= 0; offset++)
		if (isspace(ch))
			break;
	return offset;
}


unsigned find_nonspace(struct view *view, unsigned offset)
{
	int ch;
	for (; (ch = view_byte(view, offset)) >= 0; offset++)
		if (!isspace(ch))
			break;
	return offset;
}

static int is_wordch(int ch)
{
	return ch > 0x100 || isalnum(ch);
}

unsigned find_word_start(struct view *view, unsigned offset)
{
	int ch;
	unsigned prev;
	while ((ch = view_byte(view, --offset)) >= 0)
		if (!isspace(ch)) {
			offset--;
			break;
		}
	for (; (ch = view_unicode_prior(view, offset, &prev)) >= 0;
	     offset = prev)
		if (!is_wordch(ch))
			return offset;
	return offset+1;
}

unsigned find_word_end(struct view *view, unsigned offset)
{
	int ch;
	unsigned chlen;
	offset = find_nonspace(view, offset)+1;
	for (; (ch = view_unicode(view, offset, &chlen)) >= 0; offset += chlen)
		if (!is_wordch(ch))
			break;
	return offset;
}

static int is_idch(int ch)
{
	return ch == '_' || is_wordch(ch);
}

unsigned find_id_start(struct view *view, unsigned offset)
{
	int ch;
	unsigned prev;
	while ((ch = view_byte(view, --offset)) >= 0)
		if (!isspace(ch)) {
			offset--;
			break;
		}
	for (; (ch = view_unicode_prior(view, offset, &prev)) >= 0;
	     offset = prev)
		if (!is_idch(ch))
			return offset;
	return offset+1;
}

unsigned find_id_end(struct view *view, unsigned offset)
{
	int ch;
	unsigned chlen;
	offset = find_nonspace(view, offset)+1;
	for (; (ch = view_unicode(view, offset, &chlen)) >= 0; offset += chlen)
		if (!is_idch(ch))
			break;
	return offset;
}

unsigned find_sentence_start(struct view *view, unsigned offset)
{
	int ch, next = view_byte(view, --offset);

	if (next < 0)
		return offset;
	while ((ch = view_byte(view, --offset)) >= 0)
		if (ch == '.' || ch == ',' || ch == ';' || ch == ':' ||
		    ch == '(' || ch == '[' || ch == '{' ||
		    ch == '\n' && ch == next)
			break;
		else
			next = ch;
	return offset+1;
}

unsigned find_sentence_end(struct view *view, unsigned offset)
{
	int ch, last = -1;

	for (offset++; (ch = view_byte(view, offset)) >= 0; offset++)
		if (ch == '.' || ch == ',' || ch == ';' ||
		    ch == '!' || ch == '?' ||
		    ch == ')' || ch == ']' || ch == '}' ||
		    ch == '\n' && ch == last)
			break;
		else
			last = ch;
	return offset;
}

static char *extract_id(struct view *view)
{
	unsigned at = locus_get(view, CURSOR);
	char *id;

	if (is_idch(view_unicode(view, at, NULL)))
		locus_set(view, CURSOR, at = find_id_end(view, at));
	locus_set(view, MARK, find_id_start(view, at));
	id = view_extract_selection(view);
	locus_set(view, MARK, UNSET);
	return id;
}

void find_tag(struct view *view)
{
	struct view *tags = view_find("TAGS");
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
	view = view_open(this);
	allocate(this, 0);
	for (at = 0; at < view->bytes; at = find_line_end(view, at) + 1)
		if (!--line)
			break;
	locus_set(view, CURSOR, at);
	locus_set(view, MARK, UNSET);
	window_raise(view);
	return;

done:	errno = 0;
	message("couldn't find tag %s", id);
	allocate(id, 0);
}

int view_unicode_slow(struct view *view, unsigned offset, unsigned *length)
{
	char *raw;
	unsigned len = view_raw(view, &raw, offset, 8);
	len = utf8_length(raw, len);
	if (length)
		*length = len;
	return utf8_unicode(raw, len);
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
			ch = view_unicode(view, offset, &at);
		}
	}
	if (prev)
		*prev = offset;
	return ch;
}

void view_erase(struct view *view)
{
	view_delete(view, 0, view->bytes);
}

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

int view_corresponding_bracket(struct view *view, unsigned offset)
{
	static signed char peer[0x100], updown[0x100];

	int ch = view_byte(view, offset);
	unsigned char stack[32];
	int stackptr = 0;
	int dir;

	if (!peer['(']) {
		peer['('] = ')', peer[')'] = '(';
		peer['['] = ']', peer[']'] = '[';
		peer['{'] = '}', peer['}'] = '{';
		updown['('] = updown['['] = updown['{'] = 1;
		updown[')'] = updown[']'] = updown['}'] = -1;
	}

	if (ch < 0 || !(dir = updown[ch])) {
		unsigned back, ahead;
		for (back = 1; back <= offset; back++) {
			ch = view_byte(view, offset-back);
			if (updown[ch] < 0)
				if (stackptr == sizeof stack)
					break;
				else
					stack[stackptr++] = ch;
			else if (updown[ch] > 0 &&
				 (!stackptr || ch != peer[stack[--stackptr]]))
				break;
		}
		for (ahead = 1; ; ahead++) {
			ch = view_byte(view, offset+ahead);
			if (ch < 0)
				break;
			if (updown[ch] > 0)
				if (stackptr == sizeof stack)
					break;
				else
					stack[stackptr++] = ch;
			else if (updown[ch] < 0 &&
				 (!stackptr || ch != peer[stack[--stackptr]]))
				break;
		}
		if (back <= ahead && back <= offset)
			return offset - back;
		else if (offset + ahead - 1 < view->bytes)
			return offset + ahead - 1;
		else
			return -1;
	}

	stack[stackptr++] = ch;
	while (stackptr) {
		offset += dir;
		ch = view_byte(view, offset);
		if (ch < 0)
			return -1;
		if (updown[ch] == dir) {
			if (stackptr == sizeof stack)
				return -1;
			stack[stackptr++] = ch;
		} else if (updown[ch] == -dir)
			if (ch != peer[stack[--stackptr]])
				break;
	}

	return offset;
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
