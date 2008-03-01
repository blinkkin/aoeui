#include "all.h"
#include <dirent.h>
#ifndef NAME_MAX
# define NAME_MAX 256
#endif

static char *path_complete(const char *string)
{
	unsigned length = strlen(string);
	char *new = allocate(NULL, length + NAME_MAX), *p;
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
		unsigned prefix_len = new + length - p;
		unsigned best_len = 0;
		while ((dent = readdir(dir))) {
			unsigned dent_len = strlen(dent->d_name);
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
	allocate(new, 0);
	return NULL;
}

static char *word_complete(const char *string)
{
	struct text *text;
	struct view *hit_view = NULL;
	unsigned hit_offset = 0, hit_length = 0;
	unsigned length = strlen(string);

	if (!length)
		return NULL;
	for (text = text_list; text; text = text->next) {
		struct view *view = text->views;
		int offset;
		if (!view)
			continue;
		for (offset = 0;
		     (offset = find_string(view, string, offset)) >= 0;
		     offset++) {
			unsigned old_hit_length;
			unsigned last = offset + length - 1;
			unsigned this_length = find_word_end(view, last) - last - 1;
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

char *tab_complete(const char *string, int selection)
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
	if (new && !strcmp(new, string)) {
		allocate(new, 0);
		new = NULL;
	}
	return new;
}

int tab_completion_command(struct view *view)
{
	unsigned cursor = locus_get(view, CURSOR);
	unsigned mark = locus_get(view, MARK);
	char *completed = NULL, *select = NULL;
	int selection, ch;

	if ((selection = mark < cursor))
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
		allocate(completed, 0);
	}
	allocate(select, 0);
	return !!completed;
}

void insert_tab(struct view *view)
{
	unsigned cursor = locus_get(view, CURSOR);
	unsigned mark = locus_get(view, MARK);

	if (mark != UNSET && mark > cursor) {
		view_delete(view, cursor, mark - cursor);
		mark = UNSET;
	}
	if (no_tabs) {
		int tabstop = view->text->tabstop;
		int offset = 0;
		unsigned at = find_line_start(view, cursor);
		if (at)
			while (at != cursor) {
				int ch = view_char(view, at, &at);
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
	unsigned cursor = locus_get(view, CURSOR);
	unsigned lnstart0 = find_line_start(view, cursor);
	unsigned nonspace0, lnstart, offset, next;
	int ch, last = -1;
	char *indentation;
	unsigned indent = 0, spaces = 0, indent_bytes, tabstop = view->text->tabstop;
	unsigned stack[16], stackptr = 0;

	if (!lnstart0)
		return;
	tabstop |= !tabstop;

	for (nonspace0 = lnstart0;
	     (ch = view_char(view, nonspace0, &next)) >= 0;
	     nonspace0 = next)
		if (ch == '\n' || ch != ' ' && ch != '\t')
			break;
	lnstart = find_line_start(view, lnstart0-1);
	while (lnstart && view_char(view, lnstart, NULL) == '\n')
		lnstart = find_line_start(view, lnstart-1);

	for (offset = lnstart; (ch = view_char(view, offset, &offset)) >= 0; )
		if (ch == ' ')
			spaces++;
		else if (ch == '\t') {
			indent = ((indent + spaces) / tabstop + 1) * tabstop;
			spaces = 0;
		} else
			break;
	stack[stackptr++] = indent;
	stack[stackptr++] = indent += spaces;

	for (; ch >= 0; ch = view_char(view, offset, &offset)) {
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

	indent_bytes = indent / tabstop + indent % tabstop;
	indentation = allocate(NULL, indent_bytes);
	if (no_tabs)
		memset(indentation, ' ', indent);
	else {
		memset(indentation, '\t', indent / tabstop);
		memset(indentation + indent / tabstop, ' ', indent % tabstop);
	}

	view_delete(view, lnstart0, nonspace0 - lnstart0);
	view_insert(view, indentation, lnstart0, indent_bytes);
	allocate(indentation, 0);
}
