/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#include "all.h"

/* Routines that scan characters in views */

position_t find_line_start(struct view *view, position_t offset)
{
	Unicode_t ch;
	position_t prev;

	while (IS_UNICODE((ch = view_char_prior(view, offset, &prev))) &&
	       ch != '\n')
		offset = prev;
	return offset;
}

position_t find_line_end(struct view *view, position_t offset)
{
	Unicode_t ch;
	position_t next;

	while (IS_UNICODE((ch = view_char(view, offset, &next))) &&
	       ch != '\n')
		offset = next;
	return offset;
}

position_t find_paragraph_start(struct view *view, position_t offset)
{
	Unicode_t ch, nch = UNICODE_BAD, nnch = UNICODE_BAD;
	position_t prev;

	while (IS_UNICODE((ch = view_char_prior(view, offset, &prev)))) {
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

	while (IS_UNICODE((ch = view_char(view, offset, &next))) &&
	       (ch == '\n' || pch != '\n' || ppch != '\n'))
		offset = next, ppch = pch, pch = ch;
	return offset;
}

static void updown_goal(struct view *view, position_t at)
{
	if (at == view->goal.cursor)
		;
	else if (view_byte(view, at) == '\n')
		view->goal.row = ~0;
	else {
		view->goal.row = 0;
		view->goal.column = find_column(&view->goal.row, view,
						find_line_start(view, at),
						at, 0);
	}
}

static position_t same_column(struct view *view, position_t at, position_t fail)
{
	unsigned r = 0, c = 0;
	unsigned tabstop = view->text->tabstop;
	unsigned columns = window_columns(view->window);
	position_t next;

	for (; r < view->goal.row || c < view->goal.column; at = next) {
		Unicode_t ch;
		ch = view_char(view, at, &next);
		if (!IS_UNICODE(ch) || ch == '\n') {
			at = fail;
			break;
		}
		if ((c += char_columns(ch, c, tabstop)) > view->goal.column &&
		    r == view->goal.row)
			break;
		if (c > columns)
			r++, c = char_columns(ch, 0, tabstop);
	}
	return view->goal.cursor = at;
}

position_t find_line_up(struct view *view, position_t at)
{
	position_t linestart = find_line_start(view, at);

	updown_goal(view, at);
	if (!linestart)
		return 0;
	return same_column(view, find_line_start(view, linestart-1),
			   linestart-1);
}

position_t find_line_down(struct view *view, position_t at)
{
	position_t nextstart = find_line_end(view, at) + 1;

	updown_goal(view, at);
	if (nextstart >= view->bytes)
		return view->bytes;
	return same_column(view, nextstart, find_line_end(view, nextstart));
}

typedef Unicode_t (*stepper_t)(struct view *, position_t, position_t *);

static position_t find_not(struct view *view, position_t offset,
			   stepper_t stepper,
			   Boolean_t (*test)(Unicode_t))
{
	position_t next;

	while (test(stepper(view, offset, &next)))
		offset = next;
	return offset;
}

static Boolean_t space_test(Unicode_t ch)
{
	return IS_CODEPOINT(ch) && isspace(ch);
}

static Boolean_t nonspace_test(Unicode_t ch)
{
	return IS_UNICODE(ch) && !space_test(ch);
}

position_t find_space(struct view *view, position_t offset)
{
	return find_not(view, offset, view_char, nonspace_test);
}

position_t find_space_prior(struct view *view, position_t offset)
{
	return find_not(view, offset, view_char_prior, nonspace_test);
}

position_t find_nonspace(struct view *view, position_t offset)
{
	return find_not(view, offset, view_char, space_test);
}

position_t find_nonspace_prior(struct view *view, position_t offset)
{
	return find_not(view, offset, view_char_prior, space_test);
}


static position_t find_contiguous(struct view *view, position_t offset,
				  stepper_t stepper,
				  Boolean_t (*test)(Unicode_t, struct view *,
						    position_t *, stepper_t))
{
	Unicode_t ch;
	position_t next;
	Boolean_t in_region = FALSE;

	for (; IS_UNICODE((ch = stepper(view, offset, &next))); offset = next)
		if (test(ch, view, &next, stepper))
			in_region = TRUE;
		else if (in_region)
			break;
	return offset;
}

static Boolean_t word_test(Unicode_t ch, struct view *view, position_t *next,
			   stepper_t stepper)
{
	return is_wordch(ch);
}

position_t find_word_start(struct view *view, position_t offset)
{
	return find_contiguous(view, offset, view_char_prior, word_test);
}

position_t find_word_end(struct view *view, position_t offset)
{
	return find_contiguous(view, offset, view_char, word_test);
}

static Boolean_t id_test(Unicode_t ch, struct view *view, position_t *next,
			 stepper_t stepper)
{
	return is_idch(ch) ||
	       ch == ':' && stepper(view, *next, next) == ':';
}

position_t find_id_start(struct view *view, position_t offset)
{
	return find_contiguous(view, offset, view_char_prior, id_test);
}

position_t find_id_end(struct view *view, position_t offset)
{
	return find_contiguous(view, offset, view_char, id_test);
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
	static signed char peer[256], updown[256];
	const char *p = view->text->brackets;
	position_t next;
	Unicode_t ch = view_char(view, offset, &next);
	Byte_t stack[32];
	int stackptr = 0, dir;

	if (!p)
		return -1;
	memset(peer, 0, sizeof peer);
	memset(updown, 0, sizeof updown);
	for (; *p; p += 2) {
		int L = (unsigned char) p[0], R = (unsigned char) p[1];
		peer[L] = R;
		peer[R] = L;
		updown[L] = 1;
		updown[R] = -1;
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

unsigned find_column(unsigned *row, struct view *view, position_t at,
		     position_t offset, unsigned column)
{
	unsigned tabstop = view->text->tabstop;
	unsigned columns = window_columns(view->window);
	position_t next;

	for (; at < offset; at = next) {
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
