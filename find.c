#include "all.h"

/* Routines that scan characters in views */

position_t find_line_start(struct view *view, position_t offset)
{
	Unicode_t ch;
	position_t prev;

	while (IS_UNICODE(ch = view_char_prior(view, offset, &prev)) &&
	       ch != '\n')
		offset = prev;
	return offset;
}

position_t find_line_end(struct view *view, position_t offset)
{
	Unicode_t ch;
	position_t next;

	while (IS_UNICODE(ch = view_char(view, offset, &next)) &&
	       ch != '\n')
		offset = next;
	return offset;
}

position_t find_paragraph_start(struct view *view, position_t offset)
{
	Unicode_t ch, nch = UNICODE_BAD, nnch = UNICODE_BAD;
	position_t prev;

	while (IS_UNICODE(ch = view_char_prior(view, offset, &prev))) {
		if (ch == '\n' && nch == '\n' && IS_UNICODE(nnch))
			return offset + 1;
		offset = prev, nnch = nch, nch = ch;
	}
	return offset;
}

position_t find_paragraph_end(struct view *view, position_t offset)
{
	Unicode_t ch, pch = UNICODE_BAD, ppch = UNICODE_BAD;
	position_t next;

	while (IS_UNICODE(ch = view_char(view, offset, &next)) &&
	       (ch == '\n' || pch != '\n' || ppch != '\n'))
		offset = next, ppch = pch, pch = ch;
	return offset;
}

position_t find_space(struct view *view, position_t offset)
{
	Unicode_t ch;
	position_t next;

	while (IS_UNICODE(ch = view_char(view, offset, &next)) &&
	       !isspace(ch))
		offset = next;
	return offset;
}

position_t find_nonspace(struct view *view, position_t offset)
{
	position_t next;

	while (isspace(view_char(view, offset, &next)))
		offset = next;
	return offset;
}

position_t find_word_start(struct view *view, position_t offset)
{
	Unicode_t ch;
	position_t prev;

	while (IS_UNICODE(ch = view_char_prior(view, offset, &prev))) {
		offset = prev;
		if (!isspace(ch))
			break;
	}
	while (is_wordch(view_char_prior(view, offset, &prev)))
		offset = prev;
	return offset;
}

position_t find_word_end(struct view *view, position_t offset)
{
	Unicode_t ch;
	position_t next;

	offset = find_nonspace(view, offset)+1;
	for (; IS_UNICODE(ch = view_char(view, offset, &next)); offset = next)
		if (!is_wordch(ch))
			break;
	return offset;
}

position_t find_id_start(struct view *view, position_t offset)
{
	Unicode_t ch;
	position_t prev;

	while (IS_UNICODE(ch = view_char_prior(view, offset, &prev))) {
		offset = prev;
		if (!isspace(ch))
			break;
	}
	while (is_idch((ch = view_char_prior(view, offset, &prev))) ||
	       ch == ':' && view_char_prior(view, prev, &prev) == ':')
		offset = prev;
	return offset;
}

position_t find_id_end(struct view *view, position_t offset)
{
	Unicode_t ch;
	position_t next;

	offset = find_nonspace(view, offset)+1;
	for (; IS_UNICODE(ch = view_char(view, offset, &next)); offset = next)
		if (!(is_idch(ch) ||
		      ch == ':' && view_char(view, next, &next) == ':'))
			break;
	return offset;
}

position_t find_sentence_start(struct view *view, position_t offset)
{
	position_t prev;
	Unicode_t ch, next = view_char_prior(view, offset, &prev);

	if (!IS_UNICODE(next))
		return offset;
	while (IS_UNICODE(ch = view_char_prior(view, offset = prev, &prev)) &&
	       ch != '.' && ch != ',' && ch != ';' && ch != ':' &&
	       ch != '!' && ch != '?' &&
	       ch != '(' && ch != '[' && ch != '{' &&
	       (ch != '\n' || ch != next))
		next = ch;
	return offset;
}

position_t find_sentence_end(struct view *view, position_t offset)
{
	position_t next;
	Unicode_t ch, last = view_char(view, offset, &next);

	if (!IS_UNICODE(last))
		return offset;
	while (IS_UNICODE(ch = view_char(view, offset = next, &next)) &&
	       ch != '.' && ch != ',' && ch != ';' && ch != ':' &&
	       ch != '!' && ch != '?' &&
	       ch != ')' && ch != ']' && ch != '}' &&
	       (ch != '\n' || ch != last))
		last = ch;
	return offset;
}

sposition_t find_corresponding_bracket(struct view *view, position_t offset)
{
	static signed char peer[0x100], updown[0x100];
	position_t next;
	Unicode_t ch = view_char(view, offset, &next);
	Byte_t stack[32];
	int stackptr = 0, dir;

	if (!peer['(']) {
		peer['('] = ')', peer[')'] = '(';
		peer['['] = ']', peer[']'] = '[';
		peer['{'] = '}', peer['}'] = '{';
		updown['('] = updown['['] = updown['{'] = 1;
		updown[')'] = updown[']'] = updown['}'] = -1;
	}

	if (ch >= sizeof updown || !(dir = updown[ch])) {
		position_t back = offset, ahead, next = offset;
		while (IS_UNICODE(ch = view_char_prior(view, back, &back))) {
			if (ch >= sizeof updown)
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
		if (!IS_UNICODE(ch))
			back = offset+1;
		while (IS_UNICODE(ch = view_char(view, ahead = next, &next))) {
			if (ch >= sizeof updown)
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
		    (offset - back <= ahead - offset || !IS_UNICODE(ch)))
			return back;
		return IS_UNICODE(ch) ? ahead : -1;
	}

	stack[stackptr++] = ch;
	if (dir > 0)
		offset = next;
	while (stackptr) {
		ch = (dir > 0 ? view_char : view_char_prior)
			(view, offset, &next);
		if (ch >= sizeof updown)
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

position_t find_line_number(struct view *view, unsigned line)
{
	position_t offset;

	for (offset = 0;
	     offset < view->bytes;
	     offset = find_line_end(view, offset) + 1)
		if (!--line)
			break;
	return offset;
}

position_t find_row_bytes(struct view *view, position_t offset0,
			  unsigned column, unsigned columns)
{
	position_t offset = offset0, next;
	unsigned tabstop = view->text->tabstop;
	Unicode_t ch = 0;
	int charcols;

	while (column < columns) {
		if (!IS_UNICODE(ch = view_char(view, offset, &next)))
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

position_t find_column(unsigned *row, struct view *view, position_t linestart,
		       position_t offset, unsigned column, unsigned columns)
{
	unsigned tabstop = view->text->tabstop;
	position_t at, next;

	for (at = linestart; at < offset; at = next) {
		Unicode_t ch = view_char(view, at, &next);
		if (!IS_UNICODE(ch))
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

sposition_t find_string(struct view *view, const char *string,
			position_t offset)
{
	const Byte_t *ustring = (const Byte_t *) string;
	unsigned first = *ustring, j;
	Unicode_t ch;

	for (; IS_UNICODE(ch = view_byte(view, offset)); offset++) {
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
