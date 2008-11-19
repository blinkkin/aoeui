/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#include "all.h"
#include <dirent.h>
#ifndef NAME_MAX
# define NAME_MAX 256
#endif

static char *path_complete(const char *string)
{
	const char *home;
	size_t length;
	char *new = NULL, *p;
	DIR *dir;
	char *freestring = NULL;

	while (isspace(*string))
		string++;
	if (!strncmp(string, "~/", 2) &&
	    (home = getenv("HOME"))) {
		freestring = allocate(strlen(home) + strlen(string));
		sprintf(freestring, "%s%s", home, string+1);
		string = freestring;
	}

	length = strlen(string);
	new = allocate(length + NAME_MAX);
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
			goto done;
	}

	RELEASE(new);		/* sets new = NULL */
done:	RELEASE(freestring);
	return new;
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
					if (view_byte(hit_view,
						      hit_offset + length +
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
		 IS_CODEPOINT((ch = view_char_prior(view, cursor, NULL))) &&
		 (isalnum(ch) || ch == '_')) {
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
	if (view->text->flags & TEXT_NO_TABS) {
		int tabstop = view->text->tabstop;
		sposition_t offset = 0;
		position_t at = find_line_start(view, cursor);
		if (at)
			while (at != cursor) {
				Unicode_t ch = view_char(view, at, &at);
				if (ch == '\t')
					offset = (offset / tabstop + 1) *
						 tabstop;
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

static void indent_line(struct view *view,
			position_t lnstart, position_t first_nonspace,
			unsigned indentation) {
	char *indent;
	unsigned indent_bytes;
	if (view->text->flags & TEXT_NO_TABS) {
		indent_bytes = indentation;
		indent = allocate(indent_bytes);
		memset(indent, ' ', indentation);
	} else {
		unsigned tabstop = view->text->tabstop;
		tabstop |= !tabstop;
		indent_bytes = indentation / tabstop + indentation % tabstop;
		indent = allocate(indent_bytes);
		memset(indent, '\t', indentation / tabstop);
		memset(indent + indentation / tabstop, ' ',
		       indentation % tabstop);
	}

	view_delete(view, lnstart, first_nonspace - lnstart);
	view_insert(view, indent, lnstart, indent_bytes);
	RELEASE(indent);
}

void align(struct view *view)
{
	position_t cursor = locus_get(view, CURSOR);
	position_t lnstart0 = find_line_start(view, cursor);
	position_t nonspace0, lnstart, offset, at, corr;
	Unicode_t ch, firstch;
	unsigned indent, brindent, spaces, chars;
	unsigned tabstop = view->text->tabstop;
	Boolean_t is_nested;

	if (!lnstart0)
		return;
	tabstop |= !tabstop;

	for (nonspace0 = lnstart0;
	     IS_UNICODE(firstch = view_char(view, nonspace0, &offset));
	     nonspace0 = offset)
		if (firstch == '\n' ||
		    !IS_CODEPOINT(firstch) ||
		    !isspace(firstch))
			break;
	lnstart = lnstart0 - 1;

	/* Find previous non-blank line */
again:	lnstart = find_line_start(view, lnstart);
	while (lnstart && view_char(view, lnstart, NULL) == '\n')
		lnstart = find_line_start(view, lnstart-1);

	/* Determine its indentation */
	indent = spaces = chars = brindent = 0;
	for (offset = at = lnstart;
	     IS_UNICODE(ch = view_char(view, offset, &offset)) &&
	     ch != '\n'; )
		if (ch == ' ')
			spaces += !chars;
		else if (ch == '\t') {
			indent = ((indent + spaces + chars) /
				  tabstop + 1) * tabstop;
			spaces = chars = 0;
			at = offset;
		} else
			chars++;
	indent += spaces;
	at += spaces;

	/* Adjust for nesting */
	brindent = indent + 1;
	is_nested = FALSE;
	for (ch = view_char(view, at, &offset);
	     IS_UNICODE(ch) && ch != '\n';
	     ch = view_char(view, at = offset, &offset), brindent++) {
		switch (ch) {
		case '(':
		case '[':
			corr = find_corresponding_bracket(view, at);
			if (corr >= lnstart0) {
				indent = brindent;
				is_nested = TRUE;
			}
			break;
		case ')':
		case ']':
			corr = find_corresponding_bracket(view, at);
			if (corr < lnstart) {
				lnstart = corr;
				goto again;
			}
			break;
		case '\t':
			brindent = (brindent / tabstop + 1) * tabstop;
			break;
		}
	}

	/* Adjust for clues at the end of the previous line */
	if (!is_nested && lnstart0) {
		ch = view_char_prior(view, lnstart0 - 1, NULL);
		if (ch == '{' || ch == ')' || ch == ':')
			indent = (indent / tabstop + 1) * tabstop;
		else if (indent >= tabstop) {
			offset = lnstart;
			do ch = view_char_prior(view, offset, &offset);
			while (ch == ' ' || ch == '\t' || ch == '\n');
			if (ch == ')' || firstch == '}')
				indent = (indent / tabstop - 1) * tabstop;
		}
	}

	indent_line(view, lnstart0, nonspace0, indent);
}
