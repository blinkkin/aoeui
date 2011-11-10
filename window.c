/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#include "all.h"

/*
 *	A "window" is a presentation of a view on part or all of
 *	the display surface.  One window is active, meaning that
 *	it directs keyboard input to its view's command handler.
 */

struct window {
	struct view *view;
	locus_t start;
	int row, column;
	int rows, columns;
	int cursor_row, cursor_column;
	rgba_t fgrgba, bgrgba;
	unsigned last_dirties;
	Boolean_t repaint;
	position_t last_cursor, last_mark;
	struct mode *last_mode;
	struct window *next;
};

static struct window *window_list;
static struct window *active_window;
static struct display *display;
static int display_rows, display_columns;
static Boolean_t titles = TRUE;

static void title(struct window *window)
{
	char buff[128];
	struct view *view;
	position_t cursor;

	if (!titles)
		return;
	if (window != active_window)
		return;
	if (!window) {
		titles = display_title(display, NULL);
		return;
	}
	view = window->view;
	snprintf(buff, sizeof buff, "%s%s", view->name,
		 view->text->flags & TEXT_CREATED ? " (new)" :
		 view->text->flags & TEXT_RDONLY ? " (read-only)" :
		 view->text->preserved !=
		    view->text->dirties ? " (unsaved)" : "");
	cursor = locus_get(view, CURSOR);
	if (cursor < 65536) {
		int line = current_line_number(view, cursor);
		int len = strlen(buff);
		snprintf(buff + len, sizeof buff - len - 1,
			 " [%d]", line);
	}
	titles = display_title(display, buff);
}

static struct window *activate(struct window *window)
{
	if (active_window)
		active_window->repaint = TRUE;
	active_window = window;
	title(window);
	window->repaint = TRUE;
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

static Boolean_t stacked(struct window *up, struct window *down)
{
	return	up && down &&
		up->column == down->column &&
		up->columns == down->columns &&
		up->row + up->rows == down->row;
}

static Boolean_t beside(struct window *left, struct window *right)
{
	return	left && right &&
		left->row == right->row &&
		left->rows == right->rows &&
		left->column + left->columns == right->column;
}

static Boolean_t adjacent(struct window *x, struct window *y)
{
	if (x->column + x->columns < y->column ||
	    y->column + y->columns < x->column ||
	    x->row + x->rows < y->row ||
	    y->row + y->rows < x->row)
		return FALSE;
	if (y->column != x->column + x->columns &&
	    x->column != y->column + y->columns)
		return TRUE;
	return y->row < x->row + x->rows &&
	       x->row < y->row + y->rows;
}

static Boolean_t window_expand_next(struct window *window)
{
	struct window *next = window->next;
	if (!next)
		return FALSE;
	if (stacked(window, next)) {
		next->row = window->row;
		next->rows += window->rows;
		next->repaint = TRUE;
		return TRUE;
	}
	if (beside(window, next)) {
		next->column = window->column;
		next->columns += window->columns;
		next->repaint = TRUE;
		return TRUE;
	}
	return FALSE;
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
		     previous->rows <= next->rows)) {
			previous->rows += window->rows;
			previous->repaint = TRUE;
		} else if (beside(previous, window) &&
			 (!beside(window, next) ||
			  previous->columns <= next->columns)) {
			previous->columns += window->columns;
			previous->repaint = TRUE;
		} else if (!window_expand_next(window))
			window_raise(previous->view);
	} else if ((window_list = window->next) &&
		   !window_expand_next(window))
		window_raise(window_list->view);

	if (window->view) {
		locus_destroy(window->view, window->start);
		window->view->window = NULL;
	}
	if (window == active_window) {
		active_window = NULL;
		wp = window_list;
		if (previous) {
			wp = previous;
			if (wp->next)
				wp = wp->next;
		}
		if (wp)
			activate(wp);
	}
	RELEASE(window);
}

struct window *window_raise(struct view *view)
{
	if (!display)
		display = display_init();
	display_get_geometry(display, &display_rows, &display_columns);
	display_erase(display, 0, 0, display_rows, display_columns);
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
			(old->columns -= (window->columns = old->columns >> 1));
	} else {
		window->column = old->column;
		window->columns = old->columns;
		window->row = old->row +
			(old->rows -= (window->rows = old->rows >> 1));
	}
	old->repaint = TRUE;
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
	return window;
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

void windows_end_display(void)
{
	struct window *window;
	if (display) {
		display_end(display);
		display = NULL;
	}
	for (window = window_list; window; window = window->next)
		window->repaint = TRUE;
}

void windows_end(void)
{
	while (window_list)
		window_destroy(window_list);
	windows_end_display();
}

static unsigned count_rows(struct window *window, position_t start,
			   position_t end)
{
	unsigned rows = 0, bytes, max_rows = window->rows + 1;

	for (rows = 0; start < end && rows < max_rows; rows++, start += bytes)
		if (!(bytes = find_row_bytes(window->view, start, 0,
					     window->columns)))
			break;
	return rows;
}

static position_t find_row_start(struct window *window, position_t position,
				 position_t start)
{
	size_t bytes;
	if (position && position >= window->view->bytes)
		position--;
	while ((bytes = find_row_bytes(window->view, start, 0, window->columns))) {
		if (start + bytes > position)
			break;
		start += bytes;
	}
	return start;
}

static void new_start(struct view *view, position_t start)
{
	if (start)
		view_char_prior(view, start, &start);
	else
		start = UNSET;
	locus_set(view, view->window->start, start);
	view->window->last_dirties = ~0;
}

static position_t screen_start(struct view *view)
{
	position_t start = locus_get(view, view->window->start);
	if (start == UNSET)
		start = 0;
	else
		view_char(view, start, &start);
	return start;
}

static position_t focus(struct window *window)
{
	struct view *view = window->view;
	position_t cursor = locus_get(view, CURSOR);
	position_t cursorrow = find_row_start(window, cursor,
					      find_line_start(view, cursor));
	position_t start = screen_start(view);
	position_t end, at;
	unsigned above = 0, below;
	size_t bytes;

	start = find_row_start(window, start, find_line_start(view, start));

	/* Scroll by single lines when cursor is just one row out of view. */
	if (cursorrow < start &&
	    start == cursorrow + find_row_bytes(view, cursorrow,
						0, window->columns)) {
		start = cursorrow;
		goto done;
	}
	if (cursorrow >= start &&
	    (above = count_rows(window, start, cursorrow)) == window->rows) {
		start += find_row_bytes(view, start, 0, window->columns);
		above--;
		goto done;
	}

	if (cursorrow >= start && above < window->rows) {
		for (below = 1, at = cursorrow;
		     above + below < window->rows;
		     below++, at += bytes)
			if (!(bytes = find_row_bytes(view, at, 0,
						     window->columns)))
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
					    cursor, 0);
	window->cursor_row = above;
	new_start(view, start);
	return start;
}

static Boolean_t lame_space(struct view *view, position_t start,
			    unsigned next_tab)
{
	unsigned distance = 0;
	position_t offset = start;
	Unicode_t ch;
	if (view->shell_std_in >= 0 || view->text->flags & TEXT_EDITOR)
		return FALSE;
	do {
		distance++;
		ch = view_char(view, offset, &offset);
		if (!IS_UNICODE(ch) || ch == '\n' || ch == '\t')
			return TRUE;
	} while (ch == ' ');
	return distance > next_tab &&
	       !(view->text->flags & TEXT_NO_TABS) &&
	       (next_tab ||
		view_char_prior(view, start-1, NULL) == ' ');
}

static Boolean_t lame_tab(struct view *view, position_t offset)
{
	Unicode_t ch;
	if (view->shell_std_in >= 0 || view->text->flags & TEXT_EDITOR)
		return FALSE;
	if (view->text->flags & TEXT_NO_TABS)
		return TRUE;
	for (;
	     IS_UNICODE(ch = view_char(view, offset, &offset)) &&
	     ch != '\n'; )
		if (ch != ' ' && ch != '\t')
			return FALSE;
	return TRUE;
}

static int paintch(struct window *window, Unicode_t ch, int row, int column,
		   position_t at, position_t cursor, position_t mark,
		   unsigned *brackets, rgba_t fgrgba)
{
	rgba_t bgrgba = window->bgrgba;
	unsigned tabstop = window->view->text->tabstop;
	const char *brack = window->view->text->brackets;

	if (ch == '\n')
		return column;

	if (mark != UNSET &&
	    (at >= cursor && at < mark ||
	     at >= mark && at < cursor)) {
		bgrgba = window->view->mode->selection_bgrgba;
		fgrgba = SELECTION_FGRGBA;
	} else if (at == cursor) {
		rgba_t rgba = DEFAULT_CURSORRGBA;
		if (mark != UNSET)
			rgba = SELECTING_RGBA;
		else if (window->view->text->flags & TEXT_RDONLY)
			rgba = RDONLY_RGBA;
		else if (text_is_dirty(window->view->text))
			rgba = DIRTY_RGBA;
		if (!display_cursor_color(display, rgba))
			fgrgba = rgba;
	}

	if (ch == '\t') {
		rgba_t bg = lame_tab(window->view, at+1) ?
				LAMESPACE_BGRGBA : bgrgba;
		do {
			display_put(display, window->row + row,
				    window->column + column++, ' ', fgrgba, bg);
			bg = bgrgba;
		} while (column % tabstop);
		return column;
	}

	if (ch < ' ' || ch == 0x7f || IS_FOLDED(ch)) {
		fgrgba = FOLDED_FGRGBA;
		bgrgba = FOLDED_BGRGBA;
		display_put(display,
			    window->row + row, window->column + column++,
			    IS_FOLDED(ch) ? '<' : '^', fgrgba, bgrgba);
		if (IS_FOLDED(ch))
			ch = '>';
		else if (ch == 0x7f)
			ch = '?';
		else
			ch += '@';
	} else if (ch == ' ') {
		if (bgrgba == window->bgrgba &&
		    lame_space(window->view, at + 1,
			       tabstop-1 - column % tabstop))
			bgrgba = LAMESPACE_BGRGBA;
	} else if (!IS_UNICODE(ch))
		ch = ' ', bgrgba = BADCHAR_BGRGBA;
	else if (brackets && brack)
		for (; *brack; brack += 2)
			if (ch == brack[0] && (*brackets)++ & 1 ||
			    ch == brack[1] && --*brackets & 1) {
				fgrgba = BLUE_RGBA;
				break;
			}

	display_put(display, window->row + row, window->column + column++,
		    ch, fgrgba, bgrgba);
	return column;
}

static Boolean_t needs_repainting(struct window *window)
{
	struct view *view = window->view;
	position_t cursor = locus_get(view, CURSOR);
	position_t mark = locus_get(view, MARK);

	return	window->repaint ||
		view->text->dirties != window->last_dirties ||
		window->last_cursor != cursor ||
		window->last_mark != mark ||
		window->last_mode != view->mode;
}

static void repainted(struct window *window, position_t cursor, position_t mark)
{
	window->repaint = FALSE;
	window->last_dirties = window->view->text->dirties;
	window->last_cursor = cursor;
	window->last_mark = mark;
	window->last_mode = window->view->mode;
}

static void paint(struct window *window)
{
	sposition_t at;
	int row, column;
	struct view *view = window->view;
	position_t cursor = locus_get(view, CURSOR);
	position_t mark = locus_get(view, MARK);
	sposition_t comment_end = -1;
	sposition_t string_end = -1;
	unsigned brackets = 1;
	Boolean_t keywords = !no_keywords &&
			     window == active_window &&
			     view->text->keywords;

	title(window);

	at = focus(window);
	if (keywords && view->text->comment_start) {
		sposition_t start = view->text->comment_start(view, at);
		if (start >= 0)
			comment_end = view->text->comment_end(view, start);
	}

	for (row = 0; row < window->rows; row++) {

		position_t limit = at + find_row_bytes(view, at, 0,
						       window->columns);
		rgba_t fgrgba = window->fgrgba;
		Boolean_t look_for_keyword = keywords;
		position_t next;
		unsigned *brackets_ptr = &brackets;

		for (column = 0; at < limit; at = next) {

			Unicode_t ch = view_char(view, at, &next);

			if ((ch == '/' || ch == '-' || ch == '{') &&
			    at > comment_end &&
			    at > string_end &&
			    keywords &&
			    view->text->comment_end)
				comment_end = view->text->comment_end(view, at);
			if ((ch == '"' || ch == '\'') &&
			    at > comment_end &&
			    at > string_end &&
			    keywords &&
			    view->text->string_end)
				string_end = view->text->string_end(view, at);

			if (at <= comment_end) {
				fgrgba = COMMENT_FGRGBA;
				brackets_ptr = NULL;
			} else if (at <= string_end) {
				fgrgba = STRING_FGRGBA;
				brackets_ptr = NULL;
			} else if (!is_idch(ch) && ch != '#') {
				fgrgba = window->fgrgba;
				look_for_keyword = keywords;
			} else if (look_for_keyword && is_keyword(view, at))
				fgrgba = KEYWORD_FGRGBA;
			else
				look_for_keyword = FALSE;

			column = paintch(window, ch, row, column, at,
					 cursor, mark, brackets_ptr,
					 fgrgba);

			if (at == comment_end || at == string_end) {
				fgrgba = window->fgrgba;
				brackets_ptr = &brackets;
				look_for_keyword = keywords;
			}
		}

		display_erase(display, window->row + row,
			      window->column + column,
			      1, window->columns - column - 1);
		while (column < window->columns)
			display_put(display, window->row + row,
				    window->column + column++,
				    ' ', DEFAULT_FGRGBA, window->bgrgba);
	}

	repainted(window, cursor, mark);
}

void window_hint_deleting(struct window *window, position_t offset,
			  size_t bytes)
{
	struct view *view = window->view;
	position_t at = screen_start(view);
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
	column = find_column(&row, view, at, offset, 0);
	if (row >= window->rows)
		return;
	end_column = find_column(&lines, view, offset,
				 offset + bytes, column);
	if (!lines) {
		unsigned old = find_row_bytes(view, offset,
					      column, window->columns);
		unsigned new = find_row_bytes(view, offset + bytes,
					      end_column, window->columns);
		if (new + bytes == old) {
			display_delete_chars(display, window->row + row,
					     window->column + column,
					     end_column - column,
					     window->columns - column);
			return;
		}
		lines++;
	}
	if (row + lines >= window->rows) {
		lines = window->rows - row;
		end_column = 0;
	}
	display_delete_lines(display, window->row + row, window->column,
			     lines, window->rows - row, window->columns);
}

void window_hint_inserted(struct window *window, position_t offset,
			  size_t bytes)
{
	struct view *view = window->view;
	position_t at = screen_start(view);
	position_t column, end_column;
	unsigned row = 0, lines = 0;

	if (focus(window) != at || offset + bytes <= at)
		return;
	if (offset < at) {
		bytes -= at - offset;
		offset = at;
	}
	if (!bytes || view_char(view, offset, NULL) >= FOLD_START)
		return;
	column = find_column(&row, view, at, offset, 0);
	if (row >= window->rows)
		return;
	end_column = find_column(&lines, view, offset,
				 offset + bytes, column);
	if (!lines) {
		size_t old = find_row_bytes(view, offset + bytes,
					    end_column, window->columns);
		size_t new = find_row_bytes(view, offset,
					    column, window->columns);
		if (new == old + bytes) {
			display_insert_spaces(display, window->row + row,
					      window->column + column,
					      end_column - column,
					      window->columns - column);
			return;
		}
		lines++;
	}
	if (row + lines >= window->rows) {
		lines = window->rows - row;
		end_column = 0;
	}
	display_insert_lines(display, window->row + row, window->column,
			     lines, window->rows - row, window->columns);
}

void window_next(struct view *view)
{
	if (view && view->window)
		activate(view->window->next ? view->window->next : window_list);
}

void window_index(int num)
{
	struct window *window = window_list;
	while (--num > 0 && window->next)
		window = window->next;
	activate(window);
}

struct window *window_recenter(struct view *view)
{
	struct window *window = view->window;
	position_t cursor = locus_get(view, CURSOR);
	position_t start, end;
	int row, rows, columns;

	if (!display)
		display = display_init();
	display_get_geometry(display, &rows, &columns);
	if (rows != display_rows ||
	    columns != display_columns ||
	    !(window = view->window))
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

static int page_overlap(struct window *window)
{
	static int overlap_percent;
	int o;

	if (!overlap_percent) {
		const char *p = getenv("AOEUI_OVERLAP");
		overlap_percent = 1;
		if (p) {
			overlap_percent = atoi(p);
			if (overlap_percent < 0)
				overlap_percent = 1;
			else if (overlap_percent > 100)
				overlap_percent = 100;
		}
	}
	o = overlap_percent * window->rows / 100;
	if (o >= window->rows)
		o = window->rows - 1;
	if (o < 1)
		o = 1;
	return o;
}

void window_page_up(struct view *view)
{
	struct window *window = view->window;
	position_t start = screen_start(view);
	position_t end;
	int row;
	size_t bytes;

	if (start) {
		int overlap = page_overlap(window);
		for (row = 0;
		     (end = start) && row + overlap < window->rows;
		     row += count_rows(window, start, end))
			start = find_line_start(view, start-1);
		while (row-- + overlap > window->rows)
			start += find_row_bytes(view, start,
						0, window->columns);
		new_start(view, start);
		for (row = 0; row < window->rows-1; row++, start += bytes)
			if (!(bytes = find_row_bytes(view, start, 0,
						     window->columns)))
				break;
		display_insert_lines(display, window->row, window->column,
				     window->rows - overlap,
				     window->rows, window->columns);
	}
	locus_set(view, CURSOR, start);
}

void window_page_down(struct view *view)
{
	struct window *window = view->window;
	position_t start = screen_start(view);
	int row;
	size_t bytes;
	int overlap = page_overlap(window);

	for (row = 0; row + overlap < window->rows; row++, start += bytes)
		if (!(bytes = find_row_bytes(view, start, 0, window->columns)))
			break;
	display_delete_lines(display, window->row, window->column,
			     window->rows - overlap,
			     window->rows, window->columns);
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
		{ BLACK_RGBA, PALE_RGBA(WHITE_RGBA) },
		{ BLUE_RGBA, YELLOW_RGBA },
		{ BLUE_RGBA, PALE_RGBA(YELLOW_RGBA) },
		{ GREEN_RGBA, MAGENTA_RGBA },
		{ GREEN_RGBA, PALE_RGBA(MAGENTA_RGBA) },
		{ RED_RGBA, PALE_RGBA(CYAN_RGBA) },
		{ RED_RGBA, CYAN_RGBA },
		{ }
	};

	if (active_window) {
		if (active_window->fgrgba != DEFAULT_FGRGBA)
			active_window->repaint = TRUE;
		active_window->fgrgba = DEFAULT_FGRGBA;
		active_window->bgrgba = DEFAULT_BGRGBA;
	}
	for (window = window_list; window; window = window->next) {
		if (window == active_window)
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
		if (window->bgrgba != colors[j][1]) {
			window->repaint = TRUE;
			window->fgrgba = colors[j][0];
			window->bgrgba = colors[j][1];
		}
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

Unicode_t window_getch(void)
{
	Boolean_t block = FALSE;
	for (;;) {
		Unicode_t ch = display_getch(display, block);
		if (ch == ERROR_CHANGED)
			window_raise(window_current_view());
		else if (ch != ERROR_EMPTY)
			return ch;
		repaint();
		block = TRUE;
	}
}

unsigned window_columns(struct window *window)
{
	return window ? window->columns : 80;
}
