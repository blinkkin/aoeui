/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#ifndef WINDOW_H
#define WINDOW_H

/* Windows */

struct window *window_raise(struct view *);
struct window *window_activate(struct view *);
struct window *window_after(struct view *, struct view *, int vertical);
struct window *window_below(struct view *, struct view *, unsigned rows);
struct window *window_replace(struct view *, struct view *);
void window_destroy(struct window *);
void window_next(struct view *);
void window_index(int);
void window_hint_deleting(struct window *, position_t, size_t);
void window_hint_inserted(struct window *, position_t, size_t);
struct window *window_recenter(struct view *);
void window_page_up(struct view *);
void window_page_down(struct view *);
void window_beep(struct view *);
Unicode_t window_getch(void);
struct view *window_current_view(void);
unsigned window_columns(struct window *);
void windows_end(void);
void windows_end_display(void);

#endif
