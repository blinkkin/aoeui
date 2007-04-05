#include "all.h"
#include "display.h"

/*
 *	A "window" is a presentation of a view on part or all of
 *	the display surface.  One window is active, meaning that
 *	it directs keyboard input to its view's command handler.
 */

static struct window *window_list;
static struct window *active_window;
static struct display *display;
static unsigned display_rows, display_columns;

static struct window *activate(struct window *window)
{
	if ((active_window = window))
		display_title(display, active_window->view->name);
	return window;
}

static struct window *window_create(struct view *view, struct view *after)
{
	struct window *window = view->window;

	if (!window) {
		window = allocate(NULL, sizeof *window);
		memset(window, 0, sizeof *window);
		window->view = view;
		view->window = window;
		window->start = locus_create(view, UNSET);
		if (after && after->window) {
			window->next = after->window->next;
			after->window->next = window;
		} else {
			window->next = window_list;
			window_list = window;
		}
	} else
		window->row = window->column = 0;
	window->rows = display_rows;
	window->columns = display_columns;
	window->last_dirties = ~0;
	return window;
}

static int stacked(struct window *up, struct window *down)
{
	return	up && down &&
		up->column == down->column &&
		up->columns == down->columns &&
		up->row + up->rows == down->row;
}

static int beside(struct window *left, struct window *right)
{
	return	left && right &&
		left->row == right->row &&
		left->rows == right->rows &&
		left->column + left->columns == right->column;
}

static int window_expand_next(struct window *window)
{
	struct window *next = window->next;
	if (!next)
		return 0;
	if (stacked(window, next)) {
		next->row = window->row;
		next->rows += window->rows;
		return 1;
	}
	if (beside(window, next)) {
		next->column = window->column;
		next->columns += window->columns;
		return 1;
	}
	return 0;
}

static void window_close(struct window *window)
{
	struct window *previous = NULL, *wp;

	for (wp = window_list; wp != window; previous = wp, wp = wp->next)
		if (wp == window)
			break;
	if (!wp)
		;
	else if (previous) {
		struct window *next = previous->next = window->next;
		if (stacked(previous, window) &&
		    (!stacked(window, next) ||
		     previous->rows <= next->rows))
			previous->rows += window->rows;
		else if (beside(previous, window) &&
			 (!beside(window, next) ||
			  previous->columns <= next->columns))
			previous->columns += window->columns;
		else if (!window_expand_next(window))
			window_raise(previous->view);
	} else if ((window_list = window->next) &&
		   !window_expand_next(window))
		window_raise(window_list->view);

	if (window->view) {
		locus_destroy(window->view, window->start);
		window->view->window = NULL;
	}
	if (window == active_window)
		activate(window_list);
	allocate(window, 0);
}

struct window *window_raise(struct view *view)
{
	if (!display)
		display = display_init();
	display_get_geometry(display, &display_rows, &display_columns);
	while (window_list && window_list->view != view)
		window_close(window_list);
	while (window_list && window_list->next)
		window_close(window_list->next);
	return activate(window_create(view, NULL));
}

struct window *window_activate(struct view *view)
{
	if (view->window)
		return activate(view->window);
	return window_raise(view);
}

struct window *window_after(struct view *before, struct view *view,
			    int vertical)
{
	struct window *window, *old;

	if (view->window)
		return view->window;
	if (!before ||
	    !(old = before->window) ||
	    old->rows < 4 ||
	    old->columns < 16)
		return window_raise(view);
	window = window_create(view, before);
	if (vertical < 0)
		vertical = old->columns > 120;
	if (vertical) {
		window->row = old->row;
		window->rows = old->rows;
		window->column = old->column +
			(old->columns -=
			 (window->columns =
			  old->columns >> 1));
	} else {
		window->column = old->column;
		window->columns = old->columns;
		window->row = old->row +
			(old->rows -=
			 (window->rows =
			  old->rows >> 1));
	}
	return activate(window);
}

struct window *window_replace(struct view *old, struct view *new)
{
	struct window *window = old->window;

	if (!new)
		return NULL;
	if (new->window)
		return new->window;
	if (!old || !window)
		return window_raise(new);
	window->view = new;
	old->window = NULL;
	locus_destroy(old, window->start);
	window->start = locus_create(new, locus_get(new, CURSOR)+1);
	return activate(new->window = window);
}

void window_unmap(struct view *view)
{
	if (view && view->window)
		window_close(view->window);
}

struct view *window_current_view(void)
{
	if (!active_window) {
		if (!text_list) {
			if (!access(HELP_PATH, R_OK))
				view_open(HELP_PATH);
			else {
				errno = 0;
				message("\nWelcome to aoeui, pmk's "
					"Dvorak-optimized display editor.\n");
			}
		}
		window_raise(text_list->views);
	}
	return active_window->view;
}

void windows_reset(void)
{
	if (display) {
		display_end(display);
		display = NULL;
	}
	window_raise(window_current_view());
}

void windows_end(void)
{
	while (window_list)
		window_close(window_list);
	if (display) {
		display_end(display);
		display = NULL;
	}
}

INLINE unsigned char_columns(unsigned ch, unsigned column,
				unsigned tabstop)
{
	if (ch == '\t')
		return tabstop - column % tabstop;
	if (ch < ' ' || ch == 0x7f)
		return 2; /* ^X */
	return 1;
}

static unsigned row_bytes(struct view *view, int offset0, int columns)
{
	int offset = offset0;
	unsigned column = 0, chlen;
	unsigned tabstop = view->text->tabstop;
	int ch = 0;

	for (column = 0; column < columns; ) {
		ch = view_unicode(view, offset, &chlen);
		offset += chlen;
		if (ch == '\n' || ch < 0)
			break;
		column += char_columns(ch, column, tabstop);
	}

	if (column > columns)
		offset -= chlen;
	else if (column == columns &&
		 offset != locus_get(view, CURSOR)) {
		ch = view_unicode(view, offset, &chlen);
		if (ch < 0 || ch == '\n')
			offset += chlen;
	}
	return offset - offset0;
}

static void set_cursor(struct window *window, unsigned offset,
			unsigned row)
{
	struct view *view = window->view;
	unsigned cursor = locus_get(view, CURSOR);
	unsigned tabstop = view->text->tabstop;
	unsigned chlen, column;
	int ch;

	for (column = 0; offset < cursor; offset += chlen) {
		ch = view_unicode(view, offset, &chlen);
		if (ch < 0)
			break;
		if (ch == '\n') {
			row++;
			column = 0;
		} else
			column += char_columns(ch, column, tabstop);
	}
	window->cursor_row = row;
	window->cursor_column = column;
}

static unsigned count_rows(struct window *window, unsigned start, unsigned end)
{
	unsigned rows = 0, bytes;

	for (rows = 0; start < end; rows++, start += bytes)
		if (!(bytes = row_bytes(window->view, start, window->columns)))
			break;
	return rows;
}

static unsigned find_row_start(struct window *window, unsigned position,
			       unsigned start)
{
	unsigned bytes;
	while ((bytes = row_bytes(window->view, start, window->columns))) {
		if (start + bytes > position)
			break;
		start += bytes;
	}
	return start;
}

static void new_start(struct view *view, unsigned start)
{
	locus_set(view, view->window->start, start ? start-1 : UNSET);
	view->window->last_dirties = ~0;
}

static unsigned screen_start(struct view *view)
{
	unsigned start = locus_get(view, view->window->start);
	if (start++ == UNSET)
		start = 0;
	return start;
}

static unsigned focus(struct window *window)
{
	struct view *view = window->view;
	unsigned cursor = locus_get(view, CURSOR);
	unsigned cursorrow = find_row_start(window, cursor,
					    find_line_start(view, cursor));
	unsigned start = screen_start(view);
	unsigned end, above, below, at, bytes;

	start = find_row_start(window, start,
			       find_line_start(view, start));
	if (cursorrow >= start &&
	    (above = count_rows(window, start, cursorrow)) < window->rows) {
		for (below = 1, at = cursorrow;
		     above + below < window->rows;
		     below++, at += bytes)
			if (!(bytes = row_bytes(view, at, window->columns)))
				break;
		if (above + below == window->rows || !start)
			goto done;
	}

	for (start = cursorrow, above = 0;
	     (end = start) && above < window->rows >> 1;
	     above += count_rows(window, start, end))
		start = find_line_start(view, start-1);
	for (; above >= window->rows >> 1; above--)
		start += row_bytes(view, start, window->columns);

	for (below = 1, at = cursorrow;
	     above + below < window->rows;
	     below++, at += bytes)
		if (!(bytes = row_bytes(view, at, window->columns)))
			break;
	for (; (end = start) && above + below < window->rows;
	     above += count_rows(window, start, end))
		start = find_line_start(view, start-1);
	for (; above + below > window->rows; above--)
		start += row_bytes(view, start, window->columns);

done:	set_cursor(window, cursorrow, above);
	new_start(view, start);
	return start;
}

static int lame_space(struct view *view, unsigned offset, unsigned look)
{
	int all_spaces_lame = look > 1;
	while (look--) {
		unsigned chlen;
		int ch = view_unicode(view, offset, &chlen);
		if (ch < 0 || ch == '\n' || ch == '\t')
			return 1;
		if (ch != ' ')
			return 0;
		offset += chlen;
	}
	return all_spaces_lame;
}

static int lame_tab(struct view *view, unsigned offset)
{
	int ch;
	unsigned chlen;

	for (; (ch = view_unicode(view, offset, &chlen)) >= 0 && ch != '\n';
	     offset += chlen)
		if (ch != ' ' && ch != '\t')
			return 0;
	return 1;
}

static void paint(struct window *window, unsigned default_bgrgba)
{
	unsigned at, row;
	struct view *view = window->view;
	unsigned tabstop = view->text->tabstop;
	unsigned cursor = locus_get(view, CURSOR);
	unsigned mark = locus_get(view, MARK);
	unsigned columns = window->columns;

	if (window->view->text->dirties == window->last_dirties &&
	    window->last_bgrgba == default_bgrgba &&
	    window->last_cursor == cursor &&
	    window->last_mark == mark)
		return;
	window->last_dirties = window->view->text->dirties;
	window->last_bgrgba = default_bgrgba;
	window->last_cursor = cursor;
	window->last_mark = mark;

	if (mark == UNSET)
		mark = cursor;

	at = focus(window);
	for (row = window->row; row < window->row + window->rows; row++) {

		unsigned limit = at + row_bytes(view, at, columns);
		unsigned chlen, column, fgrgba, bgrgba;
		int ch;

		for (column = 0; at < limit; at += chlen) {

			ch = view_unicode(view, at, &chlen);
			if (ch < 0 || ch == '\n') {
				at = limit;
				break;
			}

			if (at >= cursor && at < mark ||
			    at >= mark && at < cursor) {
				bgrgba = 0x00ffff00;
				fgrgba = bgrgba ^ 0xffffff00;
			} else {
				bgrgba = default_bgrgba;
				if (view->text->flags & TEXT_RDONLY)
					fgrgba = 0xff000000;
				else
					fgrgba = bgrgba ^ 0xffffff00;
			}

			if (ch == '\t') {
				unsigned bg = lame_tab(view, at+1) ?
						0xff00ff00 : bgrgba;
				do {
					display_put(display, row,
						    window->column + column++,
						    ' ', 1, bg);
					bg = bgrgba;
				} while (column % tabstop);
			} else {
				if (ch < ' ' || ch == 0x7f) {
					bgrgba = 0xff000000;
					fgrgba = 0xffffff00;
					display_put(display, row,
						    window->column + column++,
						    '^', fgrgba, bgrgba);
					if (ch == 0x7f)
						ch = '?';
					else
						ch += '@';
				} else if (ch == ' ' &&
					   lame_space(view, at + 1,
						      tabstop-1 -
							column % tabstop))
						bgrgba = 0xff00ff00;
				display_put(display, row,
					    window->column + column++, ch,
					    fgrgba, bgrgba);
			}
		}

		display_erase(display, row, window->column + column,
			      1, columns - column, default_bgrgba);
	}
}

void window_next(struct view *view)
{
	if (view && view->window)
		activate(view->window->next ? view->window->next : window_list);
}

struct window *window_recenter(struct view *view)
{
	struct window *window = view->window;
	unsigned cursor = locus_get(view, CURSOR);
	unsigned start, row, end;

	if (!window)
		window = window_raise(view);
	start = find_row_start(window, cursor, find_line_start(view, cursor));
	for (row = 0;
	     (end = start) && row < window->rows >> 1;
	     row += count_rows(window, start, end))
		start = find_line_start(view, start-1);
	while(row-- > window->rows >> 1)
		start += row_bytes(view, start, window->columns);
	new_start(view, start);
	return window;
}

#define OVERLAP 1
void window_page_up(struct view *view)
{
	struct window *window = view->window;
	unsigned start = screen_start(view);
	unsigned end, row, bytes;

	if (start) {
		for (row = 0;
		     (end = start) && row + OVERLAP < window->rows;
		     row += count_rows(window, start, end))
			start = find_line_start(view, start-1);
		while (row-- + OVERLAP > window->rows)
			start += row_bytes(view, start, window->columns);
		new_start(view, start);
		for (row = 0; row < window->rows-1; row++, start += bytes)
			if (!(bytes = row_bytes(view, start, window->columns)))
				break;
	}
	locus_set(view, CURSOR, start);
}

void window_page_down(struct view *view)
{
	struct window *window = view->window;
	unsigned start = screen_start(view);
	unsigned row, bytes;

	for (row = 0; row + OVERLAP < window->rows; row++, start += bytes)
		if (!(bytes = row_bytes(view, start, window->columns)))
			break;
	new_start(view, start);
	locus_set(view, CURSOR, start);
}

void window_beep(struct view *view)
{
	if (display)
		display_beep(display);
	else
		fputc('\a', stderr);
}

static void repaint(void)
{
	struct window *window;
	unsigned odd = 0;

	for (window = window_list; window; window = window->next) {
		unsigned bgrgba;
		if (window == active_window)
			bgrgba = ~0;
		else
			bgrgba = odd ? 0xffff0000 : 0xffffff00;
		odd ^= window->next &&
		       (window->next->row == window->row ||
			window->next->column == window->column);
		paint(window, bgrgba);
	}

	display_cursor(display, active_window->row +  active_window->cursor_row,
		       active_window->column + active_window->cursor_column);
}

int window_getch(void)
{
	int block = 0;
	for (;;) {
		int ch = display_getch(display, block);
		if (ch == DISPLAY_WINCH)
			windows_reset();
		else if (ch != DISPLAY_NONE)
			return ch;
		repaint();
		block = 1;
	}
}
