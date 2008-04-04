#include "all.h"
#include <dirent.h>
#ifndef NAME_MAX
# define NAME_MAX 256
#endif

static char *path_complete(const char *string)
{
	size_t length = strlen(string);
	char *new = allocate(length + NAME_MAX), *p;
	DIR *dir;

	memcpy(new, string, length+1);
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
		struct dirent *dent;
		size_t prefix_len = new + length - p;
		size_t best_len = 0;
		while ((dent = readdir(dir))) {
			size_t dent_len = strlen(dent->d_name);
			if (dent_len <= prefix_len)
				continue;
			if (strncmp(p, dent->d_name, prefix_len))
				continue;
			if (!best_len) {
				strcpy(new + length, dent->d_name + prefix_len);
				best_len = dent_len - prefix_len;
			} else {
				unsigned old_best_len = best_len;
				for (best_len = 0;
				     best_len < old_best_len;
				     best_len++)
					if (new[length+best_len] !=
					    dent->d_name[prefix_len+best_len])
						break;
				if (!best_len)
					break;
			}
		}
		new[length+best_len] = '\0';
		closedir(dir);
		if (best_len)
			return new;
	}
	RELEASE(new);
	return NULL;
}

static char *word_complete(const char *string)
{
	struct text *text;
	struct view *hit_view = NULL;
	size_t hit_offset = 0, hit_length = 0;
	size_t length = strlen(string);

	if (!length)
		return NULL;
	for (text = text_list; text; text = text->next) {
		struct view *view = text->views;
		sposition_t offset;
		if (!view)
			continue;
		for (offset = 0;
		     (offset = find_string(view, string, offset)) >= 0;
		     offset++) {
			size_t old_hit_length;
			position_t last = offset + length - 1;
			size_t this_length =
				find_word_end(view, last) - last - 1;
			if (!this_length)
				continue;
			if (!(old_hit_length = hit_length)) {
				hit_view = view;
				hit_offset = offset;
				hit_length = this_length;
			} else {
				for (hit_length = 0;
				     hit_length < old_hit_length;
				     hit_length++)
					if (view_byte(hit_view, hit_offset + length +
							hit_length) !=
					    view_byte(view, offset + length +
							hit_length))
						break;
				if (!hit_length)
					return NULL;
			}
		}
	}

	if (!hit_length)
		return NULL;
	return view_extract(hit_view, hit_offset, length + hit_length);
}

char *tab_complete(const char *string, Boolean_t selection)
{
	char *new = NULL;
	while (isspace(*string))
		string++;
	if (!*string)
		return NULL;
	if (selection)
		new = path_complete(string);
	if (!new)
		new = word_complete(string);
	if (new && !strcmp(new, string))
		RELEASE(new);	/* assigns new = NULL; */
	return new;
}

Boolean_t tab_completion_command(struct view *view)
{
	position_t cursor = locus_get(view, CURSOR);
	position_t mark = locus_get(view, MARK);
	char *completed = NULL, *select = NULL;
	Boolean_t selection = mark < cursor;
	Unicode_t ch;
	Boolean_t result = FALSE;

	if (selection)
		select = view_extract_selection(view);
	else if (mark == UNSET &&
		 (isalnum(ch = view_char_prior(view, cursor, NULL)) ||
		  ch == '_')) {
		mark = find_word_start(view, cursor);
		if (mark < cursor)
			select = view_extract(view, mark, cursor-mark);
	}
	if (select && (completed = tab_complete(select, selection))) {
		view_delete(view, mark, cursor-mark);
		view_insert(view, completed, locus_get(view, CURSOR), -1);
		if (!selection)
			mark = cursor;
		locus_set(view, MARK, mark);
		RELEASE(completed);
		result = TRUE;
	}
	RELEASE(select);
	return result;
}

void insert_tab(struct view *view)
{
	position_t cursor = locus_get(view, CURSOR);
	position_t mark = locus_get(view, MARK);

	if (mark != UNSET && mark > cursor) {
		view_delete(view, cursor, mark - cursor);
		mark = UNSET;
	}
	if (no_tabs) {
		int tabstop = view->text->tabstop;
		sposition_t offset = 0;
		position_t at = find_line_start(view, cursor);
		if (at)
			while (at != cursor) {
				Unicode_t ch = view_char(view, at, &at);
				if (ch == '\t')
					offset = (offset / tabstop + 1) * tabstop;
				else
					offset++;
			}
		for (offset %= tabstop; offset++ < tabstop; )
			view_insert(view, " ", cursor, 1);
	} else
		view_insert(view, "\t", cursor, 1);
	if (mark == cursor)
		locus_set(view, MARK, /*old*/ cursor);
}

void align(struct view *view)
{
	position_t cursor = locus_get(view, CURSOR);
	position_t lnstart0 = find_line_start(view, cursor);
	position_t nonspace0, lnstart, offset, next;
	Unicode_t ch, last = UNICODE_BAD;
	char *indentation;
	unsigned indent = 0, spaces = 0, indent_bytes;
	unsigned tabstop = view->text->tabstop;
	unsigned stack[16], stackptr = 0;

	if (!lnstart0)
		return;
	tabstop |= !tabstop;

	for (nonspace0 = lnstart0;
	     IS_UNICODE(ch = view_char(view, nonspace0, &next));
	     nonspace0 = next)
		if (ch == '\n' || ch != ' ' && ch != '\t')
			break;
	lnstart = find_line_start(view, lnstart0-1);
	while (lnstart && view_char(view, lnstart, NULL) == '\n')
		lnstart = find_line_start(view, lnstart-1);

	for (offset = lnstart;
	     IS_UNICODE(ch = view_char(view, offset, &offset)); )
		if (ch == ' ')
			spaces++;
		else if (ch == '\t') {
			indent = ((indent + spaces) / tabstop + 1) * tabstop;
			spaces = 0;
		} else
			break;
	stack[stackptr++] = indent;
	stack[stackptr++] = indent += spaces;

	for (; IS_UNICODE(ch); ch = view_char(view, offset, &offset)) {
		if (ch == '\n')
			break;
		indent++;
		if (!isspace(ch))
			last = ch;
		if (ch == '(' || ch == '[') {
			if ((unsigned) stackptr < 16)
				stack[stackptr] = indent;
			stackptr++;
		} else if (ch == ')' || ch == ']')
			stackptr--;
		else if (ch == '\t')
			indent = ((indent-1) / tabstop + 1) * tabstop;
	}

	if (stackptr <= 0 || stackptr > 16)
		indent = stack[0];
	else {
		indent = stack[stackptr-1];
		if (stackptr <= 2 && (last == '{' || last == ')'))
			indent += tabstop;
	}

	if (no_tabs) {
		indent_bytes = indent;
		indentation = allocate(indent_bytes);
		memset(indentation, ' ', indent);
	} else {
		indent_bytes = indent / tabstop + indent % tabstop;
		indentation = allocate(indent_bytes);
		memset(indentation, '\t', indent / tabstop);
		memset(indentation + indent / tabstop, ' ', indent % tabstop);
	}

	view_delete(view, lnstart0, nonspace0 - lnstart0);
	view_insert(view, indentation, lnstart0, indent_bytes);
	RELEASE(indentation);
}
