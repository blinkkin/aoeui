#include "all.h"

/*
 *	A "window" is a presentation of a view on part or all of
 *	the display surface.  One window is active, meaning that
 *	it directs keyboard input to its view's command handler.
 */

struct window {
	struct view *view;
	unsigned start; /* locus index */
	unsigned row, column;
	unsigned rows, columns;
	unsigned cursor_row, cursor_column;
	rgba_t fgrgba, bgrgba;
	unsigned last_dirties, last_cursor, last_mark;
	rgba_t last_fgrgba, last_bgrgba;
	struct window *next;
};

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
		window = allocate0(sizeof *window);
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

static int adjacent(struct window *x, struct window *y)
{
	if (x->column + x->columns < y->column ||
	    y->column + y->columns < x->column ||
	    x->row + x->rows < y->row ||
	    y->row + y->rows < x->row)
		return 0;
	if (y->column != x->column + x->columns &&
	    x->column != y->column + y->columns)
		return 1;
	return y->row < x->row + x->rows &&
	       x->row < y->row + y->rows;
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

void window_destroy(struct window *window)
{
	struct window *previous = NULL, *wp;

	if (!window)
		return;
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
	display_erase(display, 0, 0, display_rows, display_columns,
		      0xff, 0xff /*default color*/);
	while (window_list && window_list->view != view)
		window_destroy(window_list);
	while (window_list && window_list->next)
		window_destroy(window_list->next);
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

struct window *window_below(struct view *above, struct view *view,
			    unsigned rows)
{
	struct window *window, *old;
	if (view->window)
		return view->window;
	if (!above && active_window)
		above = active_window->view;
	if (!above || !(old = above->window) || old->rows < rows*2)
		return window_raise(view);
	window = window_create(view, above);
	window->column = old->column;
	window->columns = old->columns;
	window->row = old->row + (old->rows -= (window->rows = rows));
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

struct view *window_current_view(void)
{
	if (!active_window) {
		if (!text_list)
			view_help();
		window_raise(text_list->views);
	}
	return active_window->view;
}

void windows_end(void)
{
	while (window_list)
		window_destroy(window_list);
	if (display) {
		display_end(display);
		display = NULL;
	}
}

static unsigned count_rows(struct window *window, unsigned start, unsigned end)
{
	unsigned rows = 0, bytes;

	for (rows = 0; start < end; rows++, start += bytes)
		if (!(bytes = find_row_bytes(window->view, start, 0, window->columns)))
			break;
	return rows;
}

static unsigned find_row_start(struct window *window, unsigned position,
			       unsigned start)
{
	unsigned bytes;
	if (position && position >= window->view->bytes)
		position--;
	while ((bytes = find_row_bytes(window->view, start, 0, window->columns))) {
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
			if (!(bytes = find_row_bytes(view, at, 0, window->columns)))
				break;
		if (above + below == window->rows || !start)
			goto done;
	}

	for (start = cursorrow, above = 0;
	     (end = start) && above < window->rows >> 1;
	     above += count_rows(window, start, end))
		start = find_line_start(view, start-1);
	for (; above >= window->rows >> 1; above--)
		start += find_row_bytes(view, start, 0, window->columns);

	for (below = 1, at = cursorrow;
	     above + below < window->rows;
	     below++, at += bytes)
		if (!(bytes = find_row_bytes(view, at, 0, window->columns)))
			break;
	for (; (end = start) && above + below < window->rows;
	     above += count_rows(window, start, end))
		start = find_line_start(view, start-1);
	for (; above + below > window->rows; above--)
		start += find_row_bytes(view, start, 0, window->columns);

done:	window->cursor_column = find_column(&above, view, cursorrow,
					    cursor, 0, window->columns);
	window->cursor_row = above;
	new_start(view, start);
	return start;
}

static int lame_space(struct view *view, unsigned offset, unsigned look)
{
	int all_spaces_lame = look > 1 && !no_tabs;
	while (look--) {
		int ch = view_char(view, offset, &offset);
		if (ch < 0 || ch == '\n' || ch == '\t')
			return 1;
		if (ch != ' ')
			return 0;
	}
	return all_spaces_lame;
}

static int lame_tab(struct view *view, unsigned offset)
{
	int ch;
	if (no_tabs)
		return 1;
	for (; (ch = view_char(view, offset, &offset)) >= 0 && ch != '\n';)
		if (ch != ' ' && ch != '\t')
			return 0;
	return 1;
}

static unsigned paintch(struct window *window, int ch, unsigned row,
			unsigned column,
			unsigned at, unsigned cursor, unsigned mark,
			unsigned *brackets)
{
	rgba_t fgrgba = window->fgrgba, bgrgba = window->bgrgba;
	unsigned tabstop = window->view->text->tabstop;

	if (ch == '\n')
		return column;

	if (mark != UNSET &&
	    (at >= cursor && at < mark ||
	     at >= mark && at < cursor)) {
		bgrgba = 0x00ffff00;
		fgrgba = 0xff000000;
	} else if (at == cursor) {
		if (window->view->text->flags & TEXT_RDONLY)
			fgrgba = 0xff000000;
		else if (at == mark)
			fgrgba = 0xff00ff00;
		else if (text_is_dirty(window->view->text))
			fgrgba = 0x00ff0000;
	}

	if (ch == '\t') {
		rgba_t bg = lame_tab(window->view, at+1) ? 0xff00ff00 : bgrgba;
		do {
			display_put(display, window->row + row,
				    window->column + column++, ' ', fgrgba, bg);
			bg = bgrgba;
		} while (column % tabstop);
		return column;
	}

	if (ch < ' ' || ch == 0x7f ||
	    ch >= FOLD_START && ch < FOLD_END) {
		bgrgba = 0xff000000;
		fgrgba = 0xffffff00;
		display_put(display, window->row + row, window->column + column++,
			    ch >= FOLD_START ? '<' : '^', fgrgba, bgrgba);
		if (ch >= FOLD_START)
			ch = '>';
		else if (ch == 0x7f)
			ch = '?';
		else
			ch += '@';
	} else if (ch == ' ' &&
		   lame_space(window->view, at + 1,
			      tabstop-1 - column % tabstop))
		bgrgba = 0xff00ff00;
	else if ((ch == '(' || ch == '[' || ch == '{') &&
		 (*brackets)++ & 1 ||
		 (ch == ')' || ch == ']' || ch == '}') &&
		 --*brackets & 1)
		fgrgba = 0x0000ff00;
	else if (ch < 0)
		ch = ' ', bgrgba = 0xff00ff00;

	display_put(display, window->row + row, window->column + column++,
		    ch, fgrgba, bgrgba);
	return column;
}

static int needs_repainting(struct window *window)
{
	struct view *view = window->view;
	unsigned cursor = locus_get(view, CURSOR);
	unsigned mark = locus_get(view, MARK);

	if (mark == UNSET)
		mark = cursor;
	return	view->text->dirties != window->last_dirties ||
		window->last_fgrgba != window->fgrgba ||
		window->last_bgrgba != window->bgrgba ||
		window->last_cursor != cursor ||
		window->last_mark != mark;
}

static void repainted(struct window *window, unsigned cursor, unsigned mark)
{
	window->last_dirties = window->view->text->dirties;
	window->last_fgrgba = window->fgrgba;
	window->last_bgrgba = window->bgrgba;
	window->last_cursor = cursor;
	window->last_mark = mark;
}

static void paint(struct window *window)
{
	unsigned at, row, next, column;
	struct view *view = window->view;
	unsigned cursor = locus_get(view, CURSOR);
	unsigned mark = locus_get(view, MARK);
	unsigned brackets = 1;

	repainted(window, cursor, mark);

	at = focus(window);
	for (row = 0; row < window->rows; row++) {

		unsigned limit = at + find_row_bytes(view, at, 0, window->columns);
		for (column = 0; at < limit; at = next)
			column = paintch(window, view_char(view, at, &next),
					 row, column, at, cursor, mark,
					 &brackets);

		display_erase(display, window->row + row,
			      window->column + column, 1,
			      window->columns - column,
			      window->fgrgba, window->bgrgba);
	}
}

void window_hint_deleting(struct window *window, unsigned offset, unsigned bytes)
{
	struct view *view = window->view;
	unsigned at = screen_start(view);
	unsigned row = 0, column;
	unsigned lines = 0, end_column;

	if (focus(window) != at || offset + bytes <= at)
		return;
	if (offset < at) {
		bytes -= at - offset;
		offset = at;
	}
	if (!bytes || view_char(view, offset, NULL) >= FOLD_START)
		return;
	column = find_column(&row, view, at, offset, 0, window->columns);
	if (row >= window->rows)
		return;
	end_column = find_column(&lines, view, offset,
				 offset + bytes, column, window->columns);
	if (!lines) {
		unsigned old = find_row_bytes(view, offset,
					      column, window->columns);
		unsigned new = find_row_bytes(view, offset + bytes,
					      end_column, window->columns);
		if (new + bytes == old) {
			display_delete_chars(display, window->row + row,
					     window->column + column,
					     end_column - column,
					     window->columns, window->fgrgba,
					     window->bgrgba);
			return;
		}
		lines++;
	}
	if (row + lines >= window->rows) {
		lines = window->rows - row;
		end_column = 0;
	}
	display_delete_lines(display, window->row + row, window->column,
			     lines, window->rows, window->columns,
			     window->fgrgba, window->bgrgba);
}

void window_hint_inserted(struct window *window, unsigned offset, unsigned bytes)
{
	struct view *view = window->view;
	unsigned at = screen_start(view);
	unsigned row = 0, column;
	unsigned lines = 0, end_column;

	if (focus(window) != at || offset + bytes <= at)
		return;
	if (offset < at) {
		bytes -= at - offset;
		offset = at;
	}
	if (!bytes || view_char(view, offset, NULL) >= FOLD_START)
		return;
	column = find_column(&row, view, at, offset, 0, window->columns);
	if (row >= window->rows)
		return;
	end_column = find_column(&lines, view, offset,
				 offset + bytes, column, window->columns);
	if (!lines) {
		unsigned old = find_row_bytes(view, offset + bytes,
					      end_column, window->columns);
		unsigned new = find_row_bytes(view, offset,
					      column, window->columns);
		if (new == old + bytes) {
			display_insert_spaces(display, window->row + row,
					      window->column + column,
					      end_column - column,
					      window->columns, window->fgrgba,
					      window->bgrgba);
			return;
		}
		lines++;
	}
	if (row + lines >= window->rows) {
		lines = window->rows - row;
		end_column = 0;
	}
	display_insert_lines(display, window->row + row, window->column,
			     lines, window->rows, window->columns,
			     window->fgrgba, window->bgrgba);
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
		start += find_row_bytes(view, start, 0, window->columns);
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
			start += find_row_bytes(view, start, 0, window->columns);
		new_start(view, start);
		for (row = 0; row < window->rows-1; row++, start += bytes)
			if (!(bytes = find_row_bytes(view, start, 0,
						     window->columns)))
				break;
		display_insert_lines(display, window->row, window->column,
				     1, window->rows, window->columns,
				     window->fgrgba, window->bgrgba);
	}
	locus_set(view, CURSOR, start);
}

void window_page_down(struct view *view)
{
	struct window *window = view->window;
	unsigned start = screen_start(view);
	unsigned row, bytes;

	for (row = 0; row + OVERLAP < window->rows; row++, start += bytes)
		if (!(bytes = find_row_bytes(view, start, 0, window->columns)))
			break;
	display_delete_lines(display, window->row, window->column, 1,
			     window->rows, window->columns,
			     window->fgrgba, window->bgrgba);
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

static void window_colors(void)
{
	int j;
	struct window *window, *w;

	static rgba_t colors[][2] = {
		{ 0x000000ff, ~0 },
		{ 0x00000000, 0x7f7f7f00 },
		{ 0x0000ff00, 0x7f7f0000 },
		{ 0xff00ff00, 0x007f0000 },
		{ 0xffff0000, 0x00007f00 },
		{ 0x00000000, 0xffffff00 },
		{ 0x0000ff00, 0xffff0000 },
		{ 0xff00ff00, 0x00ff0000 },
		{ 0xffff0000, 0x0000ff00 },
		{ }
	};

	for (window = window_list; window; window = window->next)
		window->bgrgba = 0;
	active_window->fgrgba = 0xff;
	active_window->bgrgba = ~0;
	for (window = window_list; window; window = window->next) {
		if (window->bgrgba)
			continue;
		for (j = 0; colors[j][1]; j++) {
			for (w = window_list; w; w = w->next)
				if (w != window &&
				    w->bgrgba == colors[j][1] &&
				    adjacent(window, w))
					break;
			if (!w)
				break;
		}
		window->fgrgba = colors[j][0];
		window->bgrgba = colors[j][1];
	}
}

static void repaint(void)
{
	struct window *window;

	window_colors();
	for (window = window_list; window; window = window->next)
		if (needs_repainting(window))
			paint(window);
	display_cursor(display,
		       active_window->row + active_window->cursor_row,
		       active_window->column + active_window->cursor_column);
}

int window_getch(void)
{
	int block = 0;
	for (;;) {
		int ch = display_getch(display, block);
		if (ch == DISPLAY_CHANGED)
			window_raise(window_current_view());
		else if (ch != DISPLAY_NONE)
			return ch;
		repaint();
		block = 1;
	}
}
