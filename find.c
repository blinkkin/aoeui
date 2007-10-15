#include "all.h"

/* Routines that scan characters in views */

unsigned find_line_start(struct view *view, unsigned offset)
{
	int ch;
	unsigned prev;
	while ((ch = view_char_prior(view, offset, &prev)) >= 0 &&
	       ch != '\n')
		offset = prev;
	return offset;
}

unsigned find_line_end(struct view *view, unsigned offset)
{
	int ch;
	unsigned next;
	while ((ch = view_char(view, offset, &next)) >= 0 &&
	       ch != '\n')
		offset = next;
	return offset;
}

unsigned find_paragraph_start(struct view *view, unsigned offset)
{
	int ch, nch = -1, nnch = -1;
	unsigned prev;
	while ((ch = view_char_prior(view, offset, &prev)) >= 0) {
		if (ch == '\n' && nch == '\n' && nnch >= 0)
			return offset + 1;
		offset = prev, nnch = nch, nch = ch;
	}
	return offset;
}

unsigned find_paragraph_end(struct view *view, unsigned offset)
{
	int ch, pch = -1, ppch = -1;
	unsigned next;
	while ((ch = view_char(view, offset, &next)) >= 0 &&
	       (ch == '\n' || pch != '\n' || ppch != '\n'))
		offset = next, ppch = pch, pch = ch;
	return offset;
}

unsigned find_space(struct view *view, unsigned offset)
{
	int ch;
	unsigned next;
	while ((ch = view_char(view, offset, &next)) >= 0 &&
	       !isspace(ch))
		offset = next;
	return offset;
}

unsigned find_nonspace(struct view *view, unsigned offset)
{
	unsigned next;
	while (isspace(view_char(view, offset, &next)))
		offset = next;
	return offset;
}

unsigned find_word_start(struct view *view, unsigned offset)
{
	int ch;
	unsigned prev;
	while ((ch = view_char_prior(view, offset, &prev)) >= 0) {
		offset = prev;
		if (!isspace(ch))
			break;
	}
	while (is_wordch(view_char_prior(view, offset, &prev)))
		offset = prev;
	return offset;
}

unsigned find_word_end(struct view *view, unsigned offset)
{
	int ch;
	unsigned next;
	offset = find_nonspace(view, offset)+1;
	for (; (ch = view_char(view, offset, &next)) >= 0; offset = next)
		if (!is_wordch(ch))
			break;
	return offset;
}

unsigned find_id_start(struct view *view, unsigned offset)
{
	int ch;
	unsigned prev;
	while ((ch = view_char_prior(view, offset, &prev)) >= 0) {
		offset = prev;
		if (!isspace(ch))
			break;
	}
	while (is_idch(view_char_prior(view, offset, &prev)))
		offset = prev;
	return offset;
}

unsigned find_id_end(struct view *view, unsigned offset)
{
	int ch;
	unsigned next;
	offset = find_nonspace(view, offset)+1;
	for (; (ch = view_char(view, offset, &next)) >= 0; offset = next)
		if (!is_idch(ch))
			break;
	return offset;
}

unsigned find_sentence_start(struct view *view, unsigned offset)
{
	unsigned prev;
	int ch, next = view_char_prior(view, offset, &prev);
	if (next < 0)
		return offset;
	while ((ch = view_char_prior(view, offset = prev, &prev)) >= 0 &&
	       ch != '.' && ch != ',' && ch != ';' && ch != ':' &&
	       ch != '!' && ch != '?' &&
	       ch != '(' && ch != '[' && ch != '{' &&
	       (ch != '\n' || ch != next))
		next = ch;
	return offset;
}

unsigned find_sentence_end(struct view *view, unsigned offset)
{
	unsigned next;
	int ch, last = view_char(view, offset, &next);
	if (last < 0)
		return offset;
	while ((ch = view_char(view, offset = next, &next)) >= 0 &&
	       ch != '.' && ch != ',' && ch != ';' && ch != ':' &&
	       ch != '!' && ch != '?' &&
	       ch != ')' && ch != ']' && ch != '}' &&
	       (ch != '\n' || ch != last))
		last = ch;
	return offset;
}

int find_corresponding_bracket(struct view *view, unsigned offset)
{
	static signed char peer[0x100], updown[0x100];
	unsigned next;
	int ch = view_char(view, offset, &next);
	unsigned char stack[32];
	int stackptr = 0, dir;

	if (!peer['(']) {
		peer['('] = ')', peer[')'] = '(';
		peer['['] = ']', peer[']'] = '[';
		peer['{'] = '}', peer['}'] = '{';
		updown['('] = updown['['] = updown['{'] = 1;
		updown[')'] = updown[']'] = updown['}'] = -1;
	}

	if ((unsigned) ch >= 0x100 || !(dir = updown[ch])) {
		unsigned back = offset, ahead, next = offset;
		while ((ch = view_char_prior(view, back, &back)) >= 0) {
			if (ch >= 0x100)
				continue;
			if (updown[ch] < 0)
				if (stackptr == sizeof stack)
					break;
				else
					stack[stackptr++] = ch;
			else if (updown[ch] > 0 &&
				 (!stackptr || ch != peer[stack[--stackptr]]))
				break;
		}
		if (ch < 0)
			back = offset+1;
		while ((ch = view_char(view, ahead = next, &next)) >= 0) {
			if (ch >= 0x100)
				continue;
			if (updown[ch] > 0)
				if (stackptr == sizeof stack)
					break;
				else
					stack[stackptr++] = ch;
			else if (updown[ch] < 0 &&
				 (!stackptr || ch != peer[stack[--stackptr]]))
				break;
		}
		if (back < offset &&
		    (offset - back <= ahead - offset || ch < 0))
			return back;
		return ch >= 0 ? ahead : -1;
	}

	stack[stackptr++] = ch;
	if (dir > 0)
		offset = next;
	while (stackptr) {
		ch = (dir > 0 ? view_char : view_char_prior)
			(view, offset, &next);
		if (ch < 0)
			return -1;
		if (updown[ch] == dir) {
			if (stackptr == sizeof stack)
				return -1;
			stack[stackptr++] = ch;
		} else if (updown[ch] == -dir)
			if (ch != peer[stack[--stackptr]] ||
			    !stackptr && dir > 0)
				break;
		offset = next;
	}

	return offset;
}

unsigned find_line_number(struct view *view, unsigned line)
{
	unsigned offset;
	for (offset = 0;
	     offset < view->bytes;
	     offset = find_line_end(view, offset) + 1)
		if (!--line)
			break;
	return offset;
}

unsigned find_row_bytes(struct view *view, unsigned offset0,
			unsigned column, unsigned columns)
{
	unsigned offset = offset0, next;
	unsigned tabstop = view->text->tabstop;
	int ch = 0, charcols;

	while (column < columns) {
		if ((ch = view_char(view, offset, &next)) < 0)
			break;
		if (ch == '\n') {
			offset = next;
			break;
		}
		charcols = char_columns(ch, column, tabstop);
		if (column+charcols > columns)
			break;
		column += charcols;
		offset = next;
	}

	if (column == columns &&
	    offset != locus_get(view, CURSOR) &&
	    view_char(view, offset, NULL) == '\n')
		offset++;
	return offset - offset0;
}

unsigned find_column(unsigned *row, struct view *view, unsigned linestart,
		     unsigned offset, unsigned column, unsigned columns)
{
	unsigned tabstop = view->text->tabstop;
	unsigned at, next;

	for (at = linestart; at < offset; at = next) {
		int ch = view_char(view, at, &next);
		if (ch < 0)
			break;
		if (ch == '\n') {
			if (row)
				++*row;
			column = 0;
		} else {
			column += char_columns(ch, column, tabstop);
			if (column >= columns) {
				if (row)
					++*row;
				column = char_columns(ch, 0, tabstop);
			}
		}
	}
	return column;
}

int find_string(struct view *view, const char *string, unsigned offset)
{
	const unsigned char *ustring = (const unsigned char *) string;
	unsigned first = *ustring, j;
	int ch;
	for (; (ch = view_byte(view, offset)) >= 0; offset++) {
		if (ch != first)
			continue;
		for (j = 1; (ch = ustring[j]); j++)
			if (ch != view_byte(view, offset+j))
				break;
		if (!ch)
			return offset;
	}
	return -1;
}
